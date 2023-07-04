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
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/algorithms/binary_search.h"
#include "ortools/base/logging.h"
#include "ortools/sat/constraint_violation.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_model.h"
#include "ortools/sat/restart.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

namespace operations_research::sat {

FeasibilityJumpSolver::~FeasibilityJumpSolver() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"fs_jump/num_general_moves_computed", num_general_evals_});
  stats.push_back({"fs_jump/num_general_moves_done", num_general_moves_});
  stats.push_back({"fs_jump/num_linear_moves_computed", num_linear_evals_});
  stats.push_back({"fs_jump/num_linear_moves_repaired_with_full_scan",
                   num_repairs_with_full_scan_});
  stats.push_back({"fs_jump/num_linear_moves_done", num_linear_moves_});
  stats.push_back({"fs_jump/num_perbubations_applied", num_perturbations_});
  stats.push_back({"fs_jump/num_solutions_imported", num_solutions_imported_});
  stats.push_back(
      {"fs_jump/num_variables_partially_scanned", num_partial_scans_});
  stats.push_back({"fs_jump/num_weight_updates", num_weight_updates_});
  shared_stats_->AddStats(stats);
}

void FeasibilityJumpSolver::Initialize() {
  is_initialized_ = true;
  evaluator_ = std::make_unique<LsEvaluator>(
      linear_model_->model_proto(), linear_model_->ignored_constraints(),
      linear_model_->additional_constraints());

  // Note that we ignore evaluator_->ModelIsSupported() assuming that the subset
  // of constraints might be enough for feasibility. We will abort as soon as
  // a solution that is supposed to be feasible do not pass the full model
  // validation.
  const int num_variables = linear_model_->model_proto().variables().size();
  var_domains_.resize(num_variables);
  var_has_two_values_.resize(num_variables);
  for (int var = 0; var < num_variables; ++var) {
    var_domains_[var] =
        ReadDomainFromProto(linear_model_->model_proto().variables(var));
    var_has_two_values_[var] = var_domains_[var].HasTwoValues();
  }
}

namespace {

int64_t ComputeRange(int64_t range, double range_ratio) {
  return static_cast<int64_t>(
      std::ceil(static_cast<double>(range) * range_ratio));
}

// TODO(user): Optimize and move to the Domain class.
// TODO(user): Improve entropy on non continuous domains.
int64_t RandomValueNearMin(const Domain& domain, double range_ratio,
                           absl::BitGenRef random) {
  if (domain.Size() == 1) return domain.FixedValue();
  if (domain.Size() == 2) {
    return absl::Bernoulli(random, 1 - range_ratio) ? domain.Min()
                                                    : domain.Max();
  }
  const int64_t range = ComputeRange(domain.Max() - domain.Min(), range_ratio);
  return domain.ValueAtOrBefore(domain.Min() +
                                absl::LogUniform<int64_t>(random, 0, range));
}

int64_t RandomValueNearMax(const Domain& domain, double range_ratio,
                           absl::BitGenRef random) {
  if (domain.Size() == 1) return domain.FixedValue();
  if (domain.Size() == 2) {
    return absl::Bernoulli(random, 1 - range_ratio) ? domain.Max()
                                                    : domain.Min();
  }
  const int64_t range = ComputeRange(domain.Max() - domain.Min(), range_ratio);
  return domain.ValueAtOrAfter(domain.Max() -
                               absl::LogUniform<int64_t>(random, 0, range));
}

int64_t RandomValueNearValue(const Domain& domain, int64_t value,
                             double range_ratio, absl::BitGenRef random) {
  DCHECK(!domain.IsFixed());

  if (domain.Min() >= value) {
    return RandomValueNearMin(domain, range_ratio, random);
  }

  if (domain.Max() <= value) {
    return RandomValueNearMax(domain, range_ratio, random);
  }

  // Split up or down, and choose value in split domain.
  const Domain greater_domain =
      domain.IntersectionWith({value + 1, domain.Max()});
  const double choose_greater_probability =
      static_cast<double>(greater_domain.Size()) /
      static_cast<double>(domain.Size() - 1);
  if (absl::Bernoulli(random, choose_greater_probability)) {
    return RandomValueNearMin(greater_domain, range_ratio, random);
  } else {
    return RandomValueNearMax(
        domain.IntersectionWith({domain.Min(), value - 1}), range_ratio,
        random);
  }
}

}  // namespace

