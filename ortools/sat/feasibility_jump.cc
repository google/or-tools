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
#include <tuple>
#include <utility>
#include <vector>

#include "absl/functional/function_ref.h"
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

namespace {
// How much do we discount moves we might fix later.
constexpr double kCompoundDiscount = 1. / 1024;

std::pair<int64_t, double> FindBestValue(Domain domain, int64_t current_value,
                                         absl::FunctionRef<double(int64_t)> f) {
  std::pair<int64_t, double> result = std::make_pair(current_value, 0.0);
  domain = domain.IntersectionWith(
      Domain(current_value, current_value).Complement());
  for (int i = 0; i < domain.NumIntervals(); ++i) {
    const auto& [min, max] = domain[i];
    const auto& [val, score] = RangeConvexMinimum(result, min, max + 1, f);
    if (score < result.second) result = std::make_pair(val, score);
  }
  return result;
}
}  // namespace

FeasibilityJumpSolver::~FeasibilityJumpSolver() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"fs_jump/num_general_moves_computed", num_general_evals_});
  stats.push_back({"fs_jump/num_general_moves_done", num_general_moves_});
  stats.push_back({"fs_jump/num_linear_moves_computed", num_linear_evals_});
  stats.push_back({"fs_jump/num_linear_moves_done", num_linear_moves_});
  stats.push_back({"fs_jump/num_perturbations_applied", num_perturbations_});
  stats.push_back({"fs_jump/num_solutions_imported", num_solutions_imported_});
  stats.push_back({"fs_jump/num_weight_updates", num_weight_updates_});
  shared_stats_->AddStats(stats);
}

void FeasibilityJumpSolver::Initialize() {
  is_initialized_ = true;

  // For now we just disable or enable it.
  // But in the future we might have more variation.
  if (params_.feasibility_jump_linearization_level() == 0) {
    evaluator_ = std::make_unique<LsEvaluator>(linear_model_->model_proto());
  } else {
    evaluator_ = std::make_unique<LsEvaluator>(
        linear_model_->model_proto(), linear_model_->ignored_constraints(),
        linear_model_->additional_constraints());
  }

  const int num_variables = linear_model_->model_proto().variables().size();
  var_domains_.resize(num_variables);
  var_has_two_values_.resize(num_variables);
  for (int v = 0; v < num_variables; ++v) {
    var_domains_[v] =
        ReadDomainFromProto(linear_model_->model_proto().variables(v));
    var_has_two_values_[v] = var_domains_[v].HasTwoValues();
  }
  move_ =
      std::make_unique<CompoundMoveBuilder>(evaluator_.get(), num_variables);
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
}

