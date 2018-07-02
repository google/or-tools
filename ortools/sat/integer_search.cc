// Copyright 2010-2017 Google
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

#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

// TODO(user): the complexity caused by the linear scan in this heuristic and
// the one below is ok when search_branching is set to SAT_SEARCH because it is
// not executed often, but otherwise it is done for each search decision,
// which seems expensive. Improve.
std::function<LiteralIndex()> FirstUnassignedVarAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model) {
  IntegerEncoder* const integer_encoder = model->GetOrCreate<IntegerEncoder>();
  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();
  return [integer_encoder, integer_trail, /*copy*/ vars]() {
    for (const IntegerVariable var : vars) {
      // Note that there is no point trying to fix a currently ignored variable.
      if (integer_trail->IsCurrentlyIgnored(var)) continue;
      const IntegerValue lb = integer_trail->LowerBound(var);
      if (lb < integer_trail->UpperBound(var)) {
        return integer_encoder
            ->GetOrCreateAssociatedLiteral(
                IntegerLiteral::LowerOrEqual(var, lb))
            .Index();
      }
    }
    return kNoLiteralIndex;
  };
}

std::function<LiteralIndex()> UnassignedVarWithLowestMinAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model) {
  IntegerEncoder* const integer_encoder = model->GetOrCreate<IntegerEncoder>();
  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();
  return [integer_encoder, integer_trail, /*copy */ vars]() {
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
    return integer_encoder
        ->GetOrCreateAssociatedLiteral(
            IntegerLiteral::LowerOrEqual(candidate, candidate_lb))
        .Index();
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

std::function<LiteralIndex()> SatSolverHeuristic(Model* model) {
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  Trail* trail = model->GetOrCreate<Trail>();
  SatDecisionPolicy* decision_policy = model->GetOrCreate<SatDecisionPolicy>();
  return [sat_solver, trail, decision_policy] {
    const bool all_assigned = trail->Index() == sat_solver->NumVariables();
    return all_assigned ? kNoLiteralIndex
                        : decision_policy->NextBranch().Index();
  };
}

std::function<LiteralIndex()> RandomizeOnRestartSatSolverHeuristic(
    Model* model) {
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  Trail* trail = model->GetOrCreate<Trail>();
  SatDecisionPolicy* decision_policy = model->GetOrCreate<SatDecisionPolicy>();
  return [sat_solver, trail, decision_policy, model] {
    if (sat_solver->CurrentDecisionLevel() == 0) {
      RandomizeDecisionHeuristic(model->GetOrCreate<ModelRandomGenerator>(),
                                 model->GetOrCreate<SatParameters>());
      decision_policy->ResetDecisionHeuristic();
    }
    const bool all_assigned = trail->Index() == sat_solver->NumVariables();
    return all_assigned ? kNoLiteralIndex
                        : decision_policy->NextBranch().Index();
  };
}

std::function<LiteralIndex()> FollowHint(
    const std::vector<BooleanOrIntegerVariable>& vars,
    const std::vector<IntegerValue>& values, Model* model) {
  const Trail* trail = model->GetOrCreate<Trail>();
  const IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
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

        // We try first (<= value), but if this do not reduce the domain we
        // try to enqueue (>= value). Note that even for domain with hole,
        // since we know that this variable is not fixed, one of the two
        // alternative must reduce the domain.
        //
        // TODO(user): De-dup with logic in ExploitIntegerLpSolution.
        if (value >= lb && value < ub) {
          const Literal le = encoder->GetOrCreateAssociatedLiteral(
              IntegerLiteral::LowerOrEqual(integer_var, value));
          CHECK(!trail->Assignment().VariableIsAssigned(le.Variable()));
          return le.Index();
        }
        if (value > lb && value <= ub) {
          const Literal ge = encoder->GetOrCreateAssociatedLiteral(
              IntegerLiteral::GreaterOrEqual(integer_var, value));
          CHECK(!trail->Assignment().VariableIsAssigned(ge.Variable()));
          return ge.Index();
        }

        // If the value is outside the current possible domain, we skip it.
        continue;
      }
    }
    return kNoLiteralIndex;
  };
}