void FeasibilityJumpSolver::ResetCurrentSolution() {
  const int num_variables = linear_model_->model_proto().variables().size();
  const double default_value_probability =
      1.0 - params_.feasibility_jump_var_randomization_probability();
  const double range_ratio =
      params_.feasibility_jump_var_perburbation_range_ratio();
  std::vector<int64_t>& solution = *evaluator_->mutable_current_solution();

  // Resize the solution if needed.
  solution.resize(num_variables);

  // Starts with values closest to zero.
  for (int var = 0; var < num_variables; ++var) {
    if (var_domains_[var].IsFixed()) {
      solution[var] = var_domains_[var].FixedValue();
      continue;
    }

    if (num_batches_ == 0 ||
        absl::Bernoulli(random_, default_value_probability)) {
      solution[var] = var_domains_[var].SmallestValue();
    } else {
      solution[var] =
          RandomValueNearValue(var_domains_[var], 0, range_ratio, random_);
    }
  }

  // Use objective half of the time (if the model has one).
  if (linear_model_->model_proto().has_objective() &&
      absl::Bernoulli(random_, 0.5)) {
    const int num_terms =
        linear_model_->model_proto().objective().vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = linear_model_->model_proto().objective().vars(i);
      if (var_domains_[var].IsFixed()) continue;

      if (linear_model_->model_proto().objective().coeffs(i) > 0) {
        if (num_batches_ == 0 ||
            absl::Bernoulli(random_, default_value_probability)) {
          solution[var] = var_domains_[var].Min();
        } else {
          solution[var] =
              RandomValueNearMin(var_domains_[var], range_ratio, random_);
        }
      } else {
        if (num_batches_ == 0 ||
            absl::Bernoulli(random_, default_value_probability)) {
          solution[var] = var_domains_[var].Max();
        } else {
          solution[var] =
              RandomValueNearMax(var_domains_[var], range_ratio, random_);
        }
      }
    }
  }

  // Starts with weights at one.
  weights_.assign(evaluator_->NumEvaluatorConstraints(), 1.0);
}

void FeasibilityJumpSolver::PerturbateCurrentSolution() {
  const int num_variables = linear_model_->model_proto().variables().size();
  const double pertubation_probability =
      params_.feasibility_jump_var_randomization_probability();
  const double perturbation_ratio =
      params_.feasibility_jump_var_perburbation_range_ratio();
  std::vector<int64_t>& solution = *evaluator_->mutable_current_solution();
  for (int var = 0; var < num_variables; ++var) {
    if (var_domains_[var].IsFixed()) continue;
    if (absl::Bernoulli(random_, pertubation_probability)) {
      solution[var] = RandomValueNearValue(var_domains_[var], solution[var],
                                           perturbation_ratio, random_);
    }
  }
}

std::string FeasibilityJumpSolver::OneLineStats() const {
  // Restarts, perturbations, and solutions imported.
  std::string restart_str;
  if (num_restarts_ > 1) {
    absl::StrAppend(&restart_str, " #restarts:", num_restarts_ - 1);
  }
  if (num_solutions_imported_ > 0) {
    absl::StrAppend(&restart_str,
                    " #solutions_imported:", num_solutions_imported_);
  }
  if (num_perturbations_ > 0) {
    absl::StrAppend(&restart_str, " #perturbations:", num_perturbations_);
  }

  // Moves and evaluations in the general iterations.
  const std::string general_str =
      num_general_evals_ == 0 && num_general_moves_ == 0
          ? ""
          : absl::StrCat(" #gen_moves:", FormatCounter(num_general_moves_), "/",
                         FormatCounter(num_general_evals_));

  // Improving jumps and infeasible constraints.
  const int num_infeasible_cts = evaluator_->NumInfeasibleConstraints();
  const std::string non_solution_str =
      num_infeasible_cts == 0
          ? ""
          : absl::StrCat(" #good_lin_moves:", FormatCounter(good_jumps_.size()),
                         " #inf_cts:",
                         FormatCounter(evaluator_->NumInfeasibleConstraints()));

  return absl::StrCat("batch:", num_batches_, restart_str,
                      " #lin_moves:", FormatCounter(num_linear_moves_), "/",
                      FormatCounter(num_linear_evals_), general_str,
                      non_solution_str,
                      " #weight_updates:", FormatCounter(num_weight_updates_));
}

