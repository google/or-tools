// Copyright 2010-2018 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ortools/sat/integer_search.h"

#include <cmath>
#include <functional>
#include <vector>

#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/pseudo_costs.h"
#include "ortools/sat/rins.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

LiteralIndex BranchDown(IntegerVariable var, IntegerValue value, Model* model) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* trail = model->GetOrCreate<Trail>();
  const Literal le = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::LowerOrEqual(var, value));
  DCHECK(!trail->Assignment().VariableIsAssigned(le.Variable()));
  return le.Index();
}

LiteralIndex BranchUp(IntegerVariable var, IntegerValue value, Model* model) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* trail = model->GetOrCreate<Trail>();
  const Literal ge = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, value));
  DCHECK(!trail->Assignment().VariableIsAssigned(ge.Variable()));
  return ge.Index();
}

LiteralIndex AtMinValue(IntegerVariable var, IntegerTrail* integer_trail,
                        IntegerEncoder* integer_encoder) {
  DCHECK(!integer_trail->IsCurrentlyIgnored(var));
  const IntegerValue lb = integer_trail->LowerBound(var);
  DCHECK_LE(lb, integer_trail->UpperBound(var));
  if (lb == integer_trail->UpperBound(var)) return kNoLiteralIndex;

  const Literal result = integer_encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::LowerOrEqual(var, lb));
  return result.Index();
}

LiteralIndex GreaterOrEqualToMiddleValue(IntegerVariable var, Model* model) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  const IntegerValue var_lb = integer_trail->LowerBound(var);
  const IntegerValue var_ub = integer_trail->UpperBound(var);
  CHECK_LT(var_lb, var_ub);

  const IntegerValue chosen_value =
      var_lb + std::max(IntegerValue(1), (var_ub - var_lb) / IntegerValue(2));
  return BranchUp(var, chosen_value, model);
}

LiteralIndex SplitAroundGivenValue(IntegerVariable positive_var,
                                   IntegerValue value, Model* model) {
  DCHECK(VariableIsPositive(positive_var));
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();

  const IntegerValue lb = integer_trail->LowerBound(positive_var);
  const IntegerValue ub = integer_trail->UpperBound(positive_var);

  const absl::flat_hash_set<IntegerVariable>& variables =
      model->GetOrCreate<ObjectiveDefinition>()->objective_impacting_variables;

  // Heuristic: Prefer the objective direction first. Reference: Conflict-Driven
  // Heuristics for Mixed Integer Programming (2019) by Jakob Witzig and Ambros
  // Gleixner.
  // NOTE: The value might be out of bounds. In that case we return
  // kNoLiteralIndex.
  const bool branch_down_feasible = value >= lb && value < ub;
  const bool branch_up_feasible = value > lb && value <= ub;
  if (variables.contains(positive_var) && branch_down_feasible) {
    return BranchDown(positive_var, value, model);
  } else if (variables.contains(NegationOf(positive_var)) &&
             branch_up_feasible) {
    return BranchUp(positive_var, value, model);
  } else if (branch_down_feasible) {
    return BranchDown(positive_var, value, model);
  } else if (branch_up_feasible) {
    return BranchUp(positive_var, value, model);
  }
  return kNoLiteralIndex;
}

LiteralIndex SplitAroundLpValue(IntegerVariable var, Model* model) {
  auto* parameters = model->GetOrCreate<SatParameters>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* lp_dispatcher = model->GetOrCreate<LinearProgrammingDispatcher>();
  DCHECK(!integer_trail->IsCurrentlyIgnored(var));

  const IntegerVariable positive_var = PositiveVariable(var);
  const LinearProgrammingConstraint* lp =
      gtl::FindWithDefault(*lp_dispatcher, positive_var, nullptr);

  // We only use this if the sub-lp has a solution, and depending on the value
  // of exploit_all_lp_solution() if it is a pure-integer solution.
  if (lp == nullptr || !lp->HasSolution()) return kNoLiteralIndex;
  if (!parameters->exploit_all_lp_solution() && !lp->SolutionIsInteger()) {
    return kNoLiteralIndex;
  }

  const IntegerValue value = IntegerValue(
      static_cast<int64>(std::round(lp->GetSolutionValue(positive_var))));

  // Because our lp solution might be from higher up in the tree, it
  // is possible that value is now outside the domain of positive_var.
  // In this case, this function will return kNoLiteralIndex.
  return SplitAroundGivenValue(positive_var, value, model);
}

