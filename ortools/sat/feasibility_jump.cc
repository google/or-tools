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

#include <stdlib.h>

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

#include "absl/functional/any_invocable.h"
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
}  // namespace

JumpTable::JumpTable(
    absl::AnyInvocable<std::pair<int64_t, double>(int)> compute_jump)
    : compute_jump_(std::move(compute_jump)) {}

void JumpTable::RecomputeAll(int num_variables) {
  deltas_.resize(num_variables);
  scores_.resize(num_variables);
  needs_recomputation_.assign(num_variables, true);
}

void JumpTable::SetJump(int var, int64_t delta, double score) {
  deltas_[var] = delta;
  scores_[var] = score;
  needs_recomputation_[var] = false;
}

void JumpTable::Recompute(int var) { needs_recomputation_[var] = true; }

bool JumpTable::PossiblyGood(int var) const {
  return needs_recomputation_[var] || scores_[var] < 0;
}

bool JumpTable::JumpIsUpToDate(int var) {
  const auto& [delta, score] = compute_jump_(var);
  if (delta != deltas_[var]) {
    LOG(ERROR) << "Incorrect delta for var " << var << ": " << deltas_[var]
               << " (should be " << delta << ")";
  }
  bool score_ok = true;
  if (abs(score - scores_[var]) / std::max(abs(score), 1.0) > 1e-2) {
    score_ok = false;
    LOG(ERROR) << "Incorrect score for var " << var << ": " << scores_[var]
               << " (should be " << score << ") "
               << " delta = " << delta;
  }
  return delta == deltas_[var] && score_ok;
}

std::pair<int64_t, double> JumpTable::GetJump(int var) {
  if (needs_recomputation_[var]) {
    needs_recomputation_[var] = false;
    std::tie(deltas_[var], scores_[var]) = compute_jump_(var);
  }
  return std::make_pair(deltas_[var], scores_[var]);
}