std::function<void()> FeasibilityJumpSolver::GenerateTask(int64_t /*task_id*/) {
  task_generated_ = true;  // Atomic.

  return [this] {
    // We delay initialization to the first task as it might be a bit slow
    // to scan the whole model, so we want to do this part in parallel.
    if (!is_initialized_) Initialize();

    bool should_recompute_violations = false;
    bool reset_weights = false;

    // In incomplete mode, query the starting solution for the shared response
    // manager.
    if (type() == SubSolver::INCOMPLETE) {
      // Choose a base solution for this neighborhood.
      const SharedSolutionRepository<int64_t>& repo =
          shared_response_->SolutionsRepository();
      CHECK_GT(repo.NumSolutions(), 0);
      const SharedSolutionRepository<int64_t>::Solution solution =
          repo.GetRandomBiasedSolution(random_);
      if (solution.rank < last_solution_rank_) {
        evaluator_->OverwriteCurrentSolution(solution.variable_values);
        should_recompute_violations = true;
        reset_weights = true;

        // Update last solution rank.
        last_solution_rank_ = solution.rank;
        VLOG(2) << name() << " import a solution with value " << solution.rank;
        ++num_solutions_imported_;
        num_batches_before_perturbation_ =
            params_.violation_ls_perturbation_period();
      } else if (num_batches_before_perturbation_ <= 0) {
        // TODO(user): Tune the improvement constant, maybe use luby.
        num_batches_before_perturbation_ =
            params_.violation_ls_perturbation_period();
        ++num_perturbations_;
        PerturbateCurrentSolution();
        should_recompute_violations = true;
        reset_weights = true;
      }
    } else {
      // Restart?  Note that we always "restart" the first time.
      const double dtime =
          evaluator_->MutableLinearEvaluator()->DeterministicTime();
      if (dtime >= dtime_restart_threshold_ &&
          num_weight_updates_ >= update_restart_threshold_) {
        if (num_restarts_ == 0 || params_.feasibility_jump_enable_restarts()) {
          ++num_restarts_;
          ResetCurrentSolution();
          should_recompute_violations = true;
          reset_weights = true;
        } else if (params_.feasibility_jump_var_randomization_probability() >
                   0.0) {
          PerturbateCurrentSolution();
          ++num_perturbations_;
          should_recompute_violations = true;
          reset_weights = true;
        }

        // We use luby restart with a base of 1 deterministic unit.
        // We also block the restart if there was not enough weight update.
        // Note that we only restart between batches too.
        //
        // TODO(user): Ideally batch should use deterministic time too so we
        // can just use number of batch for the luby restart.
        // TODO(user): Maybe have one worker with very low restart
        // rate.
        dtime_restart_threshold_ =
            dtime + 1.0 * SUniv(num_restarts_ + num_perturbations_);
        update_restart_threshold_ = num_weight_updates_ + 10;
      }
    }

    // Between chunk, we synchronize bounds.
    if (linear_model_->model_proto().has_objective()) {
      const IntegerValue lb = shared_response_->GetInnerObjectiveLowerBound();
      const IntegerValue ub = shared_response_->GetInnerObjectiveUpperBound();
      if (ub < lb) return;  // Search is finished.
      if (evaluator_->ReduceObjectiveBounds(lb.value(), ub.value())) {
        should_recompute_violations = true;
      }
    }

    // Update the variable domains with the last information.
    // It is okay to be in O(num_variables) here since we only do that between
    // chunks.
    if (shared_bounds_ != nullptr) {
      shared_bounds_->UpdateDomains(&var_domains_);
      for (int var = 0; var < var_domains_.size(); ++var) {
        // We abort if the problem is trivially UNSAT. This might happen while
        // we are cleaning up all workers at the end of a search.
        if (var_domains_[var].IsEmpty()) return;
        var_has_two_values_[var] = var_domains_[var].HasTwoValues();
      }
    }

    // Checks the current solution is compatible with updated domains.
    {
      // Make sure the solution is within the potentially updated domain.
      std::vector<int64_t>& current_solution =
          *evaluator_->mutable_current_solution();
      for (int var = 0; var < current_solution.size(); ++var) {
        const int64_t old_value = current_solution[var];
        const int64_t new_value = var_domains_[var].ClosestValue(old_value);
        if (new_value != old_value) {
          current_solution[var] = new_value;
          should_recompute_violations = true;
        }
      }
    }

    if (should_recompute_violations) {
      evaluator_->ComputeAllViolations();
    }
    if (reset_weights) {
      weights_.assign(evaluator_->NumEvaluatorConstraints(), 1.0);
    }

    // Search for feasible solution.
    if (DoSomeLinearIterations() && DoSomeGeneralIterations()) {
      // Checks for infeasibility induced by the non supported constraints.
      if (SolutionIsFeasible(linear_model_->model_proto(),
                             evaluator_->current_solution())) {
        shared_response_->NewSolution(
            evaluator_->current_solution(),
            absl::StrCat(name(), "(", OneLineStats(), ")"));
        num_batches_before_perturbation_ =
            params_.violation_ls_perturbation_period();
      } else {
        shared_response_->LogMessage(name(), "infeasible solution. Aborting.");
        model_is_supported_ = false;
      }
    } else {
      --num_batches_before_perturbation_;
    }

    // Update dtime.
    // Since we execute only one task at the time, this is safe.
    {
      const double dtime =
          evaluator_->MutableLinearEvaluator()->DeterministicTime();
      const double delta = dtime - deterministic_time();
      AddTaskDeterministicDuration(delta);
      shared_time_limit_->AdvanceDeterministicTime(delta);
    }

    task_generated_ = false;  // Atomic.
  };
}