LiteralIndex SplitDomainUsingBestSolutionValue(IntegerVariable var,
                                               Model* model) {
  SolutionDetails* solution_details = model->GetOrCreate<SolutionDetails>();
  if (solution_details->solution_count == 0) return kNoLiteralIndex;

  const IntegerVariable positive_var = PositiveVariable(var);

  if (var >= solution_details->best_solution.size()) {
    return kNoLiteralIndex;
  }

  VLOG(2) << "Using last solution value for branching";
  const IntegerValue value = solution_details->best_solution[var];
  return SplitAroundGivenValue(positive_var, value, model);
}

// TODO(user): the complexity caused by the linear scan in this heuristic and
// the one below is ok when search_branching is set to SAT_SEARCH because it is
// not executed often, but otherwise it is done for each search decision,
// which seems expensive. Improve.
std::function<LiteralIndex()> FirstUnassignedVarAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* integer_encoder = model->GetOrCreate<IntegerEncoder>();
  return [/*copy*/ vars, integer_trail, integer_encoder]() {
    for (const IntegerVariable var : vars) {
      // Note that there is no point trying to fix a currently ignored variable.
      if (integer_trail->IsCurrentlyIgnored(var)) continue;
      const LiteralIndex decision =
          AtMinValue(var, integer_trail, integer_encoder);
      if (decision != kNoLiteralIndex) return decision;
    }
    return kNoLiteralIndex;
  };
}

std::function<LiteralIndex()> UnassignedVarWithLowestMinAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* integer_encoder = model->GetOrCreate<IntegerEncoder>();
  return [/*copy */ vars, integer_trail, integer_encoder]() {
    IntegerVariable candidate = kNoIntegerVariable;
    IntegerValue candidate_lb;
    for (const IntegerVariable var : vars) {
      if (integer_trail->IsCurrentlyIgnored(var)) continue;
      const IntegerValue lb = integer_trail->LowerBound(var);
      if (lb < integer_trail->UpperBound(var) &&
          (candidate == kNoIntegerVariable || lb < candidate_lb)) {
        candidate = var;
        candidate_lb = lb;
      }
    }
    if (candidate == kNoIntegerVariable) return kNoLiteralIndex;
    return AtMinValue(candidate, integer_trail, integer_encoder);
  };
}

std::function<LiteralIndex()> SequentialSearch(
    std::vector<std::function<LiteralIndex()>> heuristics) {
  return [heuristics]() {
    for (const auto& h : heuristics) {
      const LiteralIndex li = h();
      if (li != kNoLiteralIndex) return li;
    }
    return kNoLiteralIndex;
  };
}

std::function<LiteralIndex()> SequentialValueSelection(
    std::vector<std::function<LiteralIndex(IntegerVariable)>>
        value_selection_heuristics,
    std::function<LiteralIndex()> var_selection_heuristic, Model* model) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  return [=]() {
    // Get the current decision.
    const LiteralIndex current_decision = var_selection_heuristic();
    if (current_decision == kNoLiteralIndex) return kNoLiteralIndex;

    // Decode the decision and get the variable.
    for (const IntegerLiteral l :
         encoder->GetAllIntegerLiterals(Literal(current_decision))) {
      if (integer_trail->IsCurrentlyIgnored(l.var)) continue;

      // Sequentially try the value selection heuristics.
      for (const auto& value_heuristic : value_selection_heuristics) {
        const LiteralIndex decision = value_heuristic(l.var);
        if (decision != kNoLiteralIndex) {
          return decision;
        }
      }
    }

    VLOG(2) << "Value selection: using default decision.";
    return current_decision;
  };
}

