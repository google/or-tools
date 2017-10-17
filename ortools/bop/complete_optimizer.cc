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

#include "ortools/bop/complete_optimizer.h"

#include "ortools/bop/bop_util.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/optimization.h"

namespace operations_research {
namespace bop {

SatCoreBasedOptimizer::SatCoreBasedOptimizer(const std::string& name)
    : BopOptimizerBase(name),
      state_update_stamp_(ProblemState::kInitialStampValue),
      initialized_(false),
      assumptions_already_added_(false) {
  // This is in term of number of variables not at their minimal value.
  lower_bound_ = sat::Coefficient(0);
  upper_bound_ = sat::kCoefficientMax;
}

SatCoreBasedOptimizer::~SatCoreBasedOptimizer() {}

BopOptimizerBase::Status SatCoreBasedOptimizer::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  // Note that if the solver is not empty, this only load the newly learned
  // information.
  const BopOptimizerBase::Status status =
      LoadStateProblemToSatSolver(problem_state, &solver_);
  if (status != BopOptimizerBase::CONTINUE) return status;

  if (!initialized_) {
    // Initialize the algorithm.
    nodes_ = sat::CreateInitialEncodingNodes(
        problem_state.original_problem().objective(), &offset_, &repository_);
    initialized_ = true;

    // This is used by the "stratified" approach.
    stratified_lower_bound_ = sat::Coefficient(0);
    for (sat::EncodingNode* n : nodes_) {
      stratified_lower_bound_ = std::max(stratified_lower_bound_, n->weight());
    }
  }

  // Extract the new upper bound.
  if (problem_state.solution().IsFeasible()) {
    upper_bound_ = problem_state.solution().GetCost() + offset_;
  }
  return BopOptimizerBase::CONTINUE;
}

sat::SatSolver::Status SatCoreBasedOptimizer::SolveWithAssumptions() {
  const std::vector<sat::Literal> assumptions =
      sat::ReduceNodesAndExtractAssumptions(upper_bound_,
                                            stratified_lower_bound_,
                                            &lower_bound_, &nodes_, &solver_);

  // The lower bound is proved to equal the upper bound, the upper bound
  // corresponding to the current solution value from the problem_state. As the
  // optimizer is looking for a better solution (see
  // LoadStateProblemToSatSolver), that means the current model is UNSAT and so
  // the synchronized solution is optimal.
  if (assumptions.empty()) return sat::SatSolver::MODEL_UNSAT;
  return solver_.ResetAndSolveWithGivenAssumptions(assumptions);
}

// Only run this if there is an objective.
bool SatCoreBasedOptimizer::ShouldBeRun(
    const ProblemState& problem_state) const {
  return problem_state.original_problem().objective().literals_size() > 0;
}

BopOptimizerBase::Status SatCoreBasedOptimizer::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  const BopOptimizerBase::Status sync_status =
      SynchronizeIfNeeded(problem_state);
  if (sync_status != BopOptimizerBase::CONTINUE) {
    return sync_status;
  }

  int64 conflict_limit = parameters.max_number_of_conflicts_in_random_lns();
  double deterministic_time_at_last_sync = solver_.deterministic_time();
  while (!time_limit->LimitReached()) {
    sat::SatParameters sat_params = solver_.parameters();
    sat_params.set_max_time_in_seconds(time_limit->GetTimeLeft());
    sat_params.set_max_deterministic_time(
        time_limit->GetDeterministicTimeLeft());
    sat_params.set_random_seed(parameters.random_seed());
    sat_params.set_max_number_of_conflicts(conflict_limit);
    solver_.SetParameters(sat_params);

    const int64 old_num_conflicts = solver_.num_failures();
    const sat::SatSolver::Status sat_status =
        assumptions_already_added_ ? solver_.Solve() : SolveWithAssumptions();
    time_limit->AdvanceDeterministicTime(solver_.deterministic_time() -
                                         deterministic_time_at_last_sync);
    deterministic_time_at_last_sync = solver_.deterministic_time();

    assumptions_already_added_ = true;
    conflict_limit -= solver_.num_failures() - old_num_conflicts;
    learned_info->lower_bound = lower_bound_.value() - offset_.value();

    // This is possible because we over-constrain the objective.
    if (sat_status == sat::SatSolver::MODEL_UNSAT) {
      return problem_state.solution().IsFeasible()
                 ? BopOptimizerBase::OPTIMAL_SOLUTION_FOUND
                 : BopOptimizerBase::INFEASIBLE;
    }

    ExtractLearnedInfoFromSatSolver(&solver_, learned_info);
    if (sat_status == sat::SatSolver::LIMIT_REACHED || conflict_limit < 0) {
      return BopOptimizerBase::CONTINUE;
    }
    if (sat_status == sat::SatSolver::MODEL_SAT) {
      stratified_lower_bound_ =
          MaxNodeWeightSmallerThan(nodes_, stratified_lower_bound_);

      // We found a better solution!
      SatAssignmentToBopSolution(solver_.Assignment(), &learned_info->solution);
      if (stratified_lower_bound_ > 0) {
        assumptions_already_added_ = false;
        return BopOptimizerBase::SOLUTION_FOUND;
      }
      return BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
    }

    // The interesting case: we have a core.
    // TODO(user): Check that this cannot fail because of the conflict limit.
    std::vector<sat::Literal> core = solver_.GetLastIncompatibleDecisions();
    sat::MinimizeCore(&solver_, &core);

    const sat::Coefficient min_weight = sat::ComputeCoreMinWeight(nodes_, core);
    sat::ProcessCore(core, min_weight, &repository_, &nodes_, &solver_);
    assumptions_already_added_ = false;
  }
  return BopOptimizerBase::CONTINUE;
}

}  // namespace bop
}  // namespace operations_research