bool FeasibilityJumpSolver::IsGood(int var) const {
  if (jump_scores_[var] < 0.0) return true;
  if (jump_scores_[var] > 0.0) return false;
  const int64_t delta = jump_values_[var] - evaluator_->current_solution()[var];
  return evaluator_->ObjectiveDelta(var, delta) < 0;
}

void FeasibilityJumpSolver::RecomputeJump(int var) {
  const std::vector<int64_t>& solution = evaluator_->current_solution();
  ++num_linear_evals_;
  jump_need_recomputation_[var] = false;
  if (var_domains_[var].IsFixed()) {
    jump_values_[var] = solution[var];
    jump_scores_[var] = 0.0;
    return;
  }
  LinearIncrementalEvaluator* linear_evaluator =
      evaluator_->MutableLinearEvaluator();

  if (var_has_two_values_[var]) {
    jump_values_[var] = solution[var] == var_domains_[var].Min()
                            ? var_domains_[var].Max()
                            : var_domains_[var].Min();
    jump_scores_[var] = linear_evaluator->WeightedViolationDelta(
        weights_, var, jump_values_[var] - solution[var]);
  } else {
    // In practice, after a few iterations, the chance of finding an improving
    // move is slim, and we can test that fairly easily with at most two
    // queries!
    //
    // Tricky/Annoying: if the value is not in the domain, we returns it.
    const int64_t p1 = var_domains_[var].ValueAtOrBefore(solution[var] - 1);
    const int64_t p2 = var_domains_[var].ValueAtOrAfter(solution[var] + 1);

    std::pair<int64_t, double> best_jump;
    const double v1 = var_domains_[var].Contains(p1)
                          ? linear_evaluator->WeightedViolationDelta(
                                weights_, var, p1 - solution[var])
                          : std::numeric_limits<double>::infinity();
    if (v1 < 0.0) {
      // Point p1 is improving. Look for best before it.
      // Note that we can exclude all point after solution[var] since it is
      // worse and we assume convexity.
      const Domain dom = var_domains_[var].IntersectionWith(
          Domain(std::numeric_limits<int64_t>::min(), p1 - 1));
      if (dom.IsEmpty()) {
        best_jump = {p1, v1};
      } else {
        tmp_breakpoints_ =
            linear_evaluator->SlopeBreakpoints(var, solution[var], dom);
        best_jump = ConvexMinimum<int64_t, double>(
            /*is_to_the_right=*/true, {p1, v1}, tmp_breakpoints_,
            [var, this, linear_evaluator, &solution](int64_t jump_value) {
              return linear_evaluator->WeightedViolationDelta(
                  weights_, var, jump_value - solution[var]);
            });
      }
    } else {
      const double v2 = var_domains_[var].Contains(p2)
                            ? linear_evaluator->WeightedViolationDelta(
                                  weights_, var, p2 - solution[var])
                            : std::numeric_limits<double>::infinity();
      if (v2 < 0.0) {
        // Point p2 is improving. Look for best after it.
        // Similarly, we exclude the other points by convexity.
        const Domain dom = var_domains_[var].IntersectionWith(
            Domain(p2 + 1, std::numeric_limits<int64_t>::max()));
        if (dom.IsEmpty()) {
          best_jump = {p2, v2};
        } else {
          tmp_breakpoints_ =
              linear_evaluator->SlopeBreakpoints(var, solution[var], dom);
          best_jump = ConvexMinimum<int64_t, double>(
              /*is_to_the_right=*/false, {p2, v2}, tmp_breakpoints_,
              [var, this, linear_evaluator, &solution](int64_t jump_value) {
                return linear_evaluator->WeightedViolationDelta(
                    weights_, var, jump_value - solution[var]);
              });
        }
      } else {
        // We have no improving point, result is either p1 or p2. This is the
        // most common scenario, and require no breakpoint computation!
        // Choose the direction which increases violation the least,
        // disambiguating by best objective.
        if (v1 < v2 || (v1 == v2 && evaluator_->ObjectiveDelta(
                                        var, p1 - solution[var]) < 0)) {
          best_jump = {p1, v1};
        } else {
          best_jump = {p2, v2};
        }
      }
    }

    DCHECK_NE(best_jump.first, solution[var]);
    jump_values_[var] = best_jump.first;
    jump_scores_[var] = best_jump.second;
  }

  if (IsGood(var) && !in_good_jumps_[var]) {
    in_good_jumps_[var] = true;
    good_jumps_.push_back(var);
  }
}

