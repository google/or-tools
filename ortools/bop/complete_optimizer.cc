// Copyright 2010-2025 Google LLC
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

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "ortools/bop/bop_base.h"
#include "ortools/bop/bop_parameters.pb.h"
#include "ortools/bop/bop_solution.h"
#include "ortools/bop/bop_util.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/encoding.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/stats.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace bop {

SatCoreBasedOptimizer::SatCoreBasedOptimizer(absl::string_view name)
    : BopOptimizerBase(name),
      model_(std::string(name)),
      sat_solver_(model_.GetOrCreate<sat::SatSolver>()),
      encoder_(&model_),
      state_update_stamp_(ProblemState::kInitialStampValue),
      initialized_(false),
      assumptions_already_added_(false) {
  // This is in term of number of variables not at their minimal value.
  lower_bound_ = sat::Coefficient(0);
  upper_bound_ = sat::kCoefficientMax;
}

SatCoreBasedOptimizer::~SatCoreBasedOptimizer() = default;

BopOptimizerBase::Status SatCoreBasedOptimizer::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  // Note that if the solver is not empty, this only load the newly learned
  // information.
  const BopOptimizerBase::Status status =
      LoadStateProblemToSatSolver(problem_state, sat_solver_);
  if (status != BopOptimizerBase::CONTINUE) return status;

  if (!initialized_) {
    // Initialize the algorithm.
    offset_ = 0;
    const auto objective_proto = problem_state.original_problem().objective();
    for (int i = 0; i < objective_proto.literals_size(); ++i) {
      const sat::Literal literal(objective_proto.literals(i));
      const sat::Coefficient coeff(objective_proto.coefficients(i));

      // We want to maximize the cost when this literal is true.
      if (coeff > 0) {
        encoder_.AddBaseNode(sat::EncodingNode::LiteralNode(literal, coeff));
      } else {
        encoder_.AddBaseNode(
            sat::EncodingNode::LiteralNode(literal.Negated(), -coeff));

        // Note that this increase the offset since the coeff is negative.
        offset_ -= objective_proto.coefficients(i);
      }
    }
    initialized_ = true;

    // This is used by the "stratified" approach.
    stratified_lower_bound_ = sat::Coefficient(0);
    for (const sat::EncodingNode* n : encoder_.nodes()) {
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
  sat::ReduceNodes(upper_bound_, &lower_bound_, encoder_.mutable_nodes(),
                   sat_solver_);
  const std::vector<sat::Literal> assumptions = sat::ExtractAssumptions(
      stratified_lower_bound_, encoder_.nodes(), sat_solver_);
  return sat_solver_->ResetAndSolveWithGivenAssumptions(assumptions);
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

  int64_t conflict_limit = parameters.max_number_of_conflicts_in_random_lns();
  double deterministic_time_at_last_sync = sat_solver_->deterministic_time();
  while (!time_limit->LimitReached()) {
    sat::SatParameters sat_params = sat_solver_->parameters();
    sat_params.set_max_time_in_seconds(time_limit->GetTimeLeft());
    sat_params.set_max_deterministic_time(
        time_limit->GetDeterministicTimeLeft());
    sat_params.set_random_seed(parameters.random_seed());
    sat_params.set_max_number_of_conflicts(conflict_limit);
    sat_solver_->SetParameters(sat_params);

    const int64_t old_num_conflicts = sat_solver_->num_failures();
    const sat::SatSolver::Status sat_status = assumptions_already_added_
                                                  ? sat_solver_->Solve()
                                                  : SolveWithAssumptions();
    time_limit->AdvanceDeterministicTime(sat_solver_->deterministic_time() -
                                         deterministic_time_at_last_sync);
    deterministic_time_at_last_sync = sat_solver_->deterministic_time();

    assumptions_already_added_ = true;
    conflict_limit -= sat_solver_->num_failures() - old_num_conflicts;
    learned_info->lower_bound = lower_bound_.value() - offset_.value();

    // This is possible because we over-constrain the objective.
    if (sat_status == sat::SatSolver::INFEASIBLE) {
      return problem_state.solution().IsFeasible()
                 ? BopOptimizerBase::OPTIMAL_SOLUTION_FOUND
                 : BopOptimizerBase::INFEASIBLE;
    }

    ExtractLearnedInfoFromSatSolver(sat_solver_, learned_info);
    if (sat_status == sat::SatSolver::LIMIT_REACHED || conflict_limit < 0) {
      return BopOptimizerBase::CONTINUE;
    }
    if (sat_status == sat::SatSolver::FEASIBLE) {
      stratified_lower_bound_ =
          MaxNodeWeightSmallerThan(encoder_.nodes(), stratified_lower_bound_);

      // We found a better solution!
      SatAssignmentToBopSolution(sat_solver_->Assignment(),
                                 &learned_info->solution);
      if (stratified_lower_bound_ > 0) {
        assumptions_already_added_ = false;
        return BopOptimizerBase::SOLUTION_FOUND;
      }
      return BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
    }

    // The interesting case: we have a core.
    // TODO(user): Check that this cannot fail because of the conflict limit.
    std::vector<sat::Literal> core =
        sat_solver_->GetLastIncompatibleDecisions();
    sat::MinimizeCore(sat_solver_, &core);

    const sat::Coefficient min_weight =
        sat::ComputeCoreMinWeight(encoder_.nodes(), core);
    std::string info_str;
    encoder_.ProcessCore(
        core, min_weight,
        /*gap=*/std::numeric_limits<sat::Coefficient::ValueType>::max(),
        &info_str);
    assumptions_already_added_ = false;
  }
  return BopOptimizerBase::CONTINUE;
}

}  // namespace bop
}  // namespace operations_research