// If a variable appear in the objective, branch on its best objective value.
LiteralIndex ChooseBestObjectiveValue(IntegerVariable var, Model* model) {
  const auto& variables =
      model->GetOrCreate<ObjectiveDefinition>()->objective_impacting_variables;
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  if (variables.contains(var)) {
    return AtMinValue(var, integer_trail, encoder);
  } else if (variables.contains(NegationOf(var))) {
    return AtMinValue(NegationOf(var), integer_trail, encoder);
  }
  return kNoLiteralIndex;
}

// TODO(user): Experiment more with value selection heuristics.
std::function<LiteralIndex()> IntegerValueSelectionHeuristic(
    std::function<LiteralIndex()> var_selection_heuristic, Model* model) {
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  std::vector<std::function<LiteralIndex(IntegerVariable)>>
      value_selection_heuristics;

  // LP based value.
  //
  // Note that we only do this if a big enough percentage of the problem
  // variables appear in the LP relaxation.
  if (LinearizedPartIsLarge(model) &&
      (parameters.exploit_integer_lp_solution() ||
       parameters.exploit_all_lp_solution())) {
    VLOG(1) << "Using LP value selection heuristic.";
    value_selection_heuristics.push_back([model](IntegerVariable var) {
      return SplitAroundLpValue(PositiveVariable(var), model);
    });
  }

  // Solution based value.
  if (parameters.exploit_best_solution()) {
    VLOG(1) << "Using best solution value selection heuristic.";
    value_selection_heuristics.push_back([model](IntegerVariable var) {
      return SplitDomainUsingBestSolutionValue(var, model);
    });
  }

  // Objective based value.
  if (parameters.exploit_objective()) {
    VLOG(1) << "Using objective value selection heuristic.";
    value_selection_heuristics.push_back([model](IntegerVariable var) {
      return ChooseBestObjectiveValue(var, model);
    });
  }

  return SequentialValueSelection(value_selection_heuristics,
                                  var_selection_heuristic, model);
}

std::function<LiteralIndex()> SatSolverHeuristic(Model* model) {
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  Trail* trail = model->GetOrCreate<Trail>();
  SatDecisionPolicy* decision_policy = model->GetOrCreate<SatDecisionPolicy>();
  return [sat_solver, trail, decision_policy] {
    const bool all_assigned = trail->Index() == sat_solver->NumVariables();
    if (all_assigned) return kNoLiteralIndex;
    const Literal result = decision_policy->NextBranch();
    CHECK(!sat_solver->Assignment().LiteralIsAssigned(result));
    return result.Index();
  };
}

std::function<LiteralIndex()> PseudoCost(Model* model) {
  auto* objective = model->Get<ObjectiveDefinition>();
  const bool has_objective =
      objective != nullptr && objective->objective_var != kNoIntegerVariable;
  if (!has_objective) {
    return []() { return kNoLiteralIndex; };
  }

  PseudoCosts* pseudo_costs = model->GetOrCreate<PseudoCosts>();
  return [pseudo_costs, model]() {
    const IntegerVariable chosen_var = pseudo_costs->GetBestDecisionVar();

    if (chosen_var == kNoIntegerVariable) return kNoLiteralIndex;

    return GreaterOrEqualToMiddleValue(chosen_var, model);
  };
}