void FeasibilityJumpSolver::RecomputeAllJumps() {
  const int num_variables = static_cast<int>(var_domains_.size());
  jump_values_.resize(num_variables);
  jump_scores_.resize(num_variables);
  jump_need_recomputation_.assign(num_variables, true);

  in_good_jumps_.assign(num_variables, false);
  good_jumps_.clear();

  for (int var = 0; var < num_variables; ++var) {
    RecomputeJump(var);
  }
}

int FeasibilityJumpSolver::UpdateConstraintWeights(bool linear_mode) {
  ++num_weight_updates_;

  const double kMaxWeight = 1e50;
  const double kBumpFactor = 1.0 / params_.feasibility_jump_decay();
  LinearIncrementalEvaluator* linear_evaluator =
      evaluator_->MutableLinearEvaluator();

  bump_value_ *= kBumpFactor;
  bool rescale = false;

  int num_bumps = 0;
  const int num_constraints = linear_mode
                                  ? evaluator_->NumLinearConstraints()
                                  : evaluator_->NumEvaluatorConstraints();
  linear_evaluator->ClearAffectedVariables();
  for (int c = 0; c < num_constraints; ++c) {
    if (!evaluator_->IsViolated(c)) continue;

    ++num_bumps;
    weights_[c] += bump_value_;
    if (weights_[c] > kMaxWeight) rescale = true;
    if (!rescale) {
      linear_evaluator->UpdateScoreOnWeightUpdate(
          c, bump_value_, evaluator_->current_solution(), jump_values_,
          absl::MakeSpan(jump_scores_));
    }
  }

  if (rescale) {
    const double factor = 1.0 / kMaxWeight;
    bump_value_ *= factor;
    for (int c = 0; c < num_constraints; ++c) {
      weights_[c] *= factor;
    }
    RecomputeAllJumps();
    return num_bumps;
  }

  // Recompute the affected jumps.
  // Note that the constraint violations are unaffected.
  for (const int var : linear_evaluator->VariablesAffectedByLastUpdate()) {
    // We don't need to recompute score of binary variable, it should
    // already be correct.
    if (!jump_need_recomputation_[var] && var_has_two_values_[var]) {
      DCHECK(JumpIsUpToDate(var));
      if (IsGood(var) && !in_good_jumps_[var]) {
        in_good_jumps_[var] = true;
        good_jumps_.push_back(var);
      }
      continue;
    }

    // This jump might be good, so we need to add it to the queue so it can be
    // evaluated when choosing the next jump.
    jump_need_recomputation_[var] = true;
    if (!in_good_jumps_[var]) {
      in_good_jumps_[var] = true;
      good_jumps_.push_back(var);
    }
  }

  return num_bumps;
}

