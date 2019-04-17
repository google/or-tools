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

#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/pseudo_costs.h"
#include "ortools/sat/rins.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

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
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* trail = model->GetOrCreate<Trail>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  const IntegerValue var_lb = integer_trail->LowerBound(var);
  const IntegerValue var_ub = integer_trail->UpperBound(var);
  CHECK_LT(var_lb, var_ub);

  const IntegerValue chosen_value =
      var_lb + std::max(IntegerValue(1), (var_ub - var_lb) / IntegerValue(2));
  const Literal ge = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, chosen_value));
  CHECK(!trail->Assignment().VariableIsAssigned(ge.Variable()));
  VLOG(2) << "Chosen " << var << " >= " << chosen_value;
  return ge.Index();
}

LiteralIndex SplitAroundGivenValue(IntegerVariable positive_var,
                                   IntegerValue value, Model* model) {
  DCHECK(VariableIsPositive(positive_var));
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* trail = model->GetOrCreate<Trail>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();

  // The value might be out of bounds. In that case we return kNoLiteralIndex.
  const IntegerValue lb = integer_trail->LowerBound(positive_var);
  const IntegerValue ub = integer_trail->UpperBound(positive_var);

  // We try first (<= value), but if this do not reduce the domain we
  // try to enqueue (>= value). Note that even for domain with hole,
  // since we know that this variable is not fixed, one of the two
  // alternative must reduce the domain.
  //
  // TODO(user): use GetOrCreateLiteralAssociatedToEquality() instead?
  // It may replace two decision by only one. However this function
  // cannot currently be called during search, but that should be easy
  // enough to fix.
  if (value >= lb && value < ub) {
    const Literal le = encoder->GetOrCreateAssociatedLiteral(
        IntegerLiteral::LowerOrEqual(positive_var, value));
    CHECK(!trail->Assignment().VariableIsAssigned(le.Variable()));
    return le.Index();
  }
  if (value > lb && value <= ub) {
    const Literal ge = encoder->GetOrCreateAssociatedLiteral(
        IntegerLiteral::GreaterOrEqual(positive_var, value));
    CHECK(!trail->Assignment().VariableIsAssigned(ge.Variable()));
    return ge.Index();
  }
  return kNoLiteralIndex;
}

LiteralIndex SplitAroundLpValue(IntegerVariable var, Model* model) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* lp_dispatcher = model->GetOrCreate<LinearProgrammingDispatcher>();

  const IntegerVariable positive_var = PositiveVariable(var);
  DCHECK(!integer_trail->IsCurrentlyIgnored(positive_var));
  LinearProgrammingConstraint* lp =
      gtl::FindWithDefault(*lp_dispatcher, positive_var, nullptr);
  if (lp == nullptr) return kNoLiteralIndex;
  const IntegerValue value = IntegerValue(
      static_cast<int64>(std::round(lp->GetSolutionValue(positive_var))));

  // Because our lp solution might be from higher up in the tree, it
  // is possible that value is now outside the domain of positive_var.
  // In this case, we just revert to the current literal.
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

std::function<LiteralIndex()> IntegerValueSelectionHeuristic(
    std::function<LiteralIndex()> var_selection_heuristic, Model* model) {
  std::vector<std::function<LiteralIndex(IntegerVariable)>>
      value_selection_heuristics;

  // TODO(user): Experiment with value selection heuristics.

  // LP based value.
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  if (LinearizedPartIsLarge(model) &&
      (parameters.exploit_integer_lp_solution() ||
       parameters.exploit_all_lp_solution())) {
    std::function<LiteralIndex(IntegerVariable)> lp_value =
        [model](IntegerVariable var) {
          if (LpSolutionIsExploitable(model)) {
            return SplitAroundLpValue(PositiveVariable(var), model);
          }
          return kNoLiteralIndex;
        };
    value_selection_heuristics.push_back(lp_value);
    VLOG(1) << "Using LP value selection heuristic";
  }

  // Solution based value.
  std::function<LiteralIndex(IntegerVariable)> solution_value =
      [model](IntegerVariable var) {
        return SplitDomainUsingBestSolutionValue(var, model);
      };
  value_selection_heuristics.push_back(solution_value);
  VLOG(1) << "Using best solution value selection heuristic";

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
    if (LpSolutionIsExploitable(model)) {
      return SplitAroundLpValue(PositiveVariable(var), model);
    }
    return kNoLiteralIndex;
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

LiteralIndex ExploitLpSolution(const LiteralIndex decision, Model* model) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();

  if (decision == kNoLiteralIndex) {
    return decision;
  }

  if (LpSolutionIsExploitable(model)) {
    for (const IntegerLiteral l :
         encoder->GetAllIntegerLiterals(Literal(decision))) {
      const IntegerVariable positive_var = PositiveVariable(l.var);
      if (integer_trail->IsCurrentlyIgnored(positive_var)) continue;
      const LiteralIndex lp_decision = SplitAroundLpValue(positive_var, model);
      if (lp_decision != kNoLiteralIndex) {
        return lp_decision;
      }
    }
  }
  return decision;
}