std::function<LiteralIndex()> RandomizeOnRestartHeuristic(Model* model) {
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  SatDecisionPolicy* decision_policy = model->GetOrCreate<SatDecisionPolicy>();

  // TODO(user): Add other policy and perform more experiments.
  std::function<LiteralIndex()> sat_policy = SatSolverHeuristic(model);
  std::vector<std::function<LiteralIndex()>> policies{
      sat_policy, SequentialSearch({PseudoCost(model), sat_policy})};
  // The higher weight for the sat policy is because this policy actually
  // contains a lot of variation as we randomize the sat parameters.
  // TODO(user,user): Do more experiments to find better distribution.
  std::discrete_distribution<int> var_dist{3 /*sat_policy*/, 1 /*Pseudo cost*/};

  // Value selection.
  std::vector<std::function<LiteralIndex(IntegerVariable)>>
      value_selection_heuristics;
  std::vector<int> value_selection_weight;

  // LP Based value.
  value_selection_heuristics.push_back([model](IntegerVariable var) {
    return SplitAroundLpValue(PositiveVariable(var), model);
  });
  value_selection_weight.push_back(8);

  // Solution based value.
  value_selection_heuristics.push_back([model](IntegerVariable var) {
    return SplitDomainUsingBestSolutionValue(var, model);
  });
  value_selection_weight.push_back(5);

  // Middle value.
  value_selection_heuristics.push_back([model](IntegerVariable var) {
    return GreaterOrEqualToMiddleValue(var, model);
  });
  value_selection_weight.push_back(1);

  // Min value.
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* integer_encoder = model->GetOrCreate<IntegerEncoder>();
  value_selection_heuristics.push_back(
      [integer_trail, integer_encoder](IntegerVariable var) {
        return AtMinValue(var, integer_trail, integer_encoder);
      });
  value_selection_weight.push_back(1);

  // Special case: Don't change the decision value.
  value_selection_weight.push_back(10);

  // TODO(user): These distribution values are just guessed values. They need
  // to be tuned.
  std::discrete_distribution<int> val_dist(value_selection_weight.begin(),
                                           value_selection_weight.end());

  int policy_index = 0;
  int val_policy_index = 0;
  return [=]() mutable {
    if (sat_solver->CurrentDecisionLevel() == 0) {
      auto* random = model->GetOrCreate<ModelRandomGenerator>();
      RandomizeDecisionHeuristic(random, model->GetOrCreate<SatParameters>());
      decision_policy->ResetDecisionHeuristic();

      // Select the variable selection heuristic.
      policy_index = var_dist(*(random));

      // Select the value selection heuristic.
      val_policy_index = val_dist(*(random));
    }

    // Get the current decision.
    const LiteralIndex current_decision = policies[policy_index]();
    if (current_decision == kNoLiteralIndex) return kNoLiteralIndex;

    // Special case: Don't override the decision value.
    if (val_policy_index >= value_selection_heuristics.size()) {
      return current_decision;
    }

    // Decode the decision and get the variable.
    for (const IntegerLiteral l :
         integer_encoder->GetAllIntegerLiterals(Literal(current_decision))) {
      if (integer_trail->IsCurrentlyIgnored(l.var)) continue;

      // Try the selected policy.
      const LiteralIndex new_decision =
          value_selection_heuristics[val_policy_index](l.var);
      if (new_decision != kNoLiteralIndex) {
        return new_decision;
      }
    }

    // Selected policy failed. Revert back to original decision.
    return current_decision;
  };
}

// TODO(user): Avoid the quadratic algorithm!!
std::function<LiteralIndex()> FollowHint(
    const std::vector<BooleanOrIntegerVariable>& vars,
    const std::vector<IntegerValue>& values, Model* model) {
  const Trail* trail = model->GetOrCreate<Trail>();
  const IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  return [=] {  // copy
    for (int i = 0; i < vars.size(); ++i) {
      const IntegerValue value = values[i];
      if (vars[i].bool_var != kNoBooleanVariable) {
        if (trail->Assignment().VariableIsAssigned(vars[i].bool_var)) {
          continue;
        }
        return Literal(vars[i].bool_var, value == 1).Index();
      } else {
        const IntegerVariable integer_var = vars[i].int_var;
        if (integer_trail->IsCurrentlyIgnored(integer_var)) continue;
        const IntegerValue lb = integer_trail->LowerBound(integer_var);
        const IntegerValue ub = integer_trail->UpperBound(integer_var);
        if (lb == ub) continue;

        const IntegerVariable positive_var = PositiveVariable(integer_var);
        const LiteralIndex decision =
            SplitAroundGivenValue(positive_var, value, model);
        if (decision != kNoLiteralIndex) return decision;

        // If the value is outside the current possible domain, we skip it.
        continue;
      }
    }
    return kNoLiteralIndex;
  };
}

