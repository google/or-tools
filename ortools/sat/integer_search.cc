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
#include "ortools/sat/sat_decision.h"

namespace operations_research {
namespace sat {

// TODO(user): the complexity of this heuristic and the one below is ok when
// use_fixed_search() is false because it is not executed often, however, we do
// a linear scan for each search decision when use_fixed_search() is true, which
// seems bad. Improve.
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

std::function<LiteralIndex()> NullSearch() {
  return []() { return kNoLiteralIndex; };
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
  SatSolver* const solver = model->GetOrCreate<SatSolver>();
  if (solver->IsModelUnsat()) return SatSolver::MODEL_UNSAT;
  solver->Backtrack(0);
  solver->SetAssumptionLevel(0);

  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  if (parameters.use_fixed_search()) {
    CHECK(assumptions.empty()) << "Not supported yet";
    // Note that it is important to do the level-zero propagation if it wasn't
    // already done because EnqueueDecisionAndBackjumpOnConflict() assumes that
    // the solver is in a "propagated" state.
    if (!solver->Propagate()) return SatSolver::MODEL_UNSAT;
  }

  if (parameters.use_portfolio_search() && assumptions.empty()) {
    auto incomplete_portfolio = AddModelHeuristics({next_decision}, model);
    auto portfolio = CompleteHeuristics(
        incomplete_portfolio,
        SequentialSearch({SatSolverHeuristic(model), next_decision}));
    auto default_restart_policy = SatSolverRestartPolicy(model);
    auto restart_policies = std::vector<std::function<bool()>>(
        portfolio.size(), default_restart_policy);
    return SolveProblemWithPortfolioSearch(portfolio, restart_policies, model);
  }

  TimeLimit* time_limit = model->GetOrCreate<TimeLimit>();
  while (!time_limit->LimitReached()) {
    // If we are not in fixed search, let the SAT solver do a full search to
    // instantiate all the already created Booleans.
    if (!parameters.use_fixed_search()) {
      if (assumptions.empty()) {
        const SatSolver::Status status = solver->Solve();
        if (status != SatSolver::MODEL_SAT) return status;
      } else {
        // TODO(user): We actually don't want to reset the solver from one
        // loop to the next as it is less efficient.
        const SatSolver::Status status =
            solver->ResetAndSolveWithGivenAssumptions(assumptions);
        if (status != SatSolver::MODEL_SAT) return status;
      }
    }

    // Look for the "best" non-instantiated integer variable.
    // If all variables are instantiated, we have a solution.
    const LiteralIndex decision =
        next_decision == nullptr ? kNoLiteralIndex : next_decision();
    if (decision == kNoLiteralIndex) return SatSolver::MODEL_SAT;

    // Always try to Enqueue this literal as the next decision so we bypass
    // any default polarity heuristic the solver may use on this literal.
    solver->EnqueueDecisionAndBackjumpOnConflict(Literal(decision));
    if (solver->IsModelUnsat()) return SatSolver::MODEL_UNSAT;
  }
  return SatSolver::Status::LIMIT_REACHED;
}

std::vector<std::function<LiteralIndex()>> AddModelHeuristics(
    const std::vector<std::function<LiteralIndex()>>& input_heuristics,
    Model* model) {
  std::vector<std::function<LiteralIndex()>> heuristics = input_heuristics;
  auto* extra_heuristics = model->GetOrCreate<SearchHeuristicsVector>();
  heuristics.insert(heuristics.end(), extra_heuristics->begin(),
                    extra_heuristics->end());
  heuristics.push_back(NullSearch());
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
  CHECK_EQ(num_policies, restart_policies.size());
  CHECK_GT(num_policies, 0);
  SatSolver* const solver = model->GetOrCreate<SatSolver>();
  if (solver->IsModelUnsat()) return SatSolver::MODEL_UNSAT;
  solver->Backtrack(0);

  // Note that it is important to do the level-zero propagation if it wasn't
  // already done because EnqueueDecisionAndBackjumpOnConflict() assumes that
  // the solver is in a "propagated" state.
  if (!solver->Propagate()) return SatSolver::MODEL_UNSAT;

  // Main search loop.
  int policy_index = 0;
  TimeLimit* time_limit = model->GetOrCreate<TimeLimit>();
  while (!time_limit->LimitReached()) {
    // If needed, restart and switch decision_policy.
    if (restart_policies[policy_index]()) {
      solver->Backtrack(0);
      if (!solver->Propagate()) return SatSolver::MODEL_UNSAT;
      policy_index = (policy_index + 1) % num_policies;
    }

    // Get next decision, try to enqueue.
    const LiteralIndex decision = decision_policies[policy_index]();
    if (decision == kNoLiteralIndex) return SatSolver::MODEL_SAT;
    solver->EnqueueDecisionAndBackjumpOnConflict(Literal(decision));
    if (solver->IsModelUnsat()) return SatSolver::MODEL_UNSAT;
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