std::function<LiteralIndex()> ExploitLpSolution(
    std::function<LiteralIndex()> heuristic, Model* model) {
  // Use the normal heuristic if the LP(s) do not seem to cover enough variables
  // to be relevant.
  // TODO(user): Instead, try and depending on the result call it again or not?
  if (!LinearizedPartIsLarge(model)) return heuristic;

  return [=]() mutable {
    const LiteralIndex decision = heuristic();
    return ExploitLpSolution(decision, model);
  };
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

void ConfigureSearchHeuristics(
    const std::function<LiteralIndex()>& fixed_search, Model* model) {
  SearchHeuristics& heuristics = *model->GetOrCreate<SearchHeuristics>();
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
      decision_policy = SequentialSearch({decision_policy, fixed_search});
      if (parameters.exploit_integer_lp_solution() ||
          parameters.exploit_all_lp_solution()) {
        decision_policy = ExploitLpSolution(decision_policy, model);
      }
      heuristics.decision_policies = {decision_policy};
      heuristics.restart_policies = {SatSolverRestartPolicy(model)};
      return;
    }
    case SatParameters::FIXED_SEARCH: {
      // Not all Boolean might appear in fixed_search(), so once there is no
      // decision left, we fix all Booleans that are still undecided.
      heuristics.decision_policies = {
          SequentialSearch({fixed_search, SatSolverHeuristic(model)})};

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
    case SatParameters::PORTFOLIO_SEARCH: {
      heuristics.decision_policies = CompleteHeuristics(
          AddModelHeuristics({fixed_search}, model),
          SequentialSearch({SatSolverHeuristic(model), fixed_search}));
      if (parameters.exploit_integer_lp_solution()) {
        for (auto& ref : heuristics.decision_policies) {
          ref = ExploitLpSolution(ref, model);
        }
      }
      heuristics.restart_policies.assign(heuristics.decision_policies.size(),
                                         SatSolverRestartPolicy(model));
      return;
    }
    case SatParameters::LP_SEARCH: {
      // Fill portfolio with pseudocost heuristics.
      std::vector<std::function<LiteralIndex()>> lp_heuristics;
      for (const auto& ct :
           *(model->GetOrCreate<LinearProgrammingConstraintCollection>())) {
        lp_heuristics.push_back(ct->LPReducedCostAverageBranching());
      }
      if (lp_heuristics.empty()) {  // Revert to fixed search.
        heuristics.decision_policies = {SequentialSearch(
            {fixed_search, SatSolverHeuristic(model)})},
        heuristics.restart_policies = {SatSolverRestartPolicy(model)};
        return;
      }
      heuristics.decision_policies = CompleteHeuristics(
          lp_heuristics,
          SequentialSearch({SatSolverHeuristic(model), fixed_search}));
      heuristics.restart_policies.assign(heuristics.decision_policies.size(),
                                         SatSolverRestartPolicy(model));
      return;
    }
    case SatParameters::PSEUDO_COST_SEARCH: {
      std::function<LiteralIndex()> search = SequentialSearch(
          {PseudoCost(model), SatSolverHeuristic(model), fixed_search});
      heuristics.decision_policies = {
          IntegerValueSelectionHeuristic(search, model)};
      heuristics.restart_policies = {SatSolverRestartPolicy(model)};
      return;
    }
    case SatParameters::PORTFOLIO_WITH_QUICK_RESTART_SEARCH: {
      std::function<LiteralIndex()> search =
          SequentialSearch({RandomizeOnRestartHeuristic(model), fixed_search});
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

void SolutionDetails::LoadFromTrail(const IntegerTrail& integer_trail) {
  const IntegerVariable num_vars = integer_trail.NumIntegerVariables();
  best_solution.resize(num_vars.value());
  // NOTE: There might be some variables which are not fixed.
  for (IntegerVariable var(0); var < num_vars; ++var) {
    best_solution[var] = integer_trail.LowerBound(var);
  }
  solution_count++;
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
  SatSolver* const solver = model->GetOrCreate<SatSolver>();
  if (!solver->FinishPropagation()) return solver->UnsatStatus();

  // Create and initialize pseudo costs.
  // TODO(user): If this ever shows up in a cpu profile, find a way to not
  // execute the code when pseudo costs are not needed.
  PseudoCosts* pseudo_costs = model->GetOrCreate<PseudoCosts>();

  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();

  // Main search loop.
  const int64 old_num_conflicts = solver->num_failures();
  const int64 conflict_limit =
      model->GetOrCreate<SatParameters>()->max_number_of_conflicts();
  int64 num_decisions_without_rins = 0;
  while (!time_limit->LimitReached() &&
         (solver->num_failures() - old_num_conflicts < conflict_limit)) {
    // If needed, restart and switch decision_policy.
    if (heuristics.restart_policies[heuristics.policy_index]()) {
      if (!solver->RestoreSolverToAssumptionLevel()) {
        return solver->UnsatStatus();
      }
      heuristics.policy_index = (heuristics.policy_index + 1) % num_policies;
    }

    if (solver->CurrentDecisionLevel() == 0) {
      auto* level_zero_callbacks =
          model->GetOrCreate<LevelZeroCallbackHelper>();
      for (const auto& cb : level_zero_callbacks->callbacks) {
        if (!cb()) {
          return SatSolver::INFEASIBLE;
        }
      }
    }

    // Get next decision, try to enqueue.
    const LiteralIndex decision =
        heuristics.decision_policies[heuristics.policy_index]();

    // Record the changelist and objective bounds for updating pseudo costs.
    const std::vector<PseudoCosts::VariableBoundChange> bound_changes =
        GetBoundChanges(decision, model);
    IntegerValue current_obj_lb = kMinIntegerValue;
    IntegerValue current_obj_ub = kMaxIntegerValue;
    if (objective_var != kNoIntegerVariable) {
      current_obj_lb = integer_trail->LowerBound(objective_var);
      current_obj_ub = integer_trail->UpperBound(objective_var);
    }
    const int old_level = solver->CurrentDecisionLevel();

    if (decision == kNoLiteralIndex) {
      SolutionDetails* solution_details = model->Mutable<SolutionDetails>();
      if (solution_details != nullptr) {
        solution_details->LoadFromTrail(*integer_trail);
      }
      return SatSolver::FEASIBLE;
    }

    // TODO(user): on some problems, this function can be quite long. Expand
    // so that we can check the time limit at each step?
    solver->EnqueueDecisionAndBackjumpOnConflict(Literal(decision));

    // Update the pseudo costs.
    if (solver->CurrentDecisionLevel() > old_level &&
        objective_var != kNoIntegerVariable) {
      const IntegerValue new_obj_lb = integer_trail->LowerBound(objective_var);
      const IntegerValue new_obj_ub = integer_trail->UpperBound(objective_var);
      const IntegerValue objective_bound_change =
          (new_obj_lb - current_obj_lb) + (current_obj_ub - new_obj_ub);
      pseudo_costs->UpdateCost(bound_changes, objective_bound_change);
    }

    solver->AdvanceDeterministicTime(time_limit);
    if (!solver->ReapplyAssumptionsIfNeeded()) return solver->UnsatStatus();
    if (model->GetOrCreate<SolutionDetails>()->solution_count > 0 &&
        model->Get<SharedRINSNeighborhoodManager>() != nullptr) {
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

void LogNewSolution(const std::string& event_or_solution_count,
                    double time_in_seconds, double obj_lb, double obj_ub,
                    const std::string& solution_info) {
  LOG(INFO) << absl::StrFormat("#%-5s %6.2fs  obj:[%.9g,%.9g]  %s",
                               event_or_solution_count, time_in_seconds, obj_lb,
                               obj_ub, solution_info);
}

void LogNewSatSolution(const std::string& event_or_solution_count,
                       double time_in_seconds,
                       const std::string& solution_info) {
  LOG(INFO) << absl::StrFormat("#%-5s %6.2fs  %s", event_or_solution_count,
                               time_in_seconds, solution_info);
}

}  // namespace sat
}  // namespace operations_research
