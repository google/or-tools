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
#include "absl/functional/bind_front.h"
#include "absl/functional/function_ref.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/algorithms/binary_search.h"
#include "ortools/sat/combine_solutions.h"
#include "ortools/sat/constraint_violation.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_model.h"
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

void JumpTable::SetComputeFunction(
    absl::AnyInvocable<std::pair<int64_t, double>(int) const> compute_jump) {
  compute_jump_ = std::move(compute_jump);
}

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

bool JumpTable::JumpIsUpToDate(int var) const {
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

SharedLsStates::~SharedLsStates() {
  // Do a final collection.
  for (int i = 0; i < states_.size(); ++i) {
    CollectStatistics(*states_[i].get());
  }

  // Display aggregated states
  for (const auto& [options, counters] : options_to_stats_) {
    stat_tables_->AddLsStat(
        absl::StrCat(name_, "_", options.name()), counters.num_batches,
        options_to_num_restarts_[options] + counters.num_perturbations,
        counters.num_linear_moves, counters.num_general_moves,
        counters.num_compound_moves, counters.num_backtracks,
        counters.num_weight_updates, counters.num_scores_computed);
  }
}

FeasibilityJumpSolver::~FeasibilityJumpSolver() {
  stat_tables_->AddTimingStat(*this);
}

void FeasibilityJumpSolver::ImportState() {
  state_ = states_->GetNextState();
  if (state_->move == nullptr) {
    const int num_variables = var_domains_.size();
    state_->move = std::make_unique<CompoundMoveBuilder>(num_variables);
  }
}

void FeasibilityJumpSolver::ReleaseState() { states_->Release(state_); }

bool FeasibilityJumpSolver::Initialize() {
  // For now we just disable or enable it.
  // But in the future we might have more variation.
  if (params_.feasibility_jump_linearization_level() == 0) {
    evaluator_ = std::make_unique<LsEvaluator>(linear_model_->model_proto(),
                                               params_, &time_limit_);
  } else {
    evaluator_ = std::make_unique<LsEvaluator>(
        linear_model_->model_proto(), params_,
        linear_model_->ignored_constraints(),
        linear_model_->additional_constraints(), &time_limit_);
  }

  if (time_limit_.LimitReached()) {
    evaluator_.reset();
    return false;
  }
  is_initialized_ = true;
  const int num_variables = linear_model_->model_proto().variables().size();
  var_domains_.resize(num_variables);
  for (int v = 0; v < num_variables; ++v) {
    var_domains_.Set(
        v, ReadDomainFromProto(linear_model_->model_proto().variables(v)));
  }
  var_domains_.InitializeObjective(linear_model_->model_proto());

  vars_to_scan_.ClearAndReserve(num_variables);
  var_occurs_in_non_linear_constraint_.resize(num_variables);
  for (int c = 0; c < evaluator_->NumNonLinearConstraints(); ++c) {
    for (int v : evaluator_->GeneralConstraintToVars(c)) {
      var_occurs_in_non_linear_constraint_[v] = true;
    }
  }
  return true;
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

void FeasibilityJumpSolver::ResetCurrentSolution(
    bool use_hint, bool use_objective, double perturbation_probability) {
  const int num_variables = linear_model_->model_proto().variables().size();
  const double default_value_probability = 1.0 - perturbation_probability;
  const double range_ratio =
      params_.feasibility_jump_var_perburbation_range_ratio();
  std::vector<int64_t>& solution = state_->solution;
  state_->base_solution = nullptr;

  // Resize the solution if needed.
  solution.resize(num_variables);

  // Starts with values closest to zero.
  for (int var = 0; var < num_variables; ++var) {
    if (var_domains_[var].IsFixed()) {
      solution[var] = var_domains_[var].FixedValue();
      continue;
    }

    if (absl::Bernoulli(random_, default_value_probability)) {
      solution[var] = var_domains_[var].SmallestValue();
    } else {
      solution[var] =
          RandomValueNearValue(var_domains_[var], 0, range_ratio, random_);
    }
  }

  // Use objective half of the time (if the model has one).
  if (use_objective && linear_model_->model_proto().has_objective()) {
    const int num_terms =
        linear_model_->model_proto().objective().vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = linear_model_->model_proto().objective().vars(i);
      if (var_domains_[var].IsFixed()) continue;

      if (linear_model_->model_proto().objective().coeffs(i) > 0) {
        if (absl::Bernoulli(random_, default_value_probability)) {
          solution[var] = var_domains_[var].Min();
        } else {
          solution[var] =
              RandomValueNearMin(var_domains_[var], range_ratio, random_);
        }
      } else {
        if (absl::Bernoulli(random_, default_value_probability)) {
          solution[var] = var_domains_[var].Max();
        } else {
          solution[var] =
              RandomValueNearMax(var_domains_[var], range_ratio, random_);
        }
      }
    }
  }

  if (use_hint && linear_model_->model_proto().has_solution_hint()) {
    const auto& hint = linear_model_->model_proto().solution_hint();
    for (int i = 0; i < hint.vars().size(); ++i) {
      solution[hint.vars(i)] = hint.values(i);
    }
  }
}

void FeasibilityJumpSolver::PerturbateCurrentSolution(
    double perturbation_probability) {
  if (perturbation_probability == 0.0) return;
  const int num_variables = linear_model_->model_proto().variables().size();
  const double perturbation_ratio =
      params_.feasibility_jump_var_perburbation_range_ratio();
  std::vector<int64_t>& solution = state_->solution;
  for (int var = 0; var < num_variables; ++var) {
    if (var_domains_[var].IsFixed()) continue;
    if (absl::Bernoulli(random_, perturbation_probability)) {
      solution[var] = RandomValueNearValue(var_domains_[var], solution[var],
                                           perturbation_ratio, random_);
    }
  }
}

std::string FeasibilityJumpSolver::OneLineStats() const {
  // Moves and evaluations in the general iterations.
  const std::string general_str =
      state_->counters.num_general_evals == 0 &&
              state_->counters.num_general_moves == 0
          ? ""
          : absl::StrCat(
                " gen{mvs:", FormatCounter(state_->counters.num_general_moves),
                " evals:", FormatCounter(state_->counters.num_general_evals),
                "}");
  const std::string compound_str =
      state_->counters.num_compound_moves == 0 &&
              state_->counters.num_backtracks == 0
          ? ""
          : absl::StrCat(" comp{mvs:",
                         FormatCounter(state_->counters.num_compound_moves),
                         " btracks:",
                         FormatCounter(state_->counters.num_backtracks), "}");

  // Improving jumps and infeasible constraints.
  const int num_infeasible_cts = evaluator_->NumInfeasibleConstraints();
  const std::string non_solution_str =
      num_infeasible_cts == 0
          ? ""
          : absl::StrCat(" #good_moves:", FormatCounter(vars_to_scan_.size()),
                         " #inf_cts:",
                         FormatCounter(evaluator_->NumInfeasibleConstraints()));

  return absl::StrCat(
      "batch:", state_->counters.num_batches,
      " lin{mvs:", FormatCounter(state_->counters.num_linear_moves),
      " evals:", FormatCounter(state_->counters.num_linear_evals), "}",
      general_str, compound_str, non_solution_str,
      " #w_updates:", FormatCounter(state_->counters.num_weight_updates),
      " #perturb:", FormatCounter(state_->counters.num_perturbations));
}

std::function<void()> FeasibilityJumpSolver::GenerateTask(int64_t /*task_id*/) {
  task_generated_ = true;  // Atomic.

  return [this] {
    // We delay initialization to the first task as it might be a bit slow
    // to scan the whole model, so we want to do this part in parallel.
    if (!is_initialized_) {
      if (!Initialize()) {
        return;
      }
    }

    // Load the next state to work on.
    ImportState();

    // If we found a new best solution, we will restart all violation ls (we
    // still finish each batch though). We will also reset the luby sequence.
    bool new_best_solution_was_found = false;
    if (type() == SubSolver::INCOMPLETE) {
      const int64_t best = shared_response_->GetBestSolutionObjective().value();
      if (best < state_->last_solution_rank) {
        states_->ResetLubyCounter();
        new_best_solution_was_found = true;
        state_->last_solution_rank = best;
      }
    }

    bool reset_weights = false;
    if (new_best_solution_was_found || state_->num_batches_before_change <= 0) {
      reset_weights = true;
      if (state_->options.use_restart) {
        states_->CollectStatistics(*state_);
        state_->options.Randomize(params_, &random_);
        state_->counters = LsCounters();  // Reset.
      } else {
        state_->options.Randomize(params_, &random_);
      }
      if (type() == SubSolver::INCOMPLETE) {
        // This is not used once we have a solution, and setting it to false
        // allow to fix the logs.
        state_->options.use_objective = false;
      }

      const bool first_time = (state_->num_restarts == 0);
      if (state_->options.use_restart || first_time ||
          new_best_solution_was_found) {
        if (type() == SubSolver::INCOMPLETE) {
          // Choose a base solution for this neighborhood.
          state_->base_solution =
              shared_response_->SolutionPool().GetSolutionToImprove(random_);
          state_->solution = state_->base_solution->variable_values;
          ++state_->num_solutions_imported;
        } else {
          if (!first_time) {
            // Register this solution before we reset the search.
            const int num_violations = evaluator_->ViolatedConstraints().size();
            shared_hints_->AddSolution(state_->solution, num_violations);
          }
          ResetCurrentSolution(/*use_hint=*/first_time,
                               state_->options.use_objective,
                               state_->options.perturbation_probability);
        }
      } else {
        PerturbateCurrentSolution(
            params_.feasibility_jump_var_randomization_probability());
      }

      if (state_->options.use_restart) {
        ++state_->num_restarts;
        states_->ConfigureNextLubyRestart(state_);
      } else {
        // TODO(user): Tune the improvement constant, maybe use luby.
        ++state_->counters.num_perturbations;
        state_->num_batches_before_change =
            params_.violation_ls_perturbation_period();
      }
    }

    // Between chunk, we synchronize bounds.
    //
    // TODO(user): This do not play well with optimizing solution whose
    // objective lag behind... Basically, we can run LS on old solution but will
    // only consider it feasible if it improve the best known solution.
    bool recompute_compound_weights = false;
    if (linear_model_->model_proto().has_objective()) {
      const IntegerValue lb = shared_response_->GetInnerObjectiveLowerBound();
      const IntegerValue ub = shared_response_->GetInnerObjectiveUpperBound();
      if (lb != state_->saved_inner_objective_lb ||
          ub != state_->saved_inner_objective_ub) {
        recompute_compound_weights = true;
      }
      state_->saved_inner_objective_lb = lb;
      state_->saved_inner_objective_ub = ub;

      if (ub < lb) return;  // Search is finished.
      if (evaluator_->ReduceObjectiveBounds(lb.value(), ub.value())) {
        recompute_compound_weights = true;
      }
    }

    // Update the variable domains with the last information.
    if (!var_domains_.UpdateFromSharedBounds()) return;

    // Checks the current solution is compatible with updated domains.
    {
      // Make sure the solution is within the potentially updated domain.
      // This also initialize var_domains_.CanIncrease()/CanDecrease().
      const int num_vars = state_->solution.size();
      for (int var = 0; var < num_vars; ++var) {
        const int64_t old_value = state_->solution[var];
        const int64_t new_value = var_domains_[var].ClosestValue(old_value);
        if (new_value != old_value) {
          state_->solution[var] = new_value;
          recompute_compound_weights = true;
        }
        var_domains_.OnValueChange(var, new_value);
      }
      // Check if compound move search might backtrack out of the new domains.
      if (!state_->move->StackValuesInDomains(var_domains_.AsSpan())) {
        recompute_compound_weights = true;
      }
    }

    // Search for feasible solution.
    // We always recompute that since we might have loaded from a different
    // state.
    evaluator_->ComputeAllViolations(state_->solution);

    if (reset_weights) {
      state_->bump_value = 1.0;
      state_->weights.assign(evaluator_->NumEvaluatorConstraints(), 1.0);
      recompute_compound_weights = true;
    }
    if (recompute_compound_weights) {
      state_->move->Clear();
      if (state_->options.use_compound_moves) {
        state_->compound_weights.assign(state_->weights.begin(),
                                        state_->weights.end());
        for (int c = 0; c < state_->weights.size(); ++c) {
          if (evaluator_->IsViolated(c)) continue;
          state_->compound_weights[c] *= kCompoundDiscount;
        }
        state_->compound_weight_changed.clear();
        state_->in_compound_weight_changed.assign(state_->weights.size(),
                                                  false);
        state_->compound_move_max_discrepancy = 0;
      }
    }

    if (!state_->options.use_compound_moves) {
      DCHECK_EQ(state_->move->Size(), 0);
    }

    ++state_->counters.num_batches;
    if (DoSomeLinearIterations() && DoSomeGeneralIterations()) {
      // Checks for infeasibility induced by the non supported constraints.
      //
      // TODO(user): Checking the objective is faster and we could avoid to
      // check feasibility if we are not going to keep the solution anyway.
      if (SolutionIsFeasible(linear_model_->model_proto(), state_->solution)) {
        auto pointers = PushAndMaybeCombineSolution(
            shared_response_, linear_model_->model_proto(), state_->solution,
            absl::StrCat(name(), "_", state_->options.name(), "(",
                         OneLineStats(), ")"),
            state_->base_solution);
        // If we pushed a new solution, we use it as a new "base" so that we
        // will have a smaller delta on the next solution we find.
        state_->base_solution = pointers.pushed_solution;
      } else {
        shared_response_->LogMessage(name(), "infeasible solution. Aborting.");
        model_is_supported_ = false;
      }
    }

    // Update dtime.
    // Since we execute only one task at the time, this is safe.
    {
      // TODO(user): Find better names. DeterministicTime() is maintained by
      // this class while deterministic_time() is the one saved in the SubSolver
      // base class).
      const double current_dtime = DeterministicTime();
      const double delta = current_dtime - deterministic_time();

      // Because deterministic_time() is computed with a sum of difference, it
      // might be slighlty different than DeterministicTime() and we don't want
      // to go backward, even by 1e-18.
      if (delta >= 0) {
        AddTaskDeterministicDuration(delta);
        shared_time_limit_->AdvanceDeterministicTime(delta);
      }
    }

    ReleaseState();
    task_generated_ = false;  // Atomic.
  };
}

double FeasibilityJumpSolver::ComputeScore(absl::Span<const double> weights,
                                           int var, int64_t delta,
                                           bool linear_only) {
  ++state_->counters.num_scores_computed;
  double score = evaluator_->WeightedViolationDelta(
      linear_only, weights, var, delta, absl::MakeSpan(state_->solution));
  constexpr double kEpsilon = 1.0 / std::numeric_limits<int64_t>::max();
  score += kEpsilon * delta * evaluator_->ObjectiveCoefficient(var);
  return score;
}

std::pair<int64_t, double> FeasibilityJumpSolver::ComputeLinearJump(int var) {
  DCHECK(!var_domains_[var].IsFixed());
  const int64_t current_value = state_->solution[var];

  ++state_->counters.num_linear_evals;
  const LinearIncrementalEvaluator& linear_evaluator =
      evaluator_->LinearEvaluator();

  if (var_domains_.HasTwoValues(var)) {
    const int64_t min_value = var_domains_[var].Min();
    const int64_t max_value = var_domains_[var].Max();
    const int64_t delta = current_value == min_value ? max_value - min_value
                                                     : min_value - max_value;
    return std::make_pair(
        delta, ComputeScore(ScanWeights(), var, delta, /*linear_only=*/true));
  }

  // In practice, after a few iterations, the chance of finding an improving
  // move is slim, and we can test that fairly easily with at most two
  // queries!
  //
  // Tricky/Annoying: if the value is not in the domain, we returns it.
  const int64_t p1 = var_domains_[var].ValueAtOrBefore(current_value - 1);
  const int64_t p2 = var_domains_[var].ValueAtOrAfter(current_value + 1);

  std::pair<int64_t, double> best_jump;
  const double v1 = var_domains_[var].Contains(p1)
                        ? ComputeScore(ScanWeights(), var, p1 - current_value,
                                       /*linear_only=*/true)
                        : std::numeric_limits<double>::infinity();
  if (v1 < 0.0) {
    // Point p1 is improving. Look for best before it.
    // Note that we can exclude all point after current_value since it is
    // worse and we assume convexity.
    const Domain dom = var_domains_[var].IntersectionWith(
        Domain(std::numeric_limits<int64_t>::min(), p1 - 1));
    if (dom.IsEmpty()) {
      best_jump = {p1, v1};
    } else {
      tmp_breakpoints_ =
          linear_evaluator.SlopeBreakpoints(var, current_value, dom);
      best_jump = ConvexMinimum<int64_t, double>(
          /*is_to_the_right=*/true, {p1, v1}, tmp_breakpoints_,
          [this, var, current_value](int64_t jump_value) {
            return ComputeScore(ScanWeights(), var, jump_value - current_value,
                                /*linear_only=*/true);
          });
    }
  } else {
    const double v2 = var_domains_[var].Contains(p2)
                          ? ComputeScore(ScanWeights(), var, p2 - current_value,
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
            linear_evaluator.SlopeBreakpoints(var, current_value, dom);
        best_jump = ConvexMinimum<int64_t, double>(
            /*is_to_the_right=*/false, {p2, v2}, tmp_breakpoints_,
            [this, var, current_value](int64_t jump_value) {
              return ComputeScore(ScanWeights(), var,
                                  jump_value - current_value,
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
  DCHECK_NE(best_jump.first, current_value);
  return std::make_pair(best_jump.first - current_value, best_jump.second);
}

std::pair<int64_t, double> FeasibilityJumpSolver::ComputeGeneralJump(int var) {
  if (!var_occurs_in_non_linear_constraint_[var]) {
    return ComputeLinearJump(var);
  }
  Domain domain = var_domains_[var];
  if (domain.IsFixed()) return std::make_pair(0, 0.0);

  ++state_->counters.num_general_evals;
  const int64_t current_value = state_->solution[var];
  domain = domain.IntersectionWith(
      Domain(current_value, current_value).Complement());
  std::pair<int64_t, double> result;
  for (int i = 0; i < domain.NumIntervals(); ++i) {
    const int64_t min_delta = domain[i].start - current_value;
    const int64_t max_delta = domain[i].end - current_value;
    const auto& [delta, score] = RangeConvexMinimum<int64_t, double>(
        min_delta, max_delta + 1, [&](int64_t delta) -> double {
          return ComputeScore(ScanWeights(), var, delta, /*linear_only=*/false);
        });
    if (i == 0 || score < result.second) {
      result = std::make_pair(delta, score);
    }
  }
  DCHECK(domain.Contains(current_value + result.first))
      << current_value << "+" << result.first << "  not in domain "
      << domain.ToString();
  return result;
}

void FeasibilityJumpSolver::UpdateViolatedConstraintWeights() {
  ++state_->counters.num_weight_updates;

  // Because we update the weight incrementally, it is better to not have a
  // super high magnitude, otherwise doing +max_weight and then -max_weight
  // will just ignore any constraint with a small weight and our
  // DCHECK(JumpIsUpToDate(var)) will fail more often.
  const double kMaxWeight = 1e10;
  const double kBumpFactor = 1.0 / params_.feasibility_jump_decay();
  const int num_variables = var_domains_.size();
  if (state_->options.use_decay) {
    state_->bump_value *= kBumpFactor;
  }

  // Note that ViolatedConstraints() might contain only linear constraint
  // depending on how it was initialized and updated.
  bool rescale = false;
  num_ops_ += evaluator_->ViolatedConstraints().size();
  for (const int c : evaluator_->ViolatedConstraints()) {
    DCHECK(evaluator_->IsViolated(c));
    if (state_->options.use_compound_moves) {
      DCHECK_EQ(state_->compound_weights[c], state_->weights[c]);
    }
    state_->weights[c] += state_->bump_value;
    if (state_->options.use_compound_moves) {
      state_->compound_weights[c] = state_->weights[c];
    }
    if (state_->weights[c] > kMaxWeight) {
      rescale = true;
    }
  }

  if (rescale) {
    const double factor = 1.0 / kMaxWeight;
    state_->bump_value *= factor;
    for (int c = 0; c < state_->weights.size(); ++c) {
      state_->weights[c] *= factor;
      if (state_->options.use_compound_moves) {
        state_->compound_weights[c] *= factor;
      }
    }
    jumps_.RecomputeAll(num_variables);
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
  num_ops_ += evaluator_->ViolatedConstraints().size();
  for (const int c : evaluator_->ViolatedConstraints()) {
    if (c < evaluator_->NumLinearConstraints()) {
      linear_evaluator->UpdateScoreOnWeightUpdate(
          c, jumps_.Deltas(), absl::MakeSpan(for_weight_update_));
    } else {
      for (const int v : evaluator_->ConstraintToVars(c)) {
        jumps_.Recompute(v);
        AddVarToScan(v);
      }
    }
  }

  // Recompute the affected jumps.
  // Note that the constraint violations are unaffected.
  absl::Span<double> scores = jumps_.MutableScores();
  for (const int var : linear_evaluator->VariablesAffectedByLastUpdate()) {
    // Apply the delta.
    //
    // TODO(user): We could compute the minimal bump that would lead to a
    // good move. That might change depending on the jump value though, so
    // we can only do that easily for Booleans.
    if (!var_domains_.HasTwoValues(var)) {
      jumps_.Recompute(var);
    } else {
      // We may know the correct score for binary vars.
      scores[var] += state_->bump_value * for_weight_update_[var];
    }
    AddVarToScan(var);
  }
}

bool FeasibilityJumpSolver::DoSomeLinearIterations() {
  if (VLOG_IS_ON(1)) {
    shared_response_->LogMessageWithThrottling(name(), OneLineStats());
  }

  // TODO(user): It should be possible to support compound moves with
  // the specialized linear code, but lets keep it simpler for now.
  if (state_->options.use_compound_moves) return true;

  evaluator_->RecomputeViolatedList(/*linear_only=*/true);
  jumps_.SetComputeFunction(
      absl::bind_front(&FeasibilityJumpSolver::ComputeLinearJump, this));
  RecomputeVarsToScan();

  // Do a batch of a given dtime.
  // Outer loop: when no more greedy moves, update the weight.
  const double dtime_threshold =
      DeterministicTime() + params_.feasibility_jump_batch_dtime();
  while (DeterministicTime() < dtime_threshold) {
    // Inner loop: greedy descent.
    while (DeterministicTime() < dtime_threshold) {
      // Take the best jump score amongst some random candidates.
      // It is okay if we pick twice the same, we don't really care.
      int best_var;
      int64_t best_value;
      double best_score;
      if (!ScanRelevantVariables(/*num_to_scan=*/5, &best_var, &best_value,
                                 &best_score)) {
        break;
      }

      // Perform the move.
      ++state_->counters.num_linear_moves;
      const int64_t prev_value = state_->solution[best_var];
      state_->solution[best_var] = best_value;
      evaluator_->UpdateLinearScores(best_var, prev_value, best_value,
                                     state_->weights, jumps_.Deltas(),
                                     jumps_.MutableScores());
      evaluator_->UpdateViolatedList();
      var_domains_.OnValueChange(best_var, best_value);

      MarkJumpsThatNeedToBeRecomputed(best_var);
      if (var_domains_.HasTwoValues(best_var)) {
        // We already know the score of undoing the move we just did, and that
        // this is optimal.
        jumps_.SetJump(best_var, prev_value - best_value, -best_score);
      }
    }
    if (time_limit_crossed_) return false;

    // We will update the weight unless the queue is non-empty.
    if (vars_to_scan_.empty()) {
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
void FeasibilityJumpSolver::MarkJumpsThatNeedToBeRecomputed(int changed_var) {
  // To keep DCHECKs happy. Note that we migh overwrite this afterwards with the
  // known score/jump of undoing the move.
  jumps_.Recompute(changed_var);

  // Generic part.
  // No optimization there, we just update all touched variables.
  // We need to do this before the Linear part, so that the status is correct in
  // AddVarToScan() for variable with two values.
  num_ops_ += evaluator_->VarToGeneralConstraints(changed_var).size();
  for (const int c : evaluator_->VarToGeneralConstraints(changed_var)) {
    num_ops_ += evaluator_->GeneralConstraintToVars(c).size();
    for (const int var : evaluator_->GeneralConstraintToVars(c)) {
      jumps_.Recompute(var);
      AddVarToScan(var);
    }
  }

  // Linear part.
  num_ops_ += evaluator_->VariablesAffectedByLastLinearUpdate().size();
  for (const int var : evaluator_->VariablesAffectedByLastLinearUpdate()) {
    if (!var_domains_.HasTwoValues(var)) {
      jumps_.Recompute(var);
    }
    AddVarToScan(var);
  }
}

bool FeasibilityJumpSolver::DoSomeGeneralIterations() {
  if (!state_->options.use_compound_moves &&
      evaluator_->NumNonLinearConstraints() == 0) {
    return true;
  }
  // Non-linear constraints are not evaluated in the linear phase.
  evaluator_->ComputeAllNonLinearViolations(state_->solution);
  evaluator_->RecomputeViolatedList(/*linear_only=*/false);
  if (evaluator_->NumNonLinearConstraints() == 0) {
    jumps_.SetComputeFunction(
        absl::bind_front(&FeasibilityJumpSolver::ComputeLinearJump, this));
  } else {
    jumps_.SetComputeFunction(
        absl::bind_front(&FeasibilityJumpSolver::ComputeGeneralJump, this));
  }
  RecomputeVarsToScan();

  const double dtime_threshold =
      DeterministicTime() + params_.feasibility_jump_batch_dtime();
  while (DeterministicTime() < dtime_threshold) {
    int var;
    int64_t new_value;
    double score;
    const bool found_move = ScanRelevantVariables(
        /*num_to_scan=*/3, &var, &new_value, &score);
    const bool backtrack =
        !found_move && state_->move->Backtrack(&var, &new_value, &score);
    if (found_move || backtrack) {
      if (backtrack) ++state_->counters.num_backtracks;
      DCHECK_NE(var, -1) << var << " " << found_move << " " << backtrack;

      // Perform the move.
      ++state_->counters.num_general_moves;
      const int64_t prev_value = state_->solution[var];
      DCHECK_NE(prev_value, new_value);
      state_->solution[var] = new_value;

      // Update the linear part and non-linear part.
      evaluator_->UpdateLinearScores(var, prev_value, new_value, ScanWeights(),
                                     jumps_.Deltas(), jumps_.MutableScores());
      evaluator_->UpdateNonLinearViolations(var, prev_value, state_->solution);
      evaluator_->UpdateViolatedList();
      var_domains_.OnValueChange(var, new_value);

      if (state_->options.use_compound_moves && !backtrack) {
        // `!backtrack` is just an optimisation - we can never break any new
        // constraints on backtrack, so we can never change any
        // compound_weight_.
        for (const auto& c : evaluator_->last_update_violation_changes()) {
          if (evaluator_->IsViolated(c) &&
              state_->compound_weights[c] != state_->weights[c]) {
            state_->compound_weights[c] = state_->weights[c];
            if (!state_->in_compound_weight_changed[c]) {
              state_->in_compound_weight_changed[c] = true;
              state_->compound_weight_changed.push_back(c);
            }
            for (const int v : evaluator_->ConstraintToVars(c)) {
              jumps_.Recompute(v);
              // Vars will be added in MarkJumpsThatNeedToBeRecomputed().
            }
          } else if (!evaluator_->IsViolated(c) &&
                     !state_->in_compound_weight_changed[c] &&
                     state_->compound_weights[c] == state_->weights[c]) {
            state_->in_compound_weight_changed[c] = true;
            state_->compound_weight_changed.push_back(c);
          }
        }
      }

      // Check that the score for undoing the move is -score with both the
      // default weights (which may be `state_->weights` or
      // `state_->compound_weights`), and with `weights` explicitly.
      // TODO(user): Re-enable DCHECK.
      if (/* DISABLES CODE */ false && !state_->options.use_decay) {
        DCHECK_EQ(-score, ComputeScore(state_->weights, var,
                                       prev_value - new_value, false));
        DCHECK_EQ(-score, ComputeScore(ScanWeights(), var,
                                       prev_value - new_value, false));
      }

      MarkJumpsThatNeedToBeRecomputed(var);
      if (var_domains_.HasTwoValues(var)) {
        // We already know the score of the only possible move (undoing what we
        // just did).
        jumps_.SetJump(var, prev_value - new_value, -score);
      }

      if (state_->options.use_compound_moves && !backtrack) {
        // Make sure we can undo the move.
        DCHECK_NE(prev_value, state_->solution[var]);
        state_->move->Push(var, prev_value, score);
        if (state_->move->Score() < 0) {
          state_->counters.num_compound_moves += state_->move->Size();
          state_->move->Clear();
          state_->compound_move_max_discrepancy = 0;
        }
      }
      continue;
    } else if (time_limit_crossed_) {
      return false;
    }

    DCHECK_EQ(state_->move->Size(), 0);
    if (evaluator_->ViolatedConstraints().empty()) return true;
    if (state_->options.use_compound_moves) {
      ResetChangedCompoundWeights();
    }
    if (!state_->options.use_compound_moves ||
        ++state_->compound_move_max_discrepancy > 2) {
      state_->compound_move_max_discrepancy = 0;
      UpdateViolatedConstraintWeights();
    }
  }
  return false;
}

void FeasibilityJumpSolver::ResetChangedCompoundWeights() {
  if (!state_->options.use_compound_moves) return;
  DCHECK_EQ(state_->move->Size(), 0);
  num_ops_ += state_->compound_weight_changed.size();
  for (const int c : state_->compound_weight_changed) {
    state_->in_compound_weight_changed[c] = false;
    const double expected_weight =
        (evaluator_->IsViolated(c) ? 1.0 : kCompoundDiscount) *
        state_->weights[c];
    if (state_->compound_weights[c] == expected_weight) continue;
    state_->compound_weights[c] = expected_weight;
    num_ops_ += evaluator_->ConstraintToVars(c).size();
    for (const int var : evaluator_->ConstraintToVars(c)) {
      jumps_.Recompute(var);
      AddVarToScan(var);
    }
  }
  state_->compound_weight_changed.clear();
}

bool FeasibilityJumpSolver::ShouldExtendCompoundMove(double score,
                                                     double novelty) {
  if (state_->move->Score() + score - std::max(novelty, 0.0) < 0) {
    return true;
  }
  return score < state_->move->BestChildScore();
}

bool FeasibilityJumpSolver::ScanRelevantVariables(int num_to_scan,
                                                  int* best_var,
                                                  int64_t* best_value,
                                                  double* best_score) {
  if (time_limit_crossed_) return false;
  if (state_->move->Discrepancy() > state_->compound_move_max_discrepancy) {
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
    num_ops_ += 6;  // We are slow here.
    const int index = absl::Uniform<int>(random_, 0, vars_to_scan_.size());
    const int var = vars_to_scan_[index];
    DCHECK_GE(var, 0);
    DCHECK(in_vars_to_scan_[var]);

    if (!ShouldScan(var)) {
      remove_var_to_scan_at_index(index);
      continue;
    }
    const auto [delta, scan_score] = jumps_.GetJump(var);
    if ((state_->counters.num_general_evals +
         state_->counters.num_linear_evals) %
                100 ==
            0 &&
        shared_time_limit_ != nullptr && shared_time_limit_->LimitReached()) {
      time_limit_crossed_ = true;
      return false;
    }
    const int64_t current_value = state_->solution[var];
    DCHECK(var_domains_[var].Contains(current_value + delta))
        << var << " " << current_value << "+" << delta << " not in "
        << var_domains_[var].ToString();
    DCHECK(!var_domains_[var].IsFixed());
    if (scan_score >= 0) {
      remove_var_to_scan_at_index(index);
      continue;
    }
    double score = scan_score;
    if (state_->options.use_compound_moves) {
      // We only use compound moves in general iterations.
      score = ComputeScore(
          state_->weights, var, delta,
          /*linear_only=*/!var_occurs_in_non_linear_constraint_[var]);
      if (!ShouldExtendCompoundMove(score, score - scan_score)) {
        remove_var_to_scan_at_index(index);
        continue;
      }
    }

    ++num_good;
    if (scan_score < best_scan_score) {
      DCHECK_NE(delta, 0) << score;
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

void FeasibilityJumpSolver::AddVarToScan(int var) {
  DCHECK_GE(var, 0);
  if (in_vars_to_scan_[var]) return;
  if (!ShouldScan(var)) return;
  vars_to_scan_.push_back(var);
  in_vars_to_scan_[var] = true;
}

bool FeasibilityJumpSolver::ShouldScan(int var) const {
  DCHECK_GE(var, 0);

  if (state_->move->OnStack(var)) return false;

  if (!jumps_.NeedRecomputation(var)) {
    // We already have the score/jump of that variable.
    const double score = jumps_.Score(var);
    return score < 0.0;
  }

  // See RecomputeVarsToScan(), we shouldn't have any fixed variable here.
  DCHECK(!var_domains_.IsFixed(var));

  // Return true iff var is has a better objective value in its domain.
  if (var_domains_.HasBetterObjectiveValue(var)) return true;

  // We will need to recompute the score. Lets skip variable for which we known
  // in advance that there will be no good score.
  //
  // For the objective, we don't care if it is violated or not, we only want
  // to scan variable that might improve it (and thus reduce its violation if it
  // is violated).
  //
  // TODO(user): We should generalize the objective logic to all constraint.
  // There is no point scanning a variable of a violated constraint if it is at
  // the wrong bound and cannot improve the violation!
  return evaluator_->NumViolatedConstraintsForVarIgnoringObjective(var) > 0;
}

void FeasibilityJumpSolver::RecomputeVarsToScan() {
  const int num_variables = var_domains_.size();
  jumps_.RecomputeAll(num_variables);
  DCHECK(SlowCheckNumViolatedConstraints());

  in_vars_to_scan_.assign(num_variables, false);
  vars_to_scan_.clear();

  // Since the fixed status never changes during one batch, we marks such
  // variable as "in_vars_to_scan_" even if we don't add them here. This allow
  // to skip them without any extra lookup.
  for (const int var : var_domains_.FixedVariables()) {
    in_vars_to_scan_[var] = true;
  }

  num_ops_ += evaluator_->ViolatedConstraints().size();
  for (const int c : evaluator_->ViolatedConstraints()) {
    num_ops_ += evaluator_->ConstraintToVars(c).size();
    for (const int v : evaluator_->ConstraintToVars(c)) {
      AddVarToScan(v);
    }
  }
}

bool FeasibilityJumpSolver::SlowCheckNumViolatedConstraints() const {
  std::vector<int> result;
  result.assign(var_domains_.size(), 0);
  for (const int c : evaluator_->ViolatedConstraints()) {
    if (evaluator_->IsObjectiveConstraint(c)) continue;
    for (const int v : evaluator_->ConstraintToVars(c)) {
      ++result[v];
    }
  }
  for (int v = 0; v < result.size(); ++v) {
    CHECK_EQ(result[v],
             evaluator_->NumViolatedConstraintsForVarIgnoringObjective(v));
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
  *var = stack_.back().var;
  *value = stack_.back().prev_value;
  *score = stack_.back().score;
  var_on_stack_[*var] = false;
  stack_.pop_back();
  if (!stack_.empty()) {
    ++stack_.back().discrepancy;
  }
  return true;
}

void CompoundMoveBuilder::Push(int var, int64_t prev_value, double score) {
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