void FeasibilityJumpSolver::PerturbateCurrentSolution() {
  const int num_variables = linear_model_->model_proto().variables().size();
  const double perturbation_probability =
      params_.feasibility_jump_var_randomization_probability();
  const double perturbation_ratio =
      params_.feasibility_jump_var_perburbation_range_ratio();
  std::vector<int64_t>& solution = *evaluator_->mutable_current_solution();
  for (int var = 0; var < num_variables; ++var) {
    if (var_domains_[var].IsFixed()) continue;
    if (absl::Bernoulli(random_, perturbation_probability)) {
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
          : absl::StrCat(" #gen_moves:", FormatCounter(num_general_moves_),
                         " #gen_evals:", FormatCounter(num_general_evals_));
  const std::string compound_str =
      num_compound_moves_ == 0 && move_->NumBacktracks() == 0
          ? ""
          : absl::StrCat(
                " #comp_moves:", FormatCounter(num_compound_moves_),
                " #backtracks:", FormatCounter(move_->NumBacktracks()));

  // Improving jumps and infeasible constraints.
  const int num_infeasible_cts = evaluator_->NumInfeasibleConstraints();
  const std::string non_solution_str =
      num_infeasible_cts == 0
          ? ""
          : absl::StrCat(" #good_lin_moves:", FormatCounter(good_jumps_.size()),
                         " #inf_cts:",
                         FormatCounter(evaluator_->NumInfeasibleConstraints()));

  return absl::StrCat("batch:", num_batches_, restart_str,
                      " #lin_moves:", FormatCounter(num_linear_moves_),
                      " #lin_evals:", FormatCounter(num_linear_evals_),
                      general_str, compound_str, non_solution_str,
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
      const double dtime = evaluator_->DeterministicTime();
      if (dtime >= dtime_restart_threshold_ &&
          num_weight_updates_ >= update_restart_threshold_) {
        if (num_restarts_ == 0 || params_.feasibility_jump_enable_restarts()) {
          ++num_restarts_;
          ResetCurrentSolution();
          should_recompute_violations = true;
          reset_weights = true;
        } else if (params_.feasibility_jump_var_randomization_probability() >
                   0.0) {
          ++num_perturbations_;
          PerturbateCurrentSolution();
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
        const int weight =
            std::max(1, params_.feasibility_jump_restart_factor());
        dtime_restart_threshold_ =
            dtime + weight * SUniv(num_restarts_ + num_perturbations_);
        update_restart_threshold_ = num_weight_updates_ + 10 * weight;
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
      // Each time we reset the weight, we randomly choose if we do decay or
      // not.
      bump_value_ = 1.0;
      weights_.assign(evaluator_->NumEvaluatorConstraints(), 1.0);
      use_decay_ = absl::Bernoulli(random_, 0.5);
    }
    if (params_.violation_ls_use_compound_moves()) {
      use_compound_moves_ = absl::Bernoulli(random_, 0.25);
    }
    // Search for feasible solution.
    ++num_batches_;
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
      const double dtime = evaluator_->DeterministicTime();
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
  return evaluator_->ObjectiveDelta(var, jump_deltas_[var]) < 0;
}

void FeasibilityJumpSolver::RecomputeJump(int var) {
  const std::vector<int64_t>& solution = evaluator_->current_solution();
  ++num_linear_evals_;
  jump_need_recomputation_[var] = false;
  if (var_domains_[var].IsFixed()) {
    jump_deltas_[var] = 0;
    jump_scores_[var] = 0.0;
    return;
  }
  const LinearIncrementalEvaluator& linear_evaluator =
      evaluator_->LinearEvaluator();

  if (var_has_two_values_[var]) {
    const int64_t min_value = var_domains_[var].Min();
    const int64_t max_value = var_domains_[var].Max();
    jump_deltas_[var] = solution[var] == min_value ? max_value - min_value
                                                   : min_value - max_value;
    jump_scores_[var] = linear_evaluator.WeightedViolationDelta(
        weights_, var, jump_deltas_[var]);
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
                          ? linear_evaluator.WeightedViolationDelta(
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
            linear_evaluator.SlopeBreakpoints(var, solution[var], dom);
        best_jump = ConvexMinimum<int64_t, double>(
            /*is_to_the_right=*/true, {p1, v1}, tmp_breakpoints_,
            [var, this, &linear_evaluator, &solution](int64_t jump_value) {
              return linear_evaluator.WeightedViolationDelta(
                  weights_, var, jump_value - solution[var]);
            });
      }
    } else {
      const double v2 = var_domains_[var].Contains(p2)
                            ? linear_evaluator.WeightedViolationDelta(
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
              linear_evaluator.SlopeBreakpoints(var, solution[var], dom);
          best_jump = ConvexMinimum<int64_t, double>(
              /*is_to_the_right=*/false, {p2, v2}, tmp_breakpoints_,
              [var, this, &linear_evaluator, &solution](int64_t jump_value) {
                return linear_evaluator.WeightedViolationDelta(
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
    jump_deltas_[var] = best_jump.first - solution[var];
    jump_scores_[var] = best_jump.second;
  }

  if (IsGood(var) && !in_good_jumps_[var]) {
    in_good_jumps_[var] = true;
    good_jumps_.push_back(var);
  }
}

void FeasibilityJumpSolver::RecomputeAllJumps() {
  const int num_variables = static_cast<int>(var_domains_.size());
  jump_deltas_.resize(num_variables);
  jump_scores_.resize(num_variables);
  jump_need_recomputation_.assign(num_variables, true);

  in_good_jumps_.assign(num_variables, false);
  good_jumps_.clear();

  for (int var = 0; var < num_variables; ++var) {
    RecomputeJump(var);
  }
}

void FeasibilityJumpSolver::UpdateViolatedConstraintWeights() {
  ++num_weight_updates_;

  // Because we update the weight incrementally, it is better to not have a
  // super hight magnitude, otherwise doing +max_weight and then -max_weight
  // will just ignore any constraint with a small weight and our
  // DCHECK(JumpIsUpToDate(var)) will fail more often.
  const double kMaxWeight = 1e10;
  const double kBumpFactor = 1.0 / params_.feasibility_jump_decay();
  if (use_decay_) {
    bump_value_ *= kBumpFactor;
  }

  // Note that ViolatedConstraints() might contain only linear constraint
  // depending on how it was initialized and updated.
  bool rescale = false;
  for (const int c : evaluator_->ViolatedConstraints()) {
    DCHECK(evaluator_->IsViolated(c));
    weights_[c] += bump_value_;
    if (use_compound_moves_) compound_weights_[c] = weights_[c];
    if (weights_[c] > kMaxWeight) rescale = true;
  }

  if (rescale) {
    const double factor = 1.0 / kMaxWeight;
    bump_value_ *= factor;
    for (int c = 0; c < weights_.size(); ++c) {
      weights_[c] *= factor;
      if (use_compound_moves_) compound_weights_[c] *= factor;
    }
    RecomputeAllJumps();
    return;
  }

  // Update weight incrementally.
  //
  // To maximize floating point precision, we compute the change to jump value
  // first and then apply it in one go. Also, in most situation the change is
  // purely integer and should fit exactly on a double, so we don't depend on
  // the order in which constraint are listed.
  LinearIncrementalEvaluator* linear_evaluator =
      evaluator_->MutableLinearEvaluator();
  linear_evaluator->ClearAffectedVariables();
  for_weight_update_.resize(jump_scores_.size());
  for (const int c : evaluator_->ViolatedConstraints()) {
    linear_evaluator->UpdateScoreOnWeightUpdate(
        c, jump_deltas_, absl::MakeSpan(for_weight_update_));
  }

  // Recompute the affected jumps.
  // Note that the constraint violations are unaffected.
  for (const int var : linear_evaluator->VariablesAffectedByLastUpdate()) {
    // Apply the delta.
    //
    // TODO(user): We could compute the minimal bump that lead to a good move.
    // That might change depending on the jump value though, so we can only
    // do that easily for Boolean I think.
    jump_scores_[var] += bump_value_ * for_weight_update_[var];

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
}

// Important: This is for debugging, but unfortunately it currently change the
// deterministic time and hence the overall algo behavior.
//
// TODO(user): Because we keep updating the score incrementally and we might
// have large constraint weight, we might have a pretty bad precision on
// the score though, so it is possible this fail.
bool FeasibilityJumpSolver::JumpIsUpToDate(int var) {
  const int64_t old_jump_delta = jump_deltas_[var];
  const double old_score = jump_scores_[var];
  RecomputeJump(var);
  CHECK_EQ(jump_deltas_[var], old_jump_delta);  // No change
  const double relative =
      std::max({std::abs(jump_scores_[var]), std::abs(old_score), 1.0});
  return std::abs(jump_scores_[var] - old_score) / relative < 1e-2;
}

bool FeasibilityJumpSolver::DoSomeLinearIterations() {
  RecomputeAllJumps();
  evaluator_->RecomputeViolatedList(/*linear_only=*/true);

  if (VLOG_IS_ON(1)) {
    shared_response_->LogMessageWithThrottling(name(), OneLineStats());
  }

  // TODO(user): It should be possible to support compound moves with the
  // specialized linear code, but lets keep it simpler for now.
  if (use_compound_moves_) return true;

  // Do a batch of a given number of loop here.
  // Outer loop: when no more greedy moves, update the weight.
  const int kBatchSize = 10000;
  const std::vector<int64_t>& solution = evaluator_->current_solution();
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
      int64_t best_delta = 0;
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
        const int64_t obj_delta =
            evaluator_->ObjectiveDelta(var, jump_deltas_[var]);
        if (std::make_pair(jump_scores_[var], obj_delta) <
            std::make_pair(best_score, best_obj_delta)) {
          best_var = var;
          best_index = index;
          best_delta = jump_deltas_[var];
          best_score = jump_scores_[var];
          best_obj_delta = obj_delta;
        }
      }

      if (good_jumps_.empty()) break;
      DCHECK_EQ(best_score,
                evaluator_->LinearEvaluator().WeightedViolationDelta(
                    weights_, best_var, best_delta));

      CHECK_NE(best_var, -1);
      CHECK_NE(best_index, -1);

      // Perform the move.
      ++num_linear_moves_;
      const int64_t best_value = solution[best_var] + best_delta;
      evaluator_->UpdateLinearScores(best_var, best_value, weights_,
                                     jump_deltas_,
                                     absl::MakeSpan(jump_scores_));
      evaluator_->UpdateVariableValue(best_var, best_value);

      // We already know the score of undoing the move we just did, and we know
      // this move is bad, so we can remove it from good_jumps_ right away.
      jump_deltas_[best_var] = -jump_deltas_[best_var];
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
    if (good_jumps_.empty()) {
      // Note that we only count linear constraint as violated here.
      if (evaluator_->ViolatedConstraints().empty()) return true;
      UpdateViolatedConstraintWeights();
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
  for (const int var : evaluator_->VariablesAffectedByLastLinearUpdate()) {
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
  if (!use_compound_moves_ && evaluator_->NumNonLinearConstraints() == 0) {
    return true;
  }
  const std::vector<int64_t>& solution = evaluator_->current_solution();

  // Non-linear constraints are not evaluated in the linear phase.
  evaluator_->UpdateAllNonLinearViolations();
  evaluator_->RecomputeViolatedList(/*linear_only=*/false);
  RecomputeVarsToScan();

  move_->Clear();
  absl::Span<const double> scan_weights = absl::MakeConstSpan(weights_);
  if (use_compound_moves_) {
    compound_weight_changed_.clear();
    in_compound_weight_changed_.assign(weights_.size(), false);
    compound_weights_.assign(weights_.begin(), weights_.end());
    for (int c = 0; c < evaluator_->NumEvaluatorConstraints(); ++c) {
      if (!evaluator_->IsViolated(c)) compound_weights_[c] *= kCompoundDiscount;
    }
    scan_weights = absl::MakeConstSpan(compound_weights_);
  }
  auto effort = [&]() {
    return num_general_evals_ + num_weight_updates_ + num_general_moves_;
  };
  const int64_t effort_limit = effort() + 100000;
  // Check size to make sure we are at a local minimum when we terminate.
  while (move_->Size() > 0 || effort() < effort_limit) {
    int var;
    int64_t value;
    double score;
    bool time_limit_crossed = false;
    DCHECK(!move_->IsImproving());
    // If we are past the effort limit stop looking for new moves.
    const bool found_move =
        (effort() < effort_limit &&
         ScanRelevantVariables(scan_weights, &var, &value, &score,
                               &time_limit_crossed));
    const bool backtrack =
        !found_move && move_->Backtrack(&var, &value, &score);
    if (found_move || backtrack) {
      const int64_t prev_value = solution[var];
      // Score is wrong if we are using compound moves, recompute.
      if (use_compound_moves_ && !backtrack) {
        ++num_general_evals_;
        score = evaluator_->WeightedViolationDelta(weights_, var,
                                                   value - prev_value);
      }

      // Perform the move.
      num_general_moves_++;

      // Update the linear part.
      evaluator_->UpdateLinearScores(var, value, weights_, jump_deltas_,
                                     absl::MakeSpan(jump_scores_));
      jump_scores_[var] = -score;
      jump_deltas_[var] = -jump_deltas_[var];

      // This score might include non-linear weights, so may be wrong.
      // We don't actually use it, but if we make things more incremental
      // across batches we may want this in future and it stops
      // UpdateViolatedConstraintWeights() from DCHECKing.
      jump_need_recomputation_[var] = MustRecomputeJumpOnGeneralUpdate(var);

      // Update the non-linear part. Note it also commits the move.
      evaluator_->UpdateNonLinearViolations(var, value);
      evaluator_->UpdateVariableValue(var, value);
      for (const auto& [c, violation_delta] :
           evaluator_->last_update_violation_changes()) {
        if (violation_delta == 0) continue;
        for (const int var : evaluator_->ConstraintToVars(c)) {
          if (violation_delta > 0 && evaluator_->IsViolated(c)) {
            num_violated_constraints_per_var_[var] += 1;
          } else if (violation_delta < 0 && !evaluator_->IsViolated(c)) {
            num_violated_constraints_per_var_[var] -= 1;
          }
          if (use_compound_moves_ && !in_compound_weight_changed_[c]) {
            compound_weights_[c] = weights_[c];
            compound_weight_changed_.push_back(c);
            in_compound_weight_changed_[c] = true;
          }
        }
      }

      // We call AddVarToScan() after num_violated_constraints_per_var_ has
      // been computed.
      for (const int v : evaluator_->VariablesAffectedByLastLinearUpdate()) {
        jump_need_recomputation_[v] = MustRecomputeJumpOnGeneralUpdate(v);
        AddVarToScan(v);
      }
      for (const int general_c : evaluator_->VarToGeneralConstraints(var)) {
        for (const int v : evaluator_->GeneralConstraintToVars(general_c)) {
          AddVarToScan(v);
        }
      }

      if (use_compound_moves_ && !backtrack) {
        // Make sure we can undo the move.
        move_->Push(var, prev_value, score);
        if (move_->IsImproving()) {
          if (move_->Size() > 1) {
            num_compound_moves_ += move_->Size();
          }
          move_->Clear();
          ResetChangedCompoundWeights();
        }
      }
      continue;
    } else if (time_limit_crossed) {
      return false;
    }
    DCHECK_EQ(move_->Size(), 0);
    if (evaluator_->ViolatedConstraints().empty()) return true;
    UpdateViolatedConstraintWeights();

    // Constraints with increased weight may lead to new negative score moves.
    for (const int c : evaluator_->ViolatedConstraints()) {
      for (const int v : evaluator_->ConstraintToVars(c)) {
        AddVarToScan(v);
      }
    }
    ResetChangedCompoundWeights();
  }
  return false;
}

void FeasibilityJumpSolver::ResetChangedCompoundWeights() {
  if (!use_compound_moves_) return;
  DCHECK_EQ(move_->Size(), 0);
  for (const int c : compound_weight_changed_) {
    in_compound_weight_changed_[c] = false;
    compound_weights_[c] = weights_[c];
    if (!evaluator_->IsViolated(c)) {
      compound_weights_[c] *= kCompoundDiscount;
      for (const int var : evaluator_->ConstraintToVars(c)) {
        AddVarToScan(var);
      }
    }
  }
  compound_weight_changed_.clear();
}

bool FeasibilityJumpSolver::ScanRelevantVariables(
    absl::Span<const double> weights, int* improving_var,
    int64_t* improving_value, double* improving_score,
    bool* time_limit_crossed) {
  DCHECK_GE(move_->Score(), 0);
  absl::Span<const int64_t> solution = evaluator_->current_solution();

  while (!vars_to_scan_.empty()) {
    const int idx = absl::Uniform<int>(random_, 0, vars_to_scan_.size());
    const int var = vars_to_scan_[idx];
    vars_to_scan_[idx] = vars_to_scan_.back();
    vars_to_scan_.pop_back();
    in_vars_to_scan_[var] = false;
    // Skip evaluating `var` if it cannot have an improving move.
    if (!ShouldScan(var)) continue;

    int64_t new_value;
    double score;
    const int64_t current_value = solution[var];
    if (!use_compound_moves_ &&
        evaluator_->VariableOnlyInLinearConstraintWithConvexViolationChange(
            var)) {
      // We lazily update the jump value.
      if (jump_need_recomputation_[var]) {
        RecomputeJump(var);
      } else {
        DCHECK(JumpIsUpToDate(var));
      }
      new_value = current_value + jump_deltas_[var];
      score = jump_scores_[var];
    } else {
      std::tie(new_value, score) = FindBestValue(
          var_domains_[var], current_value, [&](int64_t value) -> double {
            // Check the time limit periodically.
            if (++num_general_evals_ % 1000 == 0 &&
                shared_time_limit_ != nullptr &&
                shared_time_limit_->LimitReached()) {
              *time_limit_crossed = true;
            }
            if (*time_limit_crossed) return 0.0;
            return evaluator_->WeightedViolationDelta(weights, var,
                                                      value - current_value);
          });
      score += move_->Score();
    }
    if (*time_limit_crossed) return false;
    if (score > 0) continue;
    const double obj_delta =
        evaluator_->ObjectiveDelta(var, new_value - current_value);
    if (score == 0 && obj_delta >= 0) continue;
    *improving_var = var;
    *improving_value = new_value;
    *improving_score = score;
    return true;
  }
  return false;
}

bool FeasibilityJumpSolver::MustRecomputeJumpOnGeneralUpdate(int var) const {
  return !evaluator_->VariableOnlyInLinearConstraintWithConvexViolationChange(
             var) ||
         !var_has_two_values_[var];
}

void FeasibilityJumpSolver::AddVarToScan(int var) {
  if (in_vars_to_scan_[var] || !ShouldScan(var)) return;
  vars_to_scan_.push_back(var);
  in_vars_to_scan_[var] = true;
}

bool FeasibilityJumpSolver::ShouldScan(int var) const {
  if (move_->OnStack(var) || var_domains_[var].IsFixed()) return false;
  if (num_violated_constraints_per_var_[var] > 0) return true;
  const int64_t value = evaluator_->current_solution()[var];
  // Return true iff var is has a better objective value in its domain.
  return evaluator_->ObjectiveDelta(var, var_domains_[var].Max() - value) < 0 ||
         evaluator_->ObjectiveDelta(var, var_domains_[var].Min() - value) < 0;
}

void FeasibilityJumpSolver::RecomputeVarsToScan() {
  num_violated_constraints_per_var_.assign(var_domains_.size(), 0);
  in_vars_to_scan_.assign(evaluator_->current_solution().size(), false);
  for (const int c : evaluator_->ViolatedConstraints()) {
    for (const int v : evaluator_->ConstraintToVars(c)) {
      num_violated_constraints_per_var_[v] += 1;
      AddVarToScan(v);
    }
  }
}

bool CompoundMoveBuilder::IsImproving() const {
  return Score() < 0 || (Score() == 0 && ObjectiveDelta() < 0);
}

void CompoundMoveBuilder::Clear() {
  for (const UnitMove& move : stack_) {
    var_on_stack_[move.var] = false;
  }
  stack_.clear();
}

bool CompoundMoveBuilder::OnStack(int var) const {
  return !stack_.empty() && var_on_stack_[var];
}

bool CompoundMoveBuilder::Backtrack(int* var, int64_t* value, double* score) {
  if (stack_.empty()) return false;
  ++num_backtracks_;
  *var = stack_.back().var;
  *value = stack_.back().prev_value;
  *score = stack_.back().score;
  var_on_stack_[*var] = false;
  stack_.pop_back();
  DCHECK_NE(*value, evaluator_->current_solution()[*var]);
  return true;
}

void CompoundMoveBuilder::Push(int var, int64_t prev_value, double score) {
  DCHECK_NE(prev_value, evaluator_->current_solution()[var]);
  const double obj_delta = evaluator_->ObjectiveDelta(
      var, evaluator_->current_solution()[var] - prev_value);
  DCHECK(!var_on_stack_[var]);
  var_on_stack_[var] = true;
  stack_.push_back(
      {.var = var,
       .prev_value = prev_value,
       .score = -score,
       .cumulative_score = Score() + score,
       .cumulative_objective_delta = ObjectiveDelta() + obj_delta});
}
}  // namespace operations_research::sat