std::function<LiteralIndex()> ExploitIntegerLpSolution(
    std::function<LiteralIndex()> heuristic, Model* model) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* trail = model->GetOrCreate<Trail>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* lp_dispatcher = model->GetOrCreate<LinearProgrammingDispatcher>();
  auto* lp_constraints =
      model->GetOrCreate<LinearProgrammingConstraintCollection>();

  // Use the normal heuristic if the LP(s) do not seem to cover enough variables
  // to be relevant.
  // TODO(user): Instead, try and depending on the result call it again or not?
  {
    int num_lp_variables = 0;
    for (LinearProgrammingConstraint* lp : *lp_constraints) {
      num_lp_variables += lp->NumVariables();
    }
    const int num_integer_variables =
        model->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value() / 2;
    if (num_integer_variables > 4 * num_lp_variables) return heuristic;
  }

  bool last_decision_followed_lp = false;
  int old_level = 0;
  double old_obj = 0.0;
  return [=]() mutable {
    const LiteralIndex decision = heuristic();
    if (decision == kNoLiteralIndex) {
      if (last_decision_followed_lp) {
        VLOG(1) << "Integer LP solution is feasible, level:" << old_level
                << "->" << trail->CurrentDecisionLevel() << " obj:" << old_obj;
      }
      last_decision_followed_lp = false;
      return kNoLiteralIndex;
    }

    bool all_lp_integers = true;
    double obj = 0.0;
    for (LinearProgrammingConstraint* lp : *lp_constraints) {
      if (!lp->HasSolution() || !lp->SolutionIsInteger()) {
        all_lp_integers = false;
        break;
      }
      obj += lp->SolutionObjectiveValue();
    }
    if (all_lp_integers) {
      if (!last_decision_followed_lp || obj != old_obj) {
        old_level = trail->CurrentDecisionLevel();
        old_obj = obj;
        VLOG(2) << "Integer LP solution at level:" << old_level
                << " obj:" << old_obj;
      }
      for (IntegerLiteral l : encoder->GetIntegerLiterals(Literal(decision))) {
        const IntegerVariable positive_var =
            VariableIsPositive(l.var) ? l.var : NegationOf(l.var);
        LinearProgrammingConstraint* lp =
            gtl::FindWithDefault(*lp_dispatcher, positive_var, nullptr);
        if (lp != nullptr) {
          const IntegerValue value = IntegerValue(static_cast<int64>(
              std::round(lp->GetSolutionValue(positive_var))));

          // Because our lp solution might be from higher up in the tree, it
          // is possible that value is now outside the domain of positive_var.
          // In this case, we just revert to the current literal.
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
            last_decision_followed_lp = true;
            return le.Index();
          }
          if (value > lb && value <= ub) {
            const Literal ge = encoder->GetOrCreateAssociatedLiteral(
                IntegerLiteral::GreaterOrEqual(positive_var, value));
            CHECK(!trail->Assignment().VariableIsAssigned(ge.Variable()));
            last_decision_followed_lp = true;
            return ge.Index();
          }
        }
      }
    }

    last_decision_followed_lp = false;
    return decision;
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