// Important: This is for debugging, but unfortunately it currently change the
// deterministic time and hence the overall algo behavior.
bool FeasibilityJumpSolver::JumpIsUpToDate(int var) {
  const int64_t old_jump = jump_values_[var];
  const double old_score = jump_scores_[var];
  RecomputeJump(var);
  CHECK_EQ(jump_values_[var], old_jump);  // No change
  const double relative =
      std::max({std::abs(jump_scores_[var]), std::abs(old_score), 1.0});
  return std::abs(jump_scores_[var] - old_score) / relative < 1e-6;
}

bool FeasibilityJumpSolver::DoSomeLinearIterations() {
  ++num_batches_;

  // Recompute at the beginning of each batch.
  LinearIncrementalEvaluator* linear_evaluator =
      evaluator_->MutableLinearEvaluator();
  const std::vector<int64_t>& solution = evaluator_->current_solution();
  RecomputeAllJumps();

  if (VLOG_IS_ON(1)) {
    if (num_batches_ < 10) {
      // The first few batches are more informative to understand how the
      // heuristic behaves.
      shared_response_->LogMessage(name(), OneLineStats());
    } else {
      shared_response_->LogPeriodicMessage(name(), OneLineStats(),
                                           /*frequency_seconds=*/1.0,
                                           &last_logging_time_);
    }
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
        if (shared_time_limit_->LimitReached()) {
          return false;
        }
      }

      // Take the best jump score amongst some random candidates.
      // It is okay if we pick twice the same, we don't really care.
      int best_var = -1;
      int best_index = -1;
      int64_t best_value = 0;
      double best_score = 0.0;
      int64_t best_obj_delta = 0;
      int num_improving_jump_tested = 0;
      while (!good_jumps_.empty() && num_improving_jump_tested < 5) {
        const int index = absl::Uniform<int>(
            random_, 0, static_cast<int>(good_jumps_.size()));
        const int var = good_jumps_[index];

        // We lazily update the jump value.
        if (jump_need_recomputation_[var]) {
          RecomputeJump(var);
        } else {
          DCHECK(JumpIsUpToDate(var));
        }

        if (!IsGood(var)) {
          // Lazily remove.
          in_good_jumps_[var] = false;
          good_jumps_[index] = good_jumps_.back();
          good_jumps_.pop_back();
          if (best_index == good_jumps_.size()) best_index = index;
          continue;
        }

        ++num_improving_jump_tested;
        const int64_t obj_delta = 0;
        // evaluator_->ObjectiveDelta(var, jump_values_[var] - solution[var]);
        if (std::make_pair(jump_scores_[var], obj_delta) <
            std::make_pair(best_score, best_obj_delta)) {
          best_var = var;
          best_index = index;
          best_value = jump_values_[var];
          best_score = jump_scores_[var];
          best_obj_delta = obj_delta;
        }
      }

      if (good_jumps_.empty()) {
        // TODO(user): Re-enable the code. It can be a bit slow currently
        // especially since in many situations it doesn't achieve anything.
        if (true) break;

        bool time_limit_crossed = false;
        if (ScanAllVariables(solution, /*linear_mode=*/true, &best_var,
                             &best_value, &best_score, &time_limit_crossed)) {
          VLOG(2) << name()
                  << " scans and finds improving solution (var:" << best_var
                  << " value:" << best_value << ", delta:" << best_score << ")";
          ++num_repairs_with_full_scan_;
        } else if (time_limit_crossed) {
          return false;
        } else {
          break;
        }
      } else {
        const int64_t var_change = best_value - solution[best_var];
        DCHECK_EQ(best_score, linear_evaluator->WeightedViolationDelta(
                                  weights_, best_var, var_change));
      }

      CHECK_NE(best_var, -1);
      CHECK_NE(best_index, -1);

      // Perform the move.
      ++num_linear_moves_;
      const int64_t old_value = evaluator_->current_solution()[best_var];
      linear_evaluator->ClearAffectedVariables();
      evaluator_->UpdateLinearScores(best_var, best_value, weights_,
                                     jump_values_,
                                     absl::MakeSpan(jump_scores_));
      evaluator_->UpdateVariableValue(best_var, best_value);

      // We already know the score of undoing the move we just did, and we know
      // this move is bad, so we can remove it from good_jumps_ right away.
      jump_values_[best_var] = old_value;
      jump_scores_[best_var] = -best_score;
      if (var_has_two_values_[best_var]) {
        CHECK_EQ(good_jumps_[best_index], best_var);
        in_good_jumps_[best_var] = false;
        good_jumps_[best_index] = good_jumps_.back();
        good_jumps_.pop_back();
      } else {
        jump_need_recomputation_[best_var] = true;
      }
      MarkJumpsThatNeedsToBeRecomputed(best_var);
    }

    // We will update the weight unless the queue is non-empty.
    if (good_jumps_.empty() &&
        UpdateConstraintWeights(/*linear_mode=*/true) == 0) {
      return true;  // Feasible solution of the linear model.
    }
  }
  return false;
}