bool LpSolutionIsExploitable(Model* model) {
  auto* lp_constraints =
      model->GetOrCreate<LinearProgrammingConstraintCollection>();
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());

  // TODO(user,user): When we have more than one LP, their set of variable
  // is always disjoint. So we could still change the polarity if the next
  // variable we branch on is part of a LP that has a solution.
  for (LinearProgrammingConstraint* lp : *lp_constraints) {
    if (!lp->HasSolution() ||
        !(parameters.exploit_all_lp_solution() || lp->SolutionIsInteger())) {
      return false;
    }
  }
  return true;
}

bool LinearizedPartIsLarge(Model* model) {
  auto* lp_constraints =
      model->GetOrCreate<LinearProgrammingConstraintCollection>();

  int num_lp_variables = 0;
  for (LinearProgrammingConstraint* lp : *lp_constraints) {
    num_lp_variables += lp->NumVariables();
  }
  const int num_integer_variables =
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value() / 2;
  return (num_integer_variables <= 2 * num_lp_variables);
}

std::function<bool()> RestartEveryKFailures(int k, SatSolver* solver) {
  bool reset_at_next_call = true;
  int next_num_failures = 0;
  return [=]() mutable {
    if (reset_at_next_call) {
      next_num_failures = solver->num_failures() + k;
      reset_at_next_call = false;
    } else if (solver->num_failures() >= next_num_failures) {
      reset_at_next_call = true;
    }
    return reset_at_next_call;
  };
}

std::function<bool()> SatSolverRestartPolicy(Model* model) {
  auto policy = model->GetOrCreate<RestartPolicy>();
  return [policy]() { return policy->ShouldRestart(); };
}

void ConfigureSearchHeuristics(Model* model) {
  SearchHeuristics& heuristics = *model->GetOrCreate<SearchHeuristics>();
  CHECK(heuristics.fixed_search != nullptr);
  heuristics.policy_index = 0;
  heuristics.decision_policies.clear();
  heuristics.restart_policies.clear();

  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  switch (parameters.search_branching()) {
    case SatParameters::AUTOMATIC_SEARCH: {
      std::function<LiteralIndex()> decision_policy;
      if (parameters.randomize_search()) {
        decision_policy = RandomizeOnRestartHeuristic(model);
      } else {
        decision_policy = SatSolverHeuristic(model);
      }
      decision_policy =
          SequentialSearch({decision_policy, heuristics.fixed_search});
      decision_policy = IntegerValueSelectionHeuristic(decision_policy, model);
      heuristics.decision_policies = {decision_policy};
      heuristics.restart_policies = {SatSolverRestartPolicy(model)};
      return;
    }
    case SatParameters::FIXED_SEARCH: {
      // Not all Boolean might appear in fixed_search(), so once there is no
      // decision left, we fix all Booleans that are still undecided.
      heuristics.decision_policies = {SequentialSearch(
          {heuristics.fixed_search, SatSolverHeuristic(model)})};

      if (parameters.randomize_search()) {
        heuristics.restart_policies = {SatSolverRestartPolicy(model)};
        return;
      }

      // TODO(user): We might want to restart if external info is available.
      // Code a custom restart for this?
      auto no_restart = []() { return false; };
      heuristics.restart_policies = {no_restart};
      return;
    }
    case SatParameters::HINT_SEARCH: {
      CHECK(heuristics.hint_search != nullptr);
      heuristics.decision_policies = {
          SequentialSearch({heuristics.hint_search, SatSolverHeuristic(model),
                            heuristics.fixed_search})};
      auto no_restart = []() { return false; };
      heuristics.restart_policies = {no_restart};
      return;
    }
    case SatParameters::PORTFOLIO_SEARCH: {
      heuristics.decision_policies = CompleteHeuristics(
          AddModelHeuristics({heuristics.fixed_search}, model),
          SequentialSearch(
              {SatSolverHeuristic(model), heuristics.fixed_search}));
      for (auto& ref : heuristics.decision_policies) {
        ref = IntegerValueSelectionHeuristic(ref, model);
      }
      heuristics.restart_policies.assign(heuristics.decision_policies.size(),
                                         SatSolverRestartPolicy(model));
      return;
    }
    case SatParameters::LP_SEARCH: {
      std::vector<std::function<LiteralIndex()>> lp_heuristics;
      for (const auto& ct :
           *(model->GetOrCreate<LinearProgrammingConstraintCollection>())) {
        lp_heuristics.push_back(ct->LPReducedCostAverageBranching());
      }
      if (lp_heuristics.empty()) {  // Revert to fixed search.
        heuristics.decision_policies = {SequentialSearch(
            {heuristics.fixed_search, SatSolverHeuristic(model)})},
        heuristics.restart_policies = {SatSolverRestartPolicy(model)};
        return;
      }
      heuristics.decision_policies = CompleteHeuristics(
          lp_heuristics, SequentialSearch({SatSolverHeuristic(model),
                                           heuristics.fixed_search}));
      heuristics.restart_policies.assign(heuristics.decision_policies.size(),
                                         SatSolverRestartPolicy(model));
      return;
    }
    case SatParameters::PSEUDO_COST_SEARCH: {
      std::function<LiteralIndex()> search =
          SequentialSearch({PseudoCost(model), SatSolverHeuristic(model),
                            heuristics.fixed_search});
      heuristics.decision_policies = {
          IntegerValueSelectionHeuristic(search, model)};
      heuristics.restart_policies = {SatSolverRestartPolicy(model)};
      return;
    }
    case SatParameters::PORTFOLIO_WITH_QUICK_RESTART_SEARCH: {
      std::function<LiteralIndex()> search = SequentialSearch(
          {RandomizeOnRestartHeuristic(model), heuristics.fixed_search});
      heuristics.decision_policies = {search};
      heuristics.restart_policies = {
          RestartEveryKFailures(10, model->GetOrCreate<SatSolver>())};
      return;
    }
  }
}