FeasibilityJumpSolver::~FeasibilityJumpSolver() {
  stat_tables_->AddTimingStat(*this);
  stat_tables_->AddLsStat(name(), num_batches_, num_restarts_,
                          num_linear_moves_, num_general_moves_,
                          num_compound_moves_, num_weight_updates_);

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
    evaluator_ =
        std::make_unique<LsEvaluator>(linear_model_->model_proto(), params_);
  } else {
    evaluator_ =
        std::make_unique<LsEvaluator>(linear_model_->model_proto(), params_,
                                      linear_model_->ignored_constraints(),
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
  vars_to_scan_.reserve(num_variables);
  in_vars_to_scan_.assign(num_variables, false);
  move_ =
      std::make_unique<CompoundMoveBuilder>(evaluator_.get(), num_variables);
  var_occurs_in_non_linear_constraint_.resize(num_variables);
  for (int c = 0; c < evaluator_->NumNonLinearConstraints(); ++c) {
    for (int v : evaluator_->GeneralConstraintToVars(c)) {
      var_occurs_in_non_linear_constraint_[v] = true;
    }
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
          : absl::StrCat(" #good_moves:", FormatCounter(vars_to_scan_.size()),
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
        // The scores in the current move may now be wrong.
        move_->Clear();
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
      // Check if compound move search might backtrack out of the new domains.
      if (!move_->StackValuesInDomains(var_domains_)) {
        move_->Clear();
      }
    }

    if (should_recompute_violations) {
      evaluator_->ComputeAllViolations();
      move_->Clear();
    }
    if (reset_weights) {
      // Each time we reset the weight, we randomly choose if we do decay or
      // not.
      bump_value_ = 1.0;
      weights_.assign(evaluator_->NumEvaluatorConstraints(), 1.0);
      use_decay_ = absl::Bernoulli(random_, 0.5);
      move_->Clear();
      use_compound_moves_ = absl::Bernoulli(
          random_, params_.violation_ls_compound_move_probability());
      if (use_compound_moves_) {
        compound_move_max_discrepancy_ = 0;
        compound_weight_changed_.clear();
        in_compound_weight_changed_.assign(weights_.size(), false);
        // Compound weights for violated constraints will be set to the
        // correct values in InitializeCompoundWeights.
        compound_weights_.assign(weights_.size(), kCompoundDiscount);
      }
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

double FeasibilityJumpSolver::ComputeScore(absl::Span<const double> weights,
                                           int var, int64_t delta,
                                           bool linear_only) {
  ++num_scores_computed_;
  constexpr double kEpsilon = 1.0 / std::numeric_limits<int64_t>::max();
  double score =
      evaluator_->LinearEvaluator().WeightedViolationDelta(weights, var, delta);
  if (!linear_only) {
    score += evaluator_->WeightedNonLinearViolationDelta(weights, var, delta);
  }
  score += kEpsilon * evaluator_->ObjectiveDelta(var, delta);
  return score;
}

std::pair<int64_t, double> FeasibilityJumpSolver::ComputeLinearJump(int var) {
  const std::vector<int64_t>& solution = evaluator_->current_solution();
  if (var_domains_[var].IsFixed()) {
    return std::make_pair(0l, 0.0);
  }

  ++num_linear_evals_;
  const LinearIncrementalEvaluator& linear_evaluator =
      evaluator_->LinearEvaluator();

  if (var_has_two_values_[var]) {
    const int64_t min_value = var_domains_[var].Min();
    const int64_t max_value = var_domains_[var].Max();
    const int64_t delta = solution[var] == min_value ? max_value - min_value
                                                     : min_value - max_value;
    return std::make_pair(
        delta, ComputeScore(ScanWeights(), var, delta, /*linear_only=*/true));
  }

  // In practice, after a few iterations, the chance of finding an improving
  // move is slim, and we can test that fairly easily with at most two
  // queries!
  //
  // Tricky/Annoying: if the value is not in the domain, we returns it.
  const int64_t p1 = var_domains_[var].ValueAtOrBefore(solution[var] - 1);
  const int64_t p2 = var_domains_[var].ValueAtOrAfter(solution[var] + 1);

  std::pair<int64_t, double> best_jump;
  const double v1 = var_domains_[var].Contains(p1)
                        ? ComputeScore(ScanWeights(), var, p1 - solution[var],
                                       /*linear_only=*/true)
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
          [this, var, &solution](int64_t jump_value) {
            return ComputeScore(ScanWeights(), var, jump_value - solution[var],
                                /*linear_only=*/true);
          });
    }
  } else {
    const double v2 = var_domains_[var].Contains(p2)
                          ? ComputeScore(ScanWeights(), var, p2 - solution[var],
                                         /*linear_only=*/true)
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
            [this, var, &solution](int64_t jump_value) {
              return ComputeScore(ScanWeights(), var,
                                  jump_value - solution[var],
                                  /*linear_only=*/true);
            });
      }
    } else {
      // We have no improving point, result is either p1 or p2. This is the
      // most common scenario, and require no breakpoint computation!
      // Choose the direction which increases violation the least,
      // disambiguating by best objective.
      if (v1 < v2) {
        best_jump = {p1, v1};
      } else {
        best_jump = {p2, v2};
      }
    }
  }
  DCHECK_NE(best_jump.first, solution[var]);
  return std::make_pair(best_jump.first - solution[var], best_jump.second);
}

std::pair<int64_t, double> FeasibilityJumpSolver::ComputeGeneralJump(int var) {
  if (!var_occurs_in_non_linear_constraint_[var]) {
    return ComputeLinearJump(var);
  }
  Domain domain = var_domains_[var];
  if (domain.IsFixed()) return std::make_pair(0, 0.0);

  ++num_general_evals_;
  const int64_t current_value = evaluator_->current_solution()[var];
  domain = domain.IntersectionWith(
      Domain(current_value, current_value).Complement());
  std::pair<int64_t, double> result = RangeConvexMinimum<int64_t, double>(
      domain[0].start - current_value, domain[0].end - current_value + 1,
      [&](int64_t delta) -> double {
        return ComputeScore(ScanWeights(), var, delta, /*linear_only=*/false);
      });
  for (int i = 1; i < domain.NumIntervals(); ++i) {
    const int64_t min_delta = domain[i].start - current_value;
    const int64_t max_delta = domain[i].end - current_value;
    const auto& [delta, score] = RangeConvexMinimum<int64_t, double>(
        min_delta, max_delta + 1, [&](int64_t delta) -> double {
          return ComputeScore(ScanWeights(), var, delta, /*linear_only=*/false);
        });
    if (score < result.second) result = std::make_pair(delta, score);
  }
  DCHECK(domain.Contains(current_value + result.first))
      << current_value << "+" << result.first << "  not in domain "
      << domain.ToString();
  return result;
}