// Update the jump scores.
//
// We incrementally maintain the score (except for best_var).
// However for non-Boolean, we still need to recompute the jump value.
// We will do that in a lazy fashion.
//
// TODO(user): In the paper, they just recompute the scores and only
// change the jump values when the constraint weight changes. Experiment?
// Note however that the current code is quite fast.
//
// TODO(user): For non-Boolean, we could easily detect if a non-improving
// score cannot become improving. We don't need to add such variable to
// the queue.
void FeasibilityJumpSolver::MarkJumpsThatNeedsToBeRecomputed(int changed_var) {
  LinearIncrementalEvaluator* linear_evaluator =
      evaluator_->MutableLinearEvaluator();
  for (const int var : linear_evaluator->VariablesAffectedByLastUpdate()) {
    if (var == changed_var) continue;
    if (jump_need_recomputation_[var]) {
      DCHECK(in_good_jumps_[var]);
      continue;
    }

    // We don't need to recompute score of binary variable, it should
    // already be correct.
    if (var_has_two_values_[var]) {
      DCHECK(JumpIsUpToDate(var));
      if (IsGood(var) && !in_good_jumps_[var]) {
        in_good_jumps_[var] = true;
        good_jumps_.push_back(var);
      }
      continue;
    }

    jump_need_recomputation_[var] = true;
    if (!in_good_jumps_[var]) {
      in_good_jumps_[var] = true;
      good_jumps_.push_back(var);
    }
  }
}

bool FeasibilityJumpSolver::DoSomeGeneralIterations() {
  if (evaluator_->NumNonLinearConstraints() == 0) {
    return true;
  }

  LinearIncrementalEvaluator* linear_evaluator =
      evaluator_->MutableLinearEvaluator();
  const std::vector<int64_t>& solution = evaluator_->current_solution();

  // Non-linear constraints are not evaluated in the linear phase.
  evaluator_->UpdateAllNonLinearViolations();

  const int kLoops = 10000;
  for (int loop = 0; loop < kLoops; ++loop) {
    int var;
    int64_t value;
    double score;
    bool time_limit_crossed = false;
    if (ScanAllVariables(solution, /*linear_mode=*/false, &var, &value, &score,
                         &time_limit_crossed)) {
      // Perform the move.
      num_general_moves_++;

      // Update the linear part.
      linear_evaluator->ClearAffectedVariables();
      evaluator_->UpdateLinearScores(var, value, weights_, jump_values_,
                                     absl::MakeSpan(jump_scores_));
      jump_values_[var] = solution[var];
      jump_scores_[var] = -score;
      for (const int v : linear_evaluator->VariablesAffectedByLastUpdate()) {
        if (!var_has_two_values_[var]) {
          jump_need_recomputation_[v] = true;
        }
      }

      // Update the non-linear part. Note it also commits the move.
      evaluator_->UpdateNonLinearViolations(var, value);
      evaluator_->UpdateVariableValue(var, value);
      continue;
    } else if (time_limit_crossed) {
      return false;
    }
    if (UpdateConstraintWeights(/*linear_mode=*/false) == 0) {
      return true;
    }
  }
  return false;
}