std::vector<std::function<LiteralIndex()>> AddModelHeuristics(
    const std::vector<std::function<LiteralIndex()>>& input_heuristics,
    Model* model) {
  std::vector<std::function<LiteralIndex()>> heuristics = input_heuristics;
  auto* extra_heuristics = model->GetOrCreate<SearchHeuristicsVector>();
  heuristics.insert(heuristics.end(), extra_heuristics->begin(),
                    extra_heuristics->end());
  return heuristics;
}

std::vector<std::function<LiteralIndex()>> CompleteHeuristics(
    const std::vector<std::function<LiteralIndex()>>& incomplete_heuristics,
    const std::function<LiteralIndex()>& completion_heuristic) {
  std::vector<std::function<LiteralIndex()>> complete_heuristics;
  complete_heuristics.reserve(incomplete_heuristics.size());
  for (const auto& incomplete : incomplete_heuristics) {
    complete_heuristics.push_back(
        SequentialSearch({incomplete, completion_heuristic}));
  }
  return complete_heuristics;
}

SatSolver::Status SolveIntegerProblem(Model* model) {
  TimeLimit* time_limit = model->GetOrCreate<TimeLimit>();
  if (time_limit->LimitReached()) return SatSolver::LIMIT_REACHED;

  SearchHeuristics& heuristics = *model->GetOrCreate<SearchHeuristics>();
  const int num_policies = heuristics.decision_policies.size();
  CHECK_NE(num_policies, 0);
  CHECK_EQ(num_policies, heuristics.restart_policies.size());

  // This is needed for recording the pseudo-costs.
  IntegerVariable objective_var = kNoIntegerVariable;
  {
    const ObjectiveDefinition* objective = model->Get<ObjectiveDefinition>();
    if (objective != nullptr) objective_var = objective->objective_var;
  }

  // Note that it is important to do the level-zero propagation if it wasn't
  // already done because EnqueueDecisionAndBackjumpOnConflict() assumes that
  // the solver is in a "propagated" state.
  SatSolver* const sat_solver = model->GetOrCreate<SatSolver>();

  // TODO(user): We have the issue that at level zero. calling the propagation
  // loop more than once can propagate more! This is because we call the LP
  // again and again on each level zero propagation. This is causing some
  // CHECKs() to fail in multithread (rarely) because when we associate new
  // literals to integer ones, Propagate() is indirectly called. Not sure yet
  // how to fix.
  if (!sat_solver->FinishPropagation()) return sat_solver->UnsatStatus();

  // Create and initialize pseudo costs.
  // TODO(user): If this ever shows up in a cpu profile, find a way to not
  // execute the code when pseudo costs are not needed.
  PseudoCosts* pseudo_costs = model->GetOrCreate<PseudoCosts>();

  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* implied_bounds = model->GetOrCreate<ImpliedBounds>();

  const SatParameters& sat_parameters = *(model->GetOrCreate<SatParameters>());

  // Main search loop.
  const int64 old_num_conflicts = sat_solver->num_failures();
  const int64 conflict_limit = sat_parameters.max_number_of_conflicts();
  int64 num_decisions_without_rins = 0;
  int64 num_decisions_without_probing = 0;
  while (!time_limit->LimitReached() &&
         (sat_solver->num_failures() - old_num_conflicts < conflict_limit)) {
    // If needed, restart and switch decision_policy.
    if (heuristics.restart_policies[heuristics.policy_index]()) {
      if (!sat_solver->RestoreSolverToAssumptionLevel()) {
        return sat_solver->UnsatStatus();
      }
      heuristics.policy_index = (heuristics.policy_index + 1) % num_policies;
    }

    if (sat_solver->CurrentDecisionLevel() == 0) {
      if (!implied_bounds->EnqueueNewDeductions()) {
        return SatSolver::INFEASIBLE;
      }

      auto* level_zero_callbacks =
          model->GetOrCreate<LevelZeroCallbackHelper>();
      for (const auto& cb : level_zero_callbacks->callbacks) {
        if (!cb()) {
          return SatSolver::INFEASIBLE;
        }
      }
    }

    LiteralIndex decision = kNoLiteralIndex;
    while (true) {
      decision = heuristics.decision_policies[heuristics.policy_index]();

      if (decision == kNoLiteralIndex) break;

      if (sat_solver->Assignment().LiteralIsAssigned(Literal(decision))) {
        // TODO(user): It would be nicer if this can never happen. For now, it
        // does because of the Propagate() not reaching the fixed point as
        // mentionned in a TODO above. As a work-around, we display a message
        // but do not crash and recall the decision heuristic.
        VLOG(1) << "Trying to take a decision that is already assigned!"
                << " Fix this. Continuing for now...";
        continue;
      }

      // Probing.
      if (sat_solver->CurrentDecisionLevel() == 0 &&
          sat_parameters.probing_period_at_root() > 0 &&
          ++num_decisions_without_probing >=
              sat_parameters.probing_period_at_root()) {
        num_decisions_without_probing = 0;
        // TODO(user): Be smarter about what variables we probe, we can also
        // do more than one.

        if (!ProbeBooleanVariables(0.1, {Literal(decision).Variable()},
                                   model)) {
          return SatSolver::INFEASIBLE;
        }
        DCHECK_EQ(sat_solver->CurrentDecisionLevel(), 0);

        // We need to check after the probing that the literal is not fixed,
        // otherwise we just go to the next decision.
        if (sat_solver->Assignment().LiteralIsAssigned(Literal(decision))) {
          continue;
        }
      }
      break;
    }

    // Record the changelist and objective bounds for updating pseudo costs.
    const std::vector<PseudoCosts::VariableBoundChange> bound_changes =
        GetBoundChanges(decision, model);
    IntegerValue current_obj_lb = kMinIntegerValue;
    IntegerValue current_obj_ub = kMaxIntegerValue;
    if (objective_var != kNoIntegerVariable) {
      current_obj_lb = integer_trail->LowerBound(objective_var);
      current_obj_ub = integer_trail->UpperBound(objective_var);
    }
    const int old_level = sat_solver->CurrentDecisionLevel();

    // No decision means that we reached a leave of the search tree and that
    // we have a feasible solution.
    if (decision == kNoLiteralIndex) {
      SolutionDetails* solution_details = model->Mutable<SolutionDetails>();
      if (solution_details != nullptr) {
        solution_details->LoadFromTrail(*integer_trail);
      }

      // Save the current polarity of all Booleans in the solution. It will be
      // followed for the next SAT decisions. This is known to be a good policy
      // for optimization problem. Note that for decision problem we don't care
      // since we are just done as soon as a solution is found.
      //
      // This idea is kind of "well known", see for instance the "LinSBPS"
      // submission to the maxSAT 2018 competition by Emir Demirovic and Peter
      // Stuckey where they show it is a good idea and provide more references.
      if (model->GetOrCreate<SatParameters>()->use_optimization_hints()) {
        auto* sat_decision = model->GetOrCreate<SatDecisionPolicy>();
        const auto& trail = *model->GetOrCreate<Trail>();
        for (int i = 0; i < trail.Index(); ++i) {
          sat_decision->SetAssignmentPreference(trail[i], 0.0);
        }
      }
      return SatSolver::FEASIBLE;
    }

    // TODO(user): on some problems, this function can be quite long. Expand
    // so that we can check the time limit at each step?
    sat_solver->EnqueueDecisionAndBackjumpOnConflict(Literal(decision));

    // Update the implied bounds each time we enqueue a literal at level zero.
    // This is "almost free", so we might as well do it.
    if (old_level == 0 && sat_solver->CurrentDecisionLevel() == 1) {
      implied_bounds->ProcessIntegerTrail(Literal(decision));
    }

    // Update the pseudo costs.
    if (sat_solver->CurrentDecisionLevel() > old_level &&
        objective_var != kNoIntegerVariable) {
      const IntegerValue new_obj_lb = integer_trail->LowerBound(objective_var);
      const IntegerValue new_obj_ub = integer_trail->UpperBound(objective_var);
      const IntegerValue objective_bound_change =
          (new_obj_lb - current_obj_lb) + (current_obj_ub - new_obj_ub);
      pseudo_costs->UpdateCost(bound_changes, objective_bound_change);
    }

    sat_solver->AdvanceDeterministicTime(time_limit);
    if (!sat_solver->ReapplyAssumptionsIfNeeded()) {
      return sat_solver->UnsatStatus();
    }
    if (model->Get<SharedRINSNeighborhoodManager>() != nullptr) {
      // If RINS is activated, we need to make sure the SolutionDetails is
      // created.
      model->GetOrCreate<SolutionDetails>();
      num_decisions_without_rins++;
      // TODO(user): Experiment more around dynamically changing the
      // threshold for trigerring RINS. Alternatively expose this as parameter
      // so this can be tuned later.
      if (num_decisions_without_rins >= 100) {
        num_decisions_without_rins = 0;
        AddRINSNeighborhood(model);
      }
    }
  }
  return SatSolver::Status::LIMIT_REACHED;
}