SatSolver::Status SolveIntegerProblemWithLazyEncoding(
    const std::vector<Literal>& assumptions,
    const std::function<LiteralIndex()>& next_decision, Model* model) {
  if (model->GetOrCreate<TimeLimit>()->LimitReached()) {
    return SatSolver::LIMIT_REACHED;
  }
  SatSolver* const solver = model->GetOrCreate<SatSolver>();
  if (!solver->ResetWithGivenAssumptions(assumptions)) {
    return solver->UnsatStatus();
  }
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  switch (parameters.search_branching()) {
    case SatParameters::AUTOMATIC_SEARCH: {
      std::function<LiteralIndex()> search;
      if (parameters.randomize_search()) {
        search = SequentialSearch(
            {RandomizeOnRestartSatSolverHeuristic(model), next_decision});
      } else {
        search = SequentialSearch({SatSolverHeuristic(model), next_decision});
      }
      if (parameters.exploit_integer_lp_solution()) {
        search = ExploitIntegerLpSolution(search, model);
      }
      return SolveProblemWithPortfolioSearch(
          {search}, {SatSolverRestartPolicy(model)}, model);
    }
    case SatParameters::FIXED_SEARCH: {
      // Not all Boolean might appear in next_decision(), so once there is no
      // decision left, we fix all Booleans that are still undecided.
      if (parameters.randomize_search()) {
        return SolveProblemWithPortfolioSearch(
            {SequentialSearch({next_decision, SatSolverHeuristic(model)})},
            {SatSolverRestartPolicy(model)}, model);
      } else {
        auto no_restart = []() { return false; };
        return SolveProblemWithPortfolioSearch(
            {SequentialSearch({next_decision, SatSolverHeuristic(model)})},
            {no_restart}, model);
      }
    }
    case SatParameters::PORTFOLIO_SEARCH: {
      auto incomplete_portfolio = AddModelHeuristics({next_decision}, model);
      auto portfolio = CompleteHeuristics(
          incomplete_portfolio,
          SequentialSearch({SatSolverHeuristic(model), next_decision}));
      if (parameters.exploit_integer_lp_solution()) {
        for (auto& ref : portfolio) {
          ref = ExploitIntegerLpSolution(ref, model);
        }
      }
      auto default_restart_policy = SatSolverRestartPolicy(model);
      auto restart_policies = std::vector<std::function<bool()>>(
          portfolio.size(), default_restart_policy);
      return SolveProblemWithPortfolioSearch(portfolio, restart_policies,
                                             model);
    }
    case SatParameters::LP_SEARCH: {
      // Fill portfolio with pseudocost heuristics.
      std::vector<std::function<LiteralIndex()>> lp_heuristics;
      for (const auto& ct :
           *(model->GetOrCreate<LinearProgrammingConstraintCollection>())) {
        lp_heuristics.push_back(ct->LPReducedCostAverageBranching());
      }
      if (lp_heuristics.empty()) break;  // Fall back to automatic search.
      auto portfolio = CompleteHeuristics(
          lp_heuristics,
          SequentialSearch({SatSolverHeuristic(model), next_decision}));
      auto default_restart_policy = SatSolverRestartPolicy(model);
      auto restart_policies = std::vector<std::function<bool()>>(
          portfolio.size(), default_restart_policy);
      return SolveProblemWithPortfolioSearch(portfolio, restart_policies,
                                             model);
    }
  }
  return SatSolver::LIMIT_REACHED;
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

SatSolver::Status SolveProblemWithPortfolioSearch(
    std::vector<std::function<LiteralIndex()>> decision_policies,
    std::vector<std::function<bool()>> restart_policies, Model* model) {
  const int num_policies = decision_policies.size();
  if (num_policies == 0) return SatSolver::FEASIBLE;
  CHECK_EQ(num_policies, restart_policies.size());
  SatSolver* const solver = model->GetOrCreate<SatSolver>();
  const SatParameters* const params = model->Get<SatParameters>();
  const bool use_core = params != nullptr && params->optimize_with_core();
  const ObjectiveSynchronizationHelper* helper =
      model->Get<ObjectiveSynchronizationHelper>();
  const bool synchronize_objective =
      !use_core && helper != nullptr && helper->get_external_bound != nullptr &&
      helper->objective_var != kNoIntegerVariable;

  // Note that it is important to do the level-zero propagation if it wasn't
  // already done because EnqueueDecisionAndBackjumpOnConflict() assumes that
  // the solver is in a "propagated" state.
  if (!solver->FinishPropagation()) return solver->UnsatStatus();

  // Main search loop.
  int policy_index = 0;
  TimeLimit* time_limit = model->GetOrCreate<TimeLimit>();
  const int64 old_num_conflicts = solver->num_failures();
  const int64 conflict_limit =
      model->GetOrCreate<SatParameters>()->max_number_of_conflicts();
  while (!time_limit->LimitReached() &&
         (solver->num_failures() - old_num_conflicts < conflict_limit)) {
    // If needed, restart and switch decision_policy.
    if (restart_policies[policy_index]()) {
      if (!solver->RestoreSolverToAssumptionLevel()) {
        return solver->UnsatStatus();
      }
      policy_index = (policy_index + 1) % num_policies;
    }

    // Check external objective, and restart if a better one is supplied.
    // TODO(user): Maybe do not check this at each decision.
    if (synchronize_objective) {
      const double external_bound = helper->get_external_bound();
      if (std::isfinite(external_bound)) {
        IntegerValue best_bound(helper->UnscaledObjective(external_bound));
        IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();
        if (best_bound <= integer_trail->UpperBound(helper->objective_var)) {
          if (!solver->RestoreSolverToAssumptionLevel()) {
            return solver->UnsatStatus();
          }
          DCHECK_EQ(solver->CurrentDecisionLevel(), 0);
          if (!integer_trail->Enqueue(
                  IntegerLiteral::LowerOrEqual(helper->objective_var,
                                               best_bound - 1),
                  {}, {})) {
            return SatSolver::INFEASIBLE;
          }
          if (!solver->FinishPropagation()) {
            return solver->UnsatStatus();
          }
        }
      }
    }

    // Get next decision, try to enqueue.
    const LiteralIndex decision = decision_policies[policy_index]();
    if (decision == kNoLiteralIndex) return SatSolver::FEASIBLE;

    // TODO(user): on some problems, this function can be quite long. Expand
    // so that we can check the time limit at each step?
    solver->EnqueueDecisionAndBackjumpOnConflict(Literal(decision));
    solver->AdvanceDeterministicTime(time_limit);
    if (!solver->ReapplyAssumptionsIfNeeded()) return solver->UnsatStatus();
  }
  return SatSolver::Status::LIMIT_REACHED;
}

// Shortcut when there is no assumption, and we consider all variables in
// order for the search decision.
SatSolver::Status SolveIntegerProblemWithLazyEncoding(Model* model) {
  const IntegerVariable num_vars =
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables();
  std::vector<IntegerVariable> all_variables;
  for (IntegerVariable var(0); var < num_vars; ++var) {
    all_variables.push_back(var);
  }
  return SolveIntegerProblemWithLazyEncoding(
      {}, FirstUnassignedVarAtItsMinHeuristic(all_variables, model), model);
}

}  // namespace sat
}  // namespace operations_research