bool FeasibilityJumpSolver::ScanAllVariables(absl::Span<const int64_t> solution,
                                             bool linear_mode,
                                             int* improving_var,
                                             int64_t* improving_value,
                                             double* improving_delta,
                                             bool* time_limit_crossed) {
  LinearIncrementalEvaluator* linear_evaluator =
      evaluator_->MutableLinearEvaluator();

  // We stop at the first improving move that does not degrade the linear score.
  // Otherwise, we return the best improving move.
  tmp_to_scan_ = evaluator_->VariablesInViolatedConstraints();
  std::shuffle(tmp_to_scan_.begin(), tmp_to_scan_.end(), random_);

  // We maintain the best solution found, regardless of the linear violations.
  int best_var = -1;
  int64_t best_value = 0;
  double best_delta = -1e-4;
  // We will favor moves that keep the linear assignment feasible for as long as
  // possible (if feasible to starts with).
  //
  // TODO(user): We just need to check if the assignment is linear
  // feasible, not compute its score.
  const bool accept_moves_with_linear_violation =
      !params_.feasibility_jump_protect_linear_feasibility() ||
      linear_evaluator->WeightedViolation(weights_) > 0.0;

  for (const int var : tmp_to_scan_) {
    if (evaluator_->VariableOnlyInLinearConstraintWithConvexViolationChange(
            var)) {
      // We lazily update the jump value.
      if (jump_need_recomputation_[var]) {
        RecomputeJump(var);
      } else {
        DCHECK(JumpIsUpToDate(var));
      }

      const double delta = jump_scores_[var];
      if (IsGood(var)) {
        *improving_var = var;
        *improving_value = jump_values_[var];
        *improving_delta = delta;
        return true;
      }
      continue;
    }

    // Compute the values to scan.
    // If the domain is large we do not scan the full variable domain.
    Domain scan_range;
    const int64_t initial_value = solution[var];
    const Domain& var_domain = var_domains_[var];
    if (var_domains_[var].Size() <=
        params_.feasibility_jump_max_num_values_scanned()) {
      scan_range = var_domain;
    } else {
      // TODO(user): Partial scan of the rest of the domain.
      ++num_partial_scans_;
      const int64_t half_span =
          params_.feasibility_jump_max_num_values_scanned() / 2;
      Domain scan_range =
          Domain(initial_value).AdditionWith({-half_span, half_span});
      if (scan_range.Min() < var_domain.Min()) {
        scan_range = scan_range.AdditionWith(
            Domain(var_domain.Min() - scan_range.Min()));
      } else if (scan_range.Max() > var_domain.Max()) {
        scan_range = scan_range.AdditionWith(
            Domain(scan_range.Max() - var_domain.Max()));
      }
      scan_range = scan_range.IntersectionWith(var_domain);
    }

    // Take the best amongst all the values to scan for the current variable if
    // it does not degrade the linear score (when
    // accept_moves_with_linear_violation is false).
    int best_var_non_increasing_linear = -1;
    int64_t best_value_non_increasing_linear = 0;
    double best_delta_non_increasing_linear = -1e-4;

    for (const int64_t new_value : scan_range.Values()) {
      if (new_value == initial_value) continue;

      // Checks the time limit periodically.
      if (++num_general_evals_ % 1000 == 0 && shared_time_limit_ != nullptr &&
          shared_time_limit_->LimitReached()) {
        *time_limit_crossed = true;
        return false;
      }

      // Computes both linear and general delta.
      const double linear_delta = linear_evaluator->WeightedViolationDelta(
          weights_, var, new_value - initial_value);
      const double non_linear_delta =
          linear_mode ? 0.0
                      : evaluator_->WeightedNonLinearViolationDelta(
                            weights_, var, new_value - initial_value);
      const double delta = linear_delta + non_linear_delta;

      // Do we have a valid improving move ? We select the best value for the
      // first variable found with such a move so we delay the exit until after
      // the loop on values is finished.
      if ((accept_moves_with_linear_violation || linear_delta <= 0.0) &&
          delta < best_delta_non_increasing_linear) {
        best_var_non_increasing_linear = var;
        best_value_non_increasing_linear = new_value;
        best_delta_non_increasing_linear = delta;
      }

      // We keep the best move so far.
      if (delta < best_delta) {
        best_var = var;
        best_value = new_value;
        best_delta = delta;
      }
    }  // End scan values.

    // Early accept the best improving move that does not degrade the linear
    // score, or the best move if accept_moves_with_linear_violation is true.
    if (best_var_non_increasing_linear != -1) {
      *improving_var = best_var_non_increasing_linear;
      *improving_value = best_value_non_increasing_linear;
      *improving_delta = best_delta_non_increasing_linear;
      return true;
    }
  }

  // Accept the best improving move.
  if (best_var != -1) {
    *improving_var = best_var;
    *improving_value = best_value;
    *improving_delta = best_delta;
    return true;
  }

  // No solution found.
  return false;
}

}  // namespace operations_research::sat