SatSolver::Status ResetAndSolveIntegerProblem(
    const std::vector<Literal>& assumptions, Model* model) {
  SatSolver* const solver = model->GetOrCreate<SatSolver>();

  // Sync the bound first.
  if (!solver->ResetToLevelZero()) return solver->UnsatStatus();
  auto* level_zero_callbacks = model->GetOrCreate<LevelZeroCallbackHelper>();
  for (const auto& cb : level_zero_callbacks->callbacks) {
    if (!cb()) return SatSolver::INFEASIBLE;
  }

  // Add the assumptions if any and solve.
  if (!solver->ResetWithGivenAssumptions(assumptions)) {
    return solver->UnsatStatus();
  }
  return SolveIntegerProblem(model);
}

SatSolver::Status SolveIntegerProblemWithLazyEncoding(Model* model) {
  const IntegerVariable num_vars =
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables();
  std::vector<IntegerVariable> all_variables;
  for (IntegerVariable var(0); var < num_vars; ++var) {
    all_variables.push_back(var);
  }

  SearchHeuristics& heuristics = *model->GetOrCreate<SearchHeuristics>();
  heuristics.policy_index = 0;
  heuristics.decision_policies = {SequentialSearch(
      {SatSolverHeuristic(model),
       FirstUnassignedVarAtItsMinHeuristic(all_variables, model)})};
  heuristics.restart_policies = {SatSolverRestartPolicy(model)};
  return ResetAndSolveIntegerProblem(/*assumptions=*/{}, model);
}

}  // namespace sat
}  // namespace operations_research
