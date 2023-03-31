// Copyright 2010-2022 Google LLC
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

#include "ortools/sat/feasibility_jump.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/algorithms/binary_search.h"
#include "ortools/base/logging.h"
#include "ortools/sat/constraint_violation.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_utils.h"

namespace operations_research::sat {

void FeasibilityJumpSolver::Initialize(const CpModelProto& proto) {
  is_initialized_ = true;
  evaluator_ = std::make_unique<LsEvaluator>(proto);

  // Note that we ignore evaluator_->ModelIsSupported() assuming that the subset
  // of constraints might be enough for feasibility. We will abort as soon as
  // a solution that is supposed to be feasible do not pass the full model
  // validation.
  const int num_variables = proto.variables().size();
  var_domains_.resize(num_variables);
  for (int var = 0; var < num_variables; ++var) {
    var_domains_[var] = ReadDomainFromProto(proto.variables(var));
  }

  if (type() == SubSolver::FIRST_SOLUTION) {
    // Starts with solution closest to zero.
    // TODO(user): randomize the start instead?
    solution_.resize(num_variables);
    for (int var = 0; var < num_variables; ++var) {
      solution_[var] = var_domains_[var].SmallestValue();
    }

    if (proto.has_objective() &&
        params_.feasibility_jump_start_with_objective()) {
      const int num_terms = proto.objective().vars().size();
      for (int i = 0; i < num_terms; ++i) {
        const int var = proto.objective().vars(i);
        if (proto.objective().coeffs(i) > 0) {
          solution_[var] = var_domains_[var].Min();
        } else {
          solution_[var] = var_domains_[var].Max();
        }
      }
    }
  }

  // Make sure we have access to the transposed data.
  evaluator_->MutableLinearEvaluator()->PrecomputeTransposedView();

  // Starts with weights at one.
  weights_.assign(evaluator_->NumEvaluatorConstraints(), 1.0);
}

std::function<void()> FeasibilityJumpSolver::GenerateTask(int64_t /*task_id*/) {
  task_generated_ = true;  // Atomic.

  return [this] {
    // We delay initialization to the first task as it might be a bit slow
    // to scan the whole model, so we want to do this part in parallel.
    if (!is_initialized_) Initialize(cp_model_);

    // Between chunk, we synchronize bounds.
    const IntegerValue lb = shared_response_->GetInnerObjectiveLowerBound();
    const IntegerValue ub = shared_response_->GetInnerObjectiveUpperBound();
    evaluator_->ReduceObjectiveBounds(lb.value(), ub.value());

    // Update the variable domains with the last information.
    // It is okay to be in O(num_variables) here since we only do that between
    // chunks.
    if (shared_bounds_ != nullptr) {
      shared_bounds_->UpdateDomains(&var_domains_);
    }

    if (type() == SubSolver::INCOMPLETE) {
      // Choose a base solution for this neighborhood.
      const SharedSolutionRepository<int64_t>& repo =
          shared_response_->SolutionsRepository();
      CHECK_GT(repo.NumSolutions(), 0);
      const SharedSolutionRepository<int64_t>::Solution solution =
          repo.GetRandomBiasedSolution(random_);
      solution_.assign(solution.variable_values.begin(),
                       solution.variable_values.end());

      // Reset weights for each new solution.
      weights_.assign(evaluator_->NumEvaluatorConstraints(), 1.0);
    }

    if (DoSomeLinearIterations() && DoSomeGeneralIterations()) {
      // Checks for infeasibility induced by the non supported constraints.
      if (SolutionIsFeasible(cp_model_, solution_)) {
        shared_response_->NewSolution(
            solution_,
            absl::StrCat(
                name(), "(batch #", num_batches_, " #jumps:", num_jumps_,
                " #updates:", num_weight_updates_,
                " #evaluations:", evaluator_->num_deltas_computed(), ")"));
      } else {
        shared_response_->LogMessage(name(), "infeasible solution. Aborting.");
        model_is_supported_ = false;
      }
    }

    task_generated_ = false;  // Atomic.
  };
}

namespace {

// We only consider strictly improving moves in the algorithm
//
// TODO(user): Maybe we can do some move with delta == 0, but it is probably
// better to just update the weight if only such moves are left.
bool IsGood(double delta) { return delta < 0.0; }

}  // namespace

void FeasibilityJumpSolver::RecomputeJump(int var) {
  ++num_computed_jumps_;
  jump_need_recomputation_[var] = false;
  if (var_domains_[var].IsFixed()) {
    jump_values_[var] = solution_[var];
    jump_deltas_[var] = 0.0;
    return;
  }
  LinearIncrementalEvaluator* linear_evaluator =
      evaluator_->MutableLinearEvaluator();

  if (var_domains_[var].Size() == 2) {
    jump_values_[var] = solution_[var] == var_domains_[var].Min()
                            ? var_domains_[var].Max()
                            : var_domains_[var].Min();
    jump_deltas_[var] = linear_evaluator->WeightedViolationDelta(
        weights_, var, jump_values_[var] - solution_[var]);
  } else {
    // In practice, after a few iterations, the chance of finding an improving
    // move is slim, and we can test that fairly easily with at most two
    // queries!
    //
    // Tricky/Annoying: if the value is not in the domain, we returns it.
    const int64_t p1 = var_domains_[var].ValueAtOrBefore(solution_[var] - 1);
    const int64_t p2 = var_domains_[var].ValueAtOrAfter(solution_[var] + 1);

    std::pair<int64_t, double> best_jump;
    const double v1 = var_domains_[var].Contains(p1)
                          ? linear_evaluator->WeightedViolationDelta(
                                weights_, var, p1 - solution_[var])
                          : std::numeric_limits<double>::infinity();
    if (v1 < 0.0) {
      // Point p1 is improving. Look for best before it.
      // Note that we can exclue all point after solution_[var] since it is
      // worse and we assume convexity.
      const Domain dom = var_domains_[var].IntersectionWith(
          Domain(std::numeric_limits<int64_t>::min(), p1 - 1));
      if (dom.IsEmpty()) {
        best_jump = {p1, v1};
      } else {
        tmp_breakpoints_ = evaluator_->SlopeBreakpoints(var, dom);
        best_jump = ConvexMinimum<int64_t, double>(
            /*is_to_the_right=*/true, {p1, v1}, tmp_breakpoints_,
            [var, this, linear_evaluator](int64_t jump_value) {
              return linear_evaluator->WeightedViolationDelta(
                  weights_, var, jump_value - solution_[var]);
            });
      }
    } else {
      const double v2 = var_domains_[var].Contains(p2)
                            ? linear_evaluator->WeightedViolationDelta(
                                  weights_, var, p2 - solution_[var])
                            : std::numeric_limits<double>::infinity();
      if (v2 < 0.0) {
        // Point p2 is improving. Look for best after it.
        // Similarly, we exclude the other points by convexity.
        const Domain dom = var_domains_[var].IntersectionWith(
            Domain(p2 + 1, std::numeric_limits<int64_t>::max()));
        if (dom.IsEmpty()) {
          best_jump = {p2, v2};
        } else {
          tmp_breakpoints_ = evaluator_->SlopeBreakpoints(var, dom);
          best_jump = ConvexMinimum<int64_t, double>(
              /*is_to_the_right=*/false, {p2, v2}, tmp_breakpoints_,
              [var, this, linear_evaluator](int64_t jump_value) {
                return linear_evaluator->WeightedViolationDelta(
                    weights_, var, jump_value - solution_[var]);
              });
        }
      } else {
        // We have no improving point, result is either p1 or p2. This is the
        // most common scenario, and require no breakpoint computation!
        if (v1 < v2) {
          best_jump = {p1, v1};
        } else {
          best_jump = {p2, v2};
        }
      }
    }

    DCHECK_NE(best_jump.first, solution_[var]);
    jump_values_[var] = best_jump.first;
    jump_deltas_[var] = best_jump.second;
  }

  if (IsGood(jump_deltas_[var]) && !in_good_jumps_[var]) {
    in_good_jumps_[var] = true;
    good_jumps_.push_back(var);
  }
}

void FeasibilityJumpSolver::MarkJumpForRecomputation(int var) {
  jump_need_recomputation_[var] = true;

  // This jump might be good, so we need to add it to the queue so it can be
  // evaluated when choosing the next jump.
  if (!in_good_jumps_[var]) {
    in_good_jumps_[var] = true;
    good_jumps_.push_back(var);
  }
}

void FeasibilityJumpSolver::RecomputeAllJumps() {
  const int num_variables = static_cast<int>(var_domains_.size());
  jump_values_.resize(num_variables);
  jump_deltas_.resize(num_variables);
  jump_need_recomputation_.assign(num_variables, true);

  in_good_jumps_.assign(num_variables, false);
  good_jumps_.clear();

  for (int var = 0; var < num_variables; ++var) {
    RecomputeJump(var);
  }
}

bool FeasibilityJumpSolver::UpdateLinearConstraintWeights() {
  ++num_weight_updates_;

  const double kMaxWeight = 1e50;
  const double kBumpFactor = 1.0 / params_.feasibility_jump_decay();

  bump_value_ *= kBumpFactor;
  bool rescale = false;

  int num_bumps = 0;
  last_infeasible_constraints_.clear();
  const int num_linear_constraints = evaluator_->NumLinearConstraints();
  for (int c = 0; c < num_linear_constraints; ++c) {
    if (evaluator_->Violation(c) > 0) {
      ++num_bumps;
      weights_[c] += bump_value_;
      if (weights_[c] > kMaxWeight) rescale = true;
      last_infeasible_constraints_.push_back(c);
    }
  }

  if (rescale) {
    const double factor = 1.0 / kMaxWeight;
    bump_value_ *= factor;
    for (int c = 0; c < num_linear_constraints; ++c) {
      weights_[c] *= factor;
    }
    solution_score_ = evaluator_->WeightedViolation(weights_);
    RecomputeAllJumps();  // Only useful in the linear context.
    return num_bumps > 0;
  }

  // Recompute the affected jumps and overall score.
  // Note that the constraint violations are unaffected.
  solution_score_ = evaluator_->WeightedViolation(weights_);
  for (const int var :
       evaluator_->MutableLinearEvaluator()->ConstraintsToAffectedVariables(
           last_infeasible_constraints_)) {
    MarkJumpForRecomputation(var);
  }

  return num_bumps > 0;
}

// TODO(user): Merge with UpdateConstraintWeights().
bool FeasibilityJumpSolver::UpdateAllConstraintWeights() {
  ++num_weight_updates_;

  const double kMaxWeight = 1e50;
  const double kBumpFactor = 1.0 / params_.feasibility_jump_decay();

  bump_value_ *= kBumpFactor;
  bool rescale = false;

  int num_bumps = 0;
  for (int c = 0; c < weights_.size(); ++c) {
    if (evaluator_->Violation(c) > 0) {
      ++num_bumps;
      weights_[c] += bump_value_;
      if (weights_[c] > kMaxWeight) rescale = true;
    }
  }

  if (rescale) {
    const double factor = 1.0 / kMaxWeight;
    bump_value_ *= factor;
    for (int c = 0; c < weights_.size(); ++c) {
      weights_[c] *= factor;
    }
  }
  solution_score_ = evaluator_->WeightedViolation(weights_);

  return num_bumps > 0;
}

// TODO(user): Update deterministic time.
bool FeasibilityJumpSolver::DoSomeLinearIterations() {
  ++num_batches_;

  // Make sure the solution is within the potentially updated domain.
  for (int var = 0; var < solution_.size(); ++var) {
    solution_[var] = var_domains_[var].ClosestValue(solution_[var]);
  }

  // Recompute at the beginning of each batch.
  evaluator_->ComputeInitialViolations(solution_);
  LinearIncrementalEvaluator* linear_evaluator =
      evaluator_->MutableLinearEvaluator();
  solution_score_ = linear_evaluator->WeightedViolation(weights_);
  RecomputeAllJumps();

  if (VLOG_IS_ON(2) || type() == SubSolver::FIRST_SOLUTION) {
    shared_response_->LogPeriodicMessage(
        name(),
        absl::StrCat("batch #", num_batches_, " score:", solution_score_,
                     " #improving:", good_jumps_.size(),
                     " #inf:", evaluator_->NumInfeasibleConstraints(),
                     " #jumps:", num_jumps_, " #updates:", num_weight_updates_,
                     " #evaluations:", evaluator_->num_deltas_computed()),
        /*frequency_seconds=*/0.2, &last_logging_time_);
  }

  // Do a batch of a given number of loop here.
  // Outer loop: when no more greedy moves, update the weight.
  const int kBatchSize = 10000;
  for (int loop = 0; loop < kBatchSize; ++loop) {
    // Inner loop: greedy descent.
    for (; loop < kBatchSize; ++loop) {
      // Test the shared limit not too often.
      //
      // TODO(user): depending on the size of the problem that might be too
      // little, use deterministic time instead.
      if (loop % 100 == 0) {
        if (shared_time_limit_->LimitReached()) return false;
      }

      // Take the best jump score amongst some random candidates.
      // It is okay if we pick twice the same, we don't really care.
      int best_var = -1;
      double best_delta = 0.0;
      int num_improving_jump_tested = 0;
      while (!good_jumps_.empty() && num_improving_jump_tested < 10) {
        const int index = absl::Uniform<int>(
            random_, 0, static_cast<int>(good_jumps_.size()));
        const int var = good_jumps_[index];

        // We lazily update the jump value.
        if (jump_need_recomputation_[var]) RecomputeJump(var);

        if (!IsGood(jump_deltas_[var])) {
          // Lazily remove.
          in_good_jumps_[var] = false;
          good_jumps_[index] = good_jumps_.back();
          good_jumps_.pop_back();
          continue;
        }

        ++num_improving_jump_tested;
        if (jump_deltas_[var] <= best_delta) {
          best_var = var;
          best_delta = jump_deltas_[var];
        }
      }

      if (good_jumps_.empty()) {
        break;
      }

      const int64_t var_change = jump_values_[best_var] - solution_[best_var];
      DCHECK_EQ(best_delta, linear_evaluator->WeightedViolationDelta(
                                weights_, best_var, var_change))
          << name();

      // Perform the move.
      ++num_jumps_;
      CHECK_NE(best_var, -1);
      solution_[best_var] = jump_values_[best_var];
      solution_score_ += best_delta;

      linear_evaluator->Update(best_var, var_change);
      DCHECK_EQ(solution_score_, evaluator_->WeightedViolation(weights_));

      // For now we recompute all the affected best jump value.
      //
      // TODO(user): In the paper, they just recompute the scores and only
      // change the jump values when the constraint weight change. This should
      // be faster.
      //
      // TODO(user): We can reduce the complexity if we just recompute the
      // score. we don't need to rescan each column of the affected variables
      // again, but could update their scores directly.
      for (const int var :
           evaluator_->MutableLinearEvaluator()->AffectedVariableOnUpdate(
               best_var)) {
        MarkJumpForRecomputation(var);
      }
    }

    // We will update the weight unless there are improving moves left. Note
    // that good_jumps_ might include moves with non-updated delta, so it is why
    // we need this loop here.
    bool update_weights = true;
    for (const int var : good_jumps_) {
      if (jump_deltas_[var] < 0) {
        update_weights = false;
      }
    }
    if (update_weights && !UpdateLinearConstraintWeights()) {
      return true;  // Feasible solution of the linear model.
    }
  }
  return false;
}

bool FeasibilityJumpSolver::DoSomeGeneralIterations() {
  if (evaluator_->NumNonLinearConstraints() == 0) {
    return true;
  }

  // Non linear constraints are not evaluated in the linear phase.
  evaluator_->UpdateNonLinearViolations();
  solution_score_ = evaluator_->WeightedViolation(weights_);

  const int kLoops = 10000;
  std::vector<int> to_scan;
  for (int loop = 0; loop < kLoops; ++loop) {
    to_scan = evaluator_->VariablesInViolatedConstraints();
    std::shuffle(to_scan.begin(), to_scan.end(), random_);
    bool improving_move_found = false;

    for (const int current_var : to_scan) {
      const int64_t initial_value = solution_[current_var];
      int64_t best_value = initial_value;
      double best_delta = 0.0;
      for (const int64_t new_value : var_domains_[current_var].Values()) {
        if (new_value == initial_value) continue;
        const double delta = evaluator_->WeightedViolationDelta(
            weights_, current_var, new_value - initial_value);
        if (delta < best_delta) {
          improving_move_found = true;
          best_value = new_value;
          best_delta = delta;
        }
      }

      if (improving_move_found) {  // Accept the move with the best delta.
        solution_[current_var] = best_value;
        solution_score_ += best_delta;
        evaluator_->UpdateVariableValueAndRecomputeViolations(current_var,
                                                              best_value);
        evaluator_->ComputeInitialViolations(solution_);
        break;
      }
    }

    if (!UpdateAllConstraintWeights()) {
      return true;
    }
  }
  return false;
}

}  // namespace operations_research::sat