void FeasibilityJumpSolver::UpdateViolatedConstraintWeights(JumpTable& jumps) {
  ++num_weight_updates_;

  // Because we update the weight incrementally, it is better to not have a
  // super high magnitude, otherwise doing +max_weight and then -max_weight
  // will just ignore any constraint with a small weight and our
  // DCHECK(JumpIsUpToDate(var)) will fail more often.
  const double kMaxWeight = 1e10;
  const double kBumpFactor = 1.0 / params_.feasibility_jump_decay();
  const int num_variables = var_domains_.size();
  if (use_decay_) {
    bump_value_ *= kBumpFactor;
  }

  // Note that ViolatedConstraints() might contain only linear constraint
  // depending on how it was initialized and updated.
  bool rescale = false;
  for (const int c : evaluator_->ViolatedConstraints()) {
    DCHECK(evaluator_->IsViolated(c));
    if (use_compound_moves_) DCHECK_EQ(compound_weights_[c], weights_[c]);
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
    jumps.RecomputeAll(num_variables);
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
  for_weight_update_.resize(num_variables);
  for (const int c : evaluator_->ViolatedConstraints()) {
    if (c < evaluator_->NumLinearConstraints()) {
      linear_evaluator->UpdateScoreOnWeightUpdate(
          c, jumps.Deltas(), absl::MakeSpan(for_weight_update_));
    } else {
      for (const int v : evaluator_->ConstraintToVars(c)) {
        jumps.Recompute(v);
        AddVarToScan(jumps, v);
      }
    }
  }

  // Recompute the affected jumps.
  // Note that the constraint violations are unaffected.
  for (const int var : linear_evaluator->VariablesAffectedByLastUpdate()) {
    // Apply the delta.
    //
    // TODO(user): We could compute the minimal bump that would lead to a
    // good move. That might change depending on the jump value though, so
    // we can only do that easily for Booleans.
    if (!var_has_two_values_[var]) {
      jumps.Recompute(var);
    } else {
      // We may know the correct score for binary vars.
      jumps.MutableScores()[var] += bump_value_ * for_weight_update_[var];
    }
    AddVarToScan(jumps, var);
  }
}

bool FeasibilityJumpSolver::DoSomeLinearIterations() {
  if (VLOG_IS_ON(1)) {
    shared_response_->LogMessageWithThrottling(name(), OneLineStats());
  }

  // TODO(user): It should be possible to support compound moves with
  // the specialized linear code, but lets keep it simpler for now.
  if (use_compound_moves_) return true;

  evaluator_->RecomputeViolatedList(/*linear_only=*/true);
  RecomputeVarsToScan(linear_jumps_);

  // Do a batch of a given number of loop here.
  // Outer loop: when no more greedy moves, update the weight.
  const int kBatchSize = 10000;
  const std::vector<int64_t>& solution = evaluator_->current_solution();
  for (int loop = 0; loop < kBatchSize; ++loop) {
    // Inner loop: greedy descent.
    for (; loop < kBatchSize; ++loop) {
      // Take the best jump score amongst some random candidates.
      // It is okay if we pick twice the same, we don't really care.
      int best_var;
      int64_t best_value;
      double best_score;
      if (!ScanRelevantVariables(/*num_to_scan=*/5, linear_jumps_, &best_var,
                                 &best_value, &best_score)) {
        break;
      }
      const int64_t current_value = solution[best_var];

      // Perform the move.
      ++num_linear_moves_;
      evaluator_->UpdateLinearScores(best_var, best_value, weights_,
                                     linear_jumps_.Deltas(),
                                     linear_jumps_.MutableScores());
      evaluator_->UpdateVariableValue(best_var, best_value);

      if (var_has_two_values_[best_var]) {
        // We already know the score of undoing the move we just did, and that
        // this is optimal.
        linear_jumps_.SetJump(best_var, current_value - best_value,
                              -best_score);
      } else {
        linear_jumps_.Recompute(best_var);
      }
      MarkJumpsThatNeedToBeRecomputed(best_var, linear_jumps_);
    }
    if (time_limit_crossed_) return false;

    // We will update the weight unless the queue is non-empty.
    if (vars_to_scan_.empty()) {
      // Note that we only count linear constraint as violated here.
      if (evaluator_->ViolatedConstraints().empty()) return true;
      UpdateViolatedConstraintWeights(linear_jumps_);
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
void FeasibilityJumpSolver::MarkJumpsThatNeedToBeRecomputed(int changed_var,
                                                            JumpTable& jumps) {
  for (const int var : evaluator_->VariablesAffectedByLastLinearUpdate()) {
    if (var != changed_var && !var_has_two_values_[var]) {
      jumps.Recompute(var);
    }
    AddVarToScan(jumps, var);
  }
  for (const auto& [c, violation_delta] :
       evaluator_->last_update_violation_changes()) {
    if (c < evaluator_->NumLinearConstraints()) continue;
    for (const int var : evaluator_->ConstraintToVars(c)) {
      if (var != changed_var) {
        jumps.Recompute(var);
      }
      AddVarToScan(jumps, var);
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
  RecomputeVarsToScan(general_jumps_);
  InitializeCompoundWeights();
  auto effort = [&]() {
    return num_scores_computed_ + num_weight_updates_ + num_general_moves_;
  };
  const int64_t effort_limit = effort() + 100000;
  while (effort() < effort_limit) {
    int var;
    int64_t value;
    double score;
    const bool found_move = ScanRelevantVariables(
        /*num_to_scan=*/3, general_jumps_, &var, &value, &score);
    const bool backtrack =
        !found_move && move_->Backtrack(&var, &value, &score);
    if (found_move || backtrack) {
      // Perform the move.
      ++num_general_moves_;
      CHECK_NE(var, -1) << var << " " << found_move << " " << backtrack;
      const int64_t prev_value = solution[var];
      DCHECK_NE(prev_value, value);
      // Update the linear part.
      evaluator_->UpdateLinearScores(var, value, ScanWeights(),
                                     general_jumps_.Deltas(),
                                     general_jumps_.MutableScores());
      // Update the non-linear part. Note it also commits the move.
      evaluator_->UpdateNonLinearViolations(var, value);
      evaluator_->UpdateVariableValue(var, value);
      if (use_compound_moves_ && !backtrack) {
        // `!backtrack` is just an optimisation - we can never break any new
        // constraints on backtrack, so we can never change any
        // compound_weight_.
        for (const auto& [c, violation_delta] :
             evaluator_->last_update_violation_changes()) {
          if (violation_delta == 0) continue;
          if (evaluator_->IsViolated(c) &&
              compound_weights_[c] != weights_[c]) {
            compound_weights_[c] = weights_[c];
            if (!in_compound_weight_changed_[c]) {
              in_compound_weight_changed_[c] = true;
              compound_weight_changed_.push_back(c);
            }
            for (const int v : evaluator_->ConstraintToVars(c)) {
              general_jumps_.Recompute(v);
              // Vars will be added in MarkJumpsThatNeedToBeRecomputed.
            }
          } else if (!evaluator_->IsViolated(c) &&
                     !in_compound_weight_changed_[c]) {
            in_compound_weight_changed_[c] = true;
            compound_weight_changed_.push_back(c);
          }
        }
      }
      if (!use_decay_) {
        // Check that the score for undoing the move is -score with both the
        // default weights (which may be `weights_` or `compound_weights_`), and
        // with `weights_` explicitly.
        DCHECK_EQ(-score,
                  ComputeScore(ScanWeights(), var, prev_value - value, false));
        DCHECK_EQ(-score,
                  ComputeScore(weights_, var, prev_value - value, false));
      }
      if (var_has_two_values_[var]) {
        // We already know the score of the only possible move (undoing what we
        // just did).
        general_jumps_.SetJump(var, prev_value - value, -score);
      } else {
        general_jumps_.Recompute(var);
      }
      MarkJumpsThatNeedToBeRecomputed(var, general_jumps_);
      if (use_compound_moves_ && !backtrack) {
        // Make sure we can undo the move.
        move_->Push(var, prev_value, score);
        if (move_->Score() < 0) {
          num_compound_moves_ += move_->Size();
          move_->Clear();
          compound_move_max_discrepancy_ = 0;
        }
      }
      continue;
    } else if (time_limit_crossed_) {
      return false;
    }
    DCHECK_EQ(move_->Size(), 0);
    if (evaluator_->ViolatedConstraints().empty()) return true;
    if (use_compound_moves_) ResetChangedCompoundWeights();
    if (!use_compound_moves_ || ++compound_move_max_discrepancy_ > 2) {
      compound_move_max_discrepancy_ = 0;
      UpdateViolatedConstraintWeights(general_jumps_);
    }
  }
  return false;
}

void FeasibilityJumpSolver::ResetChangedCompoundWeights() {
  if (!use_compound_moves_) return;
  DCHECK_EQ(move_->Size(), 0);
  for (const int c : compound_weight_changed_) {
    in_compound_weight_changed_[c] = false;
    double expected_weight =
        (evaluator_->IsViolated(c) ? 1.0 : kCompoundDiscount) * weights_[c];
    if (compound_weights_[c] == expected_weight) continue;
    compound_weights_[c] = expected_weight;
    for (const int var : evaluator_->ConstraintToVars(c)) {
      general_jumps_.Recompute(var);
      AddVarToScan(general_jumps_, var);
    }
  }
  compound_weight_changed_.clear();
}

bool FeasibilityJumpSolver::ShouldExtendCompoundMove(double score,
                                                     double novelty) {
  if (move_->Score() + score - std::max(novelty, 0.0) < 0) {
    return true;
  }
  return score < move_->BestChildScore();
}

bool FeasibilityJumpSolver::ScanRelevantVariables(int num_to_scan,
                                                  JumpTable& jumps,
                                                  int* best_var,
                                                  int64_t* best_value,
                                                  double* best_score) {
  if (time_limit_crossed_) return false;
  if (move_->Discrepancy() > compound_move_max_discrepancy_) {
    return false;
  }
  double best_scan_score = 0.0;
  int num_good = 0;
  int best_index = -1;
  *best_var = -1;
  *best_score = 0.0;

  auto remove_var_to_scan_at_index = [&](int index) {
    in_vars_to_scan_[vars_to_scan_[index]] = false;
    vars_to_scan_[index] = vars_to_scan_.back();
    vars_to_scan_.pop_back();
    if (best_index == vars_to_scan_.size()) {
      best_index = index;
    }
  };
  while (!vars_to_scan_.empty() && num_good < num_to_scan) {
    const int index = absl::Uniform<int>(random_, 0, vars_to_scan_.size());
    const int var = vars_to_scan_[index];
    DCHECK_GE(var, 0);
    DCHECK(in_vars_to_scan_[var]);

    if (!ShouldScan(jumps, var)) {
      remove_var_to_scan_at_index(index);
      continue;
    }
    const auto [delta, scan_score] = jumps.GetJump(var);
    if ((num_general_evals_ + num_linear_evals_) % 100 == 0 &&
        shared_time_limit_ != nullptr && shared_time_limit_->LimitReached()) {
      time_limit_crossed_ = true;
      return false;
    }
    const int64_t current_value = evaluator_->current_solution()[var];
    DCHECK(var_domains_[var].Contains(current_value + delta))
        << var << " " << current_value << "+" << delta << " not in "
        << var_domains_[var].ToString();
    DCHECK(!var_domains_[var].IsFixed());
    // Note that this will likely fail if you use decaying weights as they
    // will have large magnitudes and the incremental update will be
    // imprecise.
    DCHECK(use_decay_ || jumps.JumpIsUpToDate(var))
        << var << " " << var_domains_[var].ToString() << " "
        << ComputeScore(ScanWeights(), var, delta, (&jumps == &linear_jumps_));
    if (scan_score >= 0) {
      remove_var_to_scan_at_index(index);
      continue;
    }
    double score = scan_score;
    if (use_compound_moves_) {
      // We only use compound moves in general iterations.
      score = ComputeScore(weights_, var, delta, /*linear_only=*/false);
      if (!ShouldExtendCompoundMove(score, score - scan_score)) {
        remove_var_to_scan_at_index(index);
        continue;
      }
    }

    ++num_good;
    if (scan_score < best_scan_score) {
      CHECK_NE(delta, 0) << score;
      *best_var = var;
      *best_value = current_value + delta;
      *best_score = score;
      best_scan_score = scan_score;
      best_index = index;
    }
  }
  if (best_index != -1) {
    DCHECK_GT(num_good, 0);
    DCHECK_GE(*best_var, 0);
    DCHECK_EQ(*best_var, vars_to_scan_[best_index]);
    remove_var_to_scan_at_index(best_index);
    return true;
  }
  return false;
}

void FeasibilityJumpSolver::AddVarToScan(const JumpTable& jumps, int var) {
  DCHECK_GE(var, 0);
  if (in_vars_to_scan_[var] || !ShouldScan(jumps, var)) return;
  vars_to_scan_.push_back(var);
  in_vars_to_scan_[var] = true;
}

bool FeasibilityJumpSolver::ShouldScan(const JumpTable& jumps, int var) const {
  DCHECK_GE(var, 0);
  if (var_domains_[var].IsFixed()) return false;
  if (!jumps.PossiblyGood(var)) return false;
  if (move_->OnStack(var)) return false;
  if (evaluator_->NumViolatedConstraintsForVar(var) > 0) return true;
  const int64_t value = evaluator_->current_solution()[var];
  // Return true iff var is has a better objective value in its domain.
  return evaluator_->ObjectiveDelta(var, var_domains_[var].Min() - value) < 0 ||
         evaluator_->ObjectiveDelta(var, var_domains_[var].Max() - value) < 0;
}

void FeasibilityJumpSolver::RecomputeVarsToScan(JumpTable& jumps) {
  const int num_variables = var_domains_.size();
  jumps.RecomputeAll(num_variables);
  in_vars_to_scan_.assign(num_variables, false);
  vars_to_scan_.clear();
  DCHECK(SlowCheckNumViolatedConstraints());
  for (const int c : evaluator_->ViolatedConstraints()) {
    for (const int v : evaluator_->ConstraintToVars(c)) {
      AddVarToScan(jumps, v);
    }
  }
}

void FeasibilityJumpSolver::InitializeCompoundWeights() {
  if (!use_compound_moves_) return;
  for (const int c : evaluator_->ViolatedConstraints()) {
    compound_weights_[c] = weights_[c];
  }
}

bool FeasibilityJumpSolver::SlowCheckNumViolatedConstraints() const {
  std::vector<int> result;
  result.assign(var_domains_.size(), 0);
  for (const int c : evaluator_->ViolatedConstraints()) {
    for (const int v : evaluator_->ConstraintToVars(c)) {
      ++result[v];
    }
  }
  for (int v = 0; v < result.size(); ++v) {
    CHECK_EQ(result[v], evaluator_->NumViolatedConstraintsForVar(v));
  }
  return true;
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
  if (!stack_.empty()) {
    ++stack_.back().discrepancy;
  }
  return true;
}

void CompoundMoveBuilder::Push(int var, int64_t prev_value, double score) {
  DCHECK_NE(prev_value, evaluator_->current_solution()[var]);
  DCHECK(!var_on_stack_[var]);
  if (!stack_.empty()) {
    stack_.back().best_child_score =
        std::min(stack_.back().best_child_score, score);
  }
  var_on_stack_[var] = true;
  stack_.push_back({
      .var = var,
      .prev_value = prev_value,
      .score = -score,
      .cumulative_score = Score() + score,
      .discrepancy = Discrepancy(),
  });
}

bool CompoundMoveBuilder::StackValuesInDomains(
    absl::Span<const Domain> var_domains) const {
  for (const UnitMove& move : stack_) {
    if (!var_domains[move.var].Contains(move.prev_value)) return false;
  }
  return true;
}

}  // namespace operations_research::sat
