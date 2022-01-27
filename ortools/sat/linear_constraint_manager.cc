// Copyright 2010-2021 Google LLC
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

#include "ortools/sat/linear_constraint_manager.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"

namespace operations_research {
namespace sat {

namespace {

const LinearConstraintManager::ConstraintIndex kInvalidConstraintIndex(-1);

size_t ComputeHashOfTerms(const LinearConstraint& ct) {
  DCHECK(std::is_sorted(ct.vars.begin(), ct.vars.end()));
  size_t hash = 0;
  const int num_terms = ct.vars.size();
  for (int i = 0; i < num_terms; ++i) {
    hash = util_hash::Hash(ct.vars[i].value(), hash);
    hash = util_hash::Hash(ct.coeffs[i].value(), hash);
  }
  return hash;
}

}  // namespace

std::string LinearConstraintManager::Statistics() const {
  std::string result;
  absl::StrAppend(&result, "  managed constraints: ", constraint_infos_.size(),
                  "\n");
  if (num_merged_constraints_ > 0) {
    absl::StrAppend(&result, "  merged constraints: ", num_merged_constraints_,
                    "\n");
  }
  if (num_shortened_constraints_ > 0) {
    absl::StrAppend(
        &result, "  shortened constraints: ", num_shortened_constraints_, "\n");
  }
  if (num_splitted_constraints_ > 0) {
    absl::StrAppend(
        &result, "  splitted constraints: ", num_splitted_constraints_, "\n");
  }
  if (num_coeff_strenghtening_ > 0) {
    absl::StrAppend(&result,
                    "  coefficient strenghtenings: ", num_coeff_strenghtening_,
                    "\n");
  }
  if (num_simplifications_ > 0) {
    absl::StrAppend(&result, "  num simplifications: ", num_simplifications_,
                    "\n");
  }
  absl::StrAppend(&result, "  total cuts added: ", num_cuts_, " (out of ",
                  num_add_cut_calls_, " calls)\n");
  for (const auto& entry : type_to_num_cuts_) {
    absl::StrAppend(&result, "    - '", entry.first, "': ", entry.second, "\n");
  }
  if (!result.empty()) result.pop_back();  // Remove last \n.
  return result;
}

void LinearConstraintManager::RescaleActiveCounts(const double scaling_factor) {
  for (ConstraintIndex i(0); i < constraint_infos_.size(); ++i) {
    constraint_infos_[i].active_count *= scaling_factor;
  }
  constraint_active_count_increase_ *= scaling_factor;
  VLOG(2) << "Rescaled active counts by " << scaling_factor;
}

bool LinearConstraintManager::MaybeRemoveSomeInactiveConstraints(
    glop::BasisState* solution_state) {
  if (solution_state->IsEmpty()) return false;  // Mainly to simplify tests.
  const glop::RowIndex num_rows(lp_constraints_.size());
  const glop::ColIndex num_cols =
      solution_state->statuses.size() - RowToColIndex(num_rows);
  int new_size = 0;
  for (int i = 0; i < num_rows; ++i) {
    const ConstraintIndex constraint_index = lp_constraints_[i];

    // Constraints that are not tight in the current solution have a basic
    // status. We remove the ones that have been inactive in the last recent
    // solves.
    //
    // TODO(user): More advanced heuristics might perform better, I didn't do
    // a lot of tuning experiments yet.
    const glop::VariableStatus row_status =
        solution_state->statuses[num_cols + glop::ColIndex(i)];
    if (row_status == glop::VariableStatus::BASIC) {
      constraint_infos_[constraint_index].inactive_count++;
      if (constraint_infos_[constraint_index].inactive_count >
          sat_parameters_.max_consecutive_inactive_count()) {
        constraint_infos_[constraint_index].is_in_lp = false;
        continue;  // Remove it.
      }
    } else {
      // Only count consecutive inactivities.
      constraint_infos_[constraint_index].inactive_count = 0;
    }

    lp_constraints_[new_size] = constraint_index;
    solution_state->statuses[num_cols + glop::ColIndex(new_size)] = row_status;
    new_size++;
  }
  const int num_removed_constraints = lp_constraints_.size() - new_size;
  lp_constraints_.resize(new_size);
  solution_state->statuses.resize(num_cols + glop::ColIndex(new_size));
  if (num_removed_constraints > 0) {
    VLOG(2) << "Removed " << num_removed_constraints << " constraints";
  }
  return num_removed_constraints > 0;
}

// Because sometimes we split a == constraint in two (>= and <=), it makes sense
// to detect duplicate constraints and merge bounds. This is also relevant if
// we regenerate identical cuts for some reason.
LinearConstraintManager::ConstraintIndex LinearConstraintManager::Add(
    LinearConstraint ct, bool* added) {
  CHECK(!ct.vars.empty());
  DCHECK(NoDuplicateVariable(ct));
  SimplifyConstraint(&ct);
  DivideByGCD(&ct);
  CanonicalizeConstraint(&ct);
  DCHECK(DebugCheckConstraint(ct));

  // If an identical constraint exists, only updates its bound.
  const size_t key = ComputeHashOfTerms(ct);
  if (equiv_constraints_.contains(key)) {
    const ConstraintIndex ct_index = equiv_constraints_[key];
    if (constraint_infos_[ct_index].constraint.vars == ct.vars &&
        constraint_infos_[ct_index].constraint.coeffs == ct.coeffs) {
      if (added != nullptr) *added = false;
      if (ct.lb > constraint_infos_[ct_index].constraint.lb) {
        if (constraint_infos_[ct_index].is_in_lp) current_lp_is_changed_ = true;
        constraint_infos_[ct_index].constraint.lb = ct.lb;
        if (added != nullptr) *added = true;
      }
      if (ct.ub < constraint_infos_[ct_index].constraint.ub) {
        if (constraint_infos_[ct_index].is_in_lp) current_lp_is_changed_ = true;
        constraint_infos_[ct_index].constraint.ub = ct.ub;
        if (added != nullptr) *added = true;
      }
      ++num_merged_constraints_;
      return ct_index;
    }
  }

  if (added != nullptr) *added = true;
  const ConstraintIndex ct_index(constraint_infos_.size());
  ConstraintInfo ct_info;
  ct_info.constraint = std::move(ct);
  ct_info.l2_norm = ComputeL2Norm(ct_info.constraint);
  ct_info.hash = key;
  equiv_constraints_[key] = ct_index;
  ct_info.active_count = constraint_active_count_increase_;
  constraint_infos_.push_back(std::move(ct_info));
  return ct_index;
}

void LinearConstraintManager::ComputeObjectiveParallelism(
    const ConstraintIndex ct_index) {
  CHECK(objective_is_defined_);
  // lazy computation of objective norm.
  if (!objective_norm_computed_) {
    objective_l2_norm_ = std::sqrt(sum_of_squared_objective_coeffs_);
    objective_norm_computed_ = true;
  }
  CHECK_GT(objective_l2_norm_, 0.0);

  constraint_infos_[ct_index].objective_parallelism_computed = true;
  if (constraint_infos_[ct_index].l2_norm == 0.0) {
    constraint_infos_[ct_index].objective_parallelism = 0.0;
    return;
  }

  const LinearConstraint& lc = constraint_infos_[ct_index].constraint;
  double unscaled_objective_parallelism = 0.0;
  for (int i = 0; i < lc.vars.size(); ++i) {
    const IntegerVariable var = lc.vars[i];
    const auto it = objective_map_.find(var);
    if (it == objective_map_.end()) continue;
    unscaled_objective_parallelism += it->second * ToDouble(lc.coeffs[i]);
  }
  const double objective_parallelism =
      unscaled_objective_parallelism /
      (constraint_infos_[ct_index].l2_norm * objective_l2_norm_);
  constraint_infos_[ct_index].objective_parallelism =
      std::abs(objective_parallelism);
}

// Same as Add(), but logs some information about the newly added constraint.
// Cuts are also handled slightly differently than normal constraints.
bool LinearConstraintManager::AddCut(
    LinearConstraint ct, std::string type_name,
    const absl::StrongVector<IntegerVariable, double>& lp_solution,
    std::string extra_info) {
  ++num_add_cut_calls_;
  if (ct.vars.empty()) return false;

  const double activity = ComputeActivity(ct, lp_solution);
  const double violation =
      std::max(activity - ToDouble(ct.ub), ToDouble(ct.lb) - activity);
  const double l2_norm = ComputeL2Norm(ct);

  // Only add cut with sufficient efficacy.
  if (violation / l2_norm < 1e-5) return false;

  bool added = false;
  const ConstraintIndex ct_index = Add(std::move(ct), &added);

  // We only mark the constraint as a cut if it is not an update of an already
  // existing one.
  if (!added) return false;

  // TODO(user): Use better heuristic here for detecting good cuts and mark
  // them undeletable.
  constraint_infos_[ct_index].is_deletable = true;

  VLOG(1) << "Cut '" << type_name << "'"
          << " size=" << constraint_infos_[ct_index].constraint.vars.size()
          << " max_magnitude="
          << ComputeInfinityNorm(constraint_infos_[ct_index].constraint)
          << " norm=" << l2_norm << " violation=" << violation
          << " eff=" << violation / l2_norm << " " << extra_info;

  num_cuts_++;
  num_deletable_constraints_++;
  type_to_num_cuts_[type_name]++;
  return true;
}

void LinearConstraintManager::PermanentlyRemoveSomeConstraints() {
  std::vector<double> deletable_constraint_counts;
  for (ConstraintIndex i(0); i < constraint_infos_.size(); ++i) {
    if (constraint_infos_[i].is_deletable && !constraint_infos_[i].is_in_lp) {
      deletable_constraint_counts.push_back(constraint_infos_[i].active_count);
    }
  }
  if (deletable_constraint_counts.empty()) return;
  std::sort(deletable_constraint_counts.begin(),
            deletable_constraint_counts.end());

  // We will delete the oldest (in the order they where added) cleanup target
  // constraints with a count lower or equal to this.
  double active_count_threshold = std::numeric_limits<double>::infinity();
  if (sat_parameters_.cut_cleanup_target() <
      deletable_constraint_counts.size()) {
    active_count_threshold =
        deletable_constraint_counts[sat_parameters_.cut_cleanup_target()];
  }

  ConstraintIndex new_size(0);
  equiv_constraints_.clear();
  absl::StrongVector<ConstraintIndex, ConstraintIndex> index_mapping(
      constraint_infos_.size());
  int num_deleted_constraints = 0;
  for (ConstraintIndex i(0); i < constraint_infos_.size(); ++i) {
    if (constraint_infos_[i].is_deletable && !constraint_infos_[i].is_in_lp &&
        constraint_infos_[i].active_count <= active_count_threshold &&
        num_deleted_constraints < sat_parameters_.cut_cleanup_target()) {
      ++num_deleted_constraints;
      continue;
    }

    if (i != new_size) {
      constraint_infos_[new_size] = std::move(constraint_infos_[i]);
    }
    index_mapping[i] = new_size;

    // Make sure we recompute the hash_map of identical constraints.
    equiv_constraints_[constraint_infos_[new_size].hash] = new_size;
    new_size++;
  }
  constraint_infos_.resize(new_size.value());

  // Also update lp_constraints_
  for (int i = 0; i < lp_constraints_.size(); ++i) {
    lp_constraints_[i] = index_mapping[lp_constraints_[i]];
  }

  if (num_deleted_constraints > 0) {
    VLOG(1) << "Constraint manager cleanup: #deleted:"
            << num_deleted_constraints;
  }
  num_deletable_constraints_ -= num_deleted_constraints;
}

void LinearConstraintManager::SetObjectiveCoefficient(IntegerVariable var,
                                                      IntegerValue coeff) {
  if (coeff == IntegerValue(0)) return;
  objective_is_defined_ = true;
  if (!VariableIsPositive(var)) {
    var = NegationOf(var);
    coeff = -coeff;
  }
  const double coeff_as_double = ToDouble(coeff);
  const auto insert = objective_map_.insert({var, coeff_as_double});
  CHECK(insert.second)
      << "SetObjectiveCoefficient() called twice with same variable";
  sum_of_squared_objective_coeffs_ += coeff_as_double * coeff_as_double;
}

bool LinearConstraintManager::SimplifyConstraint(LinearConstraint* ct) {
  bool term_changed = false;

  IntegerValue min_sum(0);
  IntegerValue max_sum(0);
  IntegerValue max_magnitude(0);
  int new_size = 0;
  const int num_terms = ct->vars.size();
  for (int i = 0; i < num_terms; ++i) {
    const IntegerVariable var = ct->vars[i];
    const IntegerValue coeff = ct->coeffs[i];
    const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
    const IntegerValue ub = integer_trail_.LevelZeroUpperBound(var);

    // For now we do not change ct, but just compute its new_size if we where
    // to remove a fixed term.
    if (lb == ub) continue;
    ++new_size;

    max_magnitude = std::max(max_magnitude, IntTypeAbs(coeff));
    if (coeff > 0.0) {
      min_sum += coeff * lb;
      max_sum += coeff * ub;
    } else {
      min_sum += coeff * ub;
      max_sum += coeff * lb;
    }
  }

  // Shorten the constraint if needed.
  if (new_size < num_terms) {
    term_changed = true;
    ++num_shortened_constraints_;
    new_size = 0;
    for (int i = 0; i < num_terms; ++i) {
      const IntegerVariable var = ct->vars[i];
      const IntegerValue coeff = ct->coeffs[i];
      const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
      const IntegerValue ub = integer_trail_.LevelZeroUpperBound(var);
      if (lb == ub) {
        const IntegerValue rhs_adjust = lb * coeff;
        if (ct->lb > kMinIntegerValue) ct->lb -= rhs_adjust;
        if (ct->ub < kMaxIntegerValue) ct->ub -= rhs_adjust;
        continue;
      }
      ct->vars[new_size] = var;
      ct->coeffs[new_size] = coeff;
      ++new_size;
    }
    ct->vars.resize(new_size);
    ct->coeffs.resize(new_size);
  }

  // Relax the bound if needed, note that this doesn't require a change to
  // the equiv map.
  if (min_sum >= ct->lb) ct->lb = kMinIntegerValue;
  if (max_sum <= ct->ub) ct->ub = kMaxIntegerValue;

  // Clear constraints that are always true.
  // We rely on the deletion code to remove them eventually.
  if (ct->lb == kMinIntegerValue && ct->ub == kMaxIntegerValue) {
    ct->vars.clear();
    ct->coeffs.clear();
    return true;
  }

  // TODO(user): Split constraint in two if it is boxed and there is possible
  // reduction?
  //
  // TODO(user): Make sure there cannot be any overflow. They shouldn't, but
  // I am not sure all the generated cuts are safe regarding min/max sum
  // computation. We should check this.
  if (ct->ub != kMaxIntegerValue && max_magnitude > max_sum - ct->ub) {
    if (ct->lb != kMinIntegerValue) {
      ++num_splitted_constraints_;
    } else {
      term_changed = true;
      ++num_coeff_strenghtening_;
      const int num_terms = ct->vars.size();
      const IntegerValue target = max_sum - ct->ub;
      for (int i = 0; i < num_terms; ++i) {
        const IntegerValue coeff = ct->coeffs[i];
        if (coeff > target) {
          const IntegerVariable var = ct->vars[i];
          const IntegerValue ub = integer_trail_.LevelZeroUpperBound(var);
          ct->coeffs[i] = target;
          ct->ub -= (coeff - target) * ub;
        } else if (coeff < -target) {
          const IntegerVariable var = ct->vars[i];
          const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
          ct->coeffs[i] = -target;
          ct->ub += (-target - coeff) * lb;
        }
      }
    }
  }

  if (ct->lb != kMinIntegerValue && max_magnitude > ct->lb - min_sum) {
    if (ct->ub != kMaxIntegerValue) {
      ++num_splitted_constraints_;
    } else {
      term_changed = true;
      ++num_coeff_strenghtening_;
      const int num_terms = ct->vars.size();
      const IntegerValue target = ct->lb - min_sum;
      for (int i = 0; i < num_terms; ++i) {
        const IntegerValue coeff = ct->coeffs[i];
        if (coeff > target) {
          const IntegerVariable var = ct->vars[i];
          const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
          ct->coeffs[i] = target;
          ct->lb -= (coeff - target) * lb;
        } else if (coeff < -target) {
          const IntegerVariable var = ct->vars[i];
          const IntegerValue ub = integer_trail_.LevelZeroUpperBound(var);
          ct->coeffs[i] = -target;
          ct->lb += (-target - coeff) * ub;
        }
      }
    }
  }

  return term_changed;
}

bool LinearConstraintManager::ChangeLp(
    const absl::StrongVector<IntegerVariable, double>& lp_solution,
    glop::BasisState* solution_state) {
  VLOG(3) << "Enter ChangeLP, scan " << constraint_infos_.size()
          << " constraints";
  const double saved_dtime = dtime_;
  std::vector<ConstraintIndex> new_constraints;
  std::vector<double> new_constraints_efficacies;
  std::vector<double> new_constraints_orthogonalities;

  const bool simplify_constraints =
      integer_trail_.num_level_zero_enqueues() > last_simplification_timestamp_;
  last_simplification_timestamp_ = integer_trail_.num_level_zero_enqueues();

  // We keep any constraints that is already present, and otherwise, we add the
  // ones that are currently not satisfied by at least "tolerance" to the set
  // of potential new constraints.
  bool rescale_active_count = false;
  const double tolerance = 1e-6;
  for (ConstraintIndex i(0); i < constraint_infos_.size(); ++i) {
    // Inprocessing of the constraint.
    if (simplify_constraints &&
        SimplifyConstraint(&constraint_infos_[i].constraint)) {
      ++num_simplifications_;

      // Note that the canonicalization shouldn't be needed since the order
      // of the variable is not changed by the simplification, and we only
      // reduce the coefficients at both end of the spectrum.
      DivideByGCD(&constraint_infos_[i].constraint);
      DCHECK(DebugCheckConstraint(constraint_infos_[i].constraint));

      constraint_infos_[i].objective_parallelism_computed = false;
      constraint_infos_[i].l2_norm =
          ComputeL2Norm(constraint_infos_[i].constraint);

      if (constraint_infos_[i].is_in_lp) current_lp_is_changed_ = true;
      equiv_constraints_.erase(constraint_infos_[i].hash);
      constraint_infos_[i].hash =
          ComputeHashOfTerms(constraint_infos_[i].constraint);

      // TODO(user): Because we simplified this constraint, it is possible that
      // it is now a duplicate of another one. Merge them.
      equiv_constraints_[constraint_infos_[i].hash] = i;
    }

    if (constraint_infos_[i].is_in_lp) continue;

    // ComputeActivity() often represent the bulk of the time spent in
    // ChangeLP().
    dtime_ += 1.7e-9 *
              static_cast<double>(constraint_infos_[i].constraint.vars.size());
    const double activity =
        ComputeActivity(constraint_infos_[i].constraint, lp_solution);
    const double lb_violation =
        ToDouble(constraint_infos_[i].constraint.lb) - activity;
    const double ub_violation =
        activity - ToDouble(constraint_infos_[i].constraint.ub);
    const double violation = std::max(lb_violation, ub_violation);
    if (violation >= tolerance) {
      constraint_infos_[i].inactive_count = 0;
      new_constraints.push_back(i);
      new_constraints_efficacies.push_back(violation /
                                           constraint_infos_[i].l2_norm);
      new_constraints_orthogonalities.push_back(1.0);

      if (objective_is_defined_ &&
          !constraint_infos_[i].objective_parallelism_computed) {
        ComputeObjectiveParallelism(i);
      } else if (!objective_is_defined_) {
        constraint_infos_[i].objective_parallelism = 0.0;
      }

      constraint_infos_[i].current_score =
          new_constraints_efficacies.back() +
          constraint_infos_[i].objective_parallelism;

      if (constraint_infos_[i].is_deletable) {
        constraint_infos_[i].active_count += constraint_active_count_increase_;
        if (constraint_infos_[i].active_count >
            sat_parameters_.cut_max_active_count_value()) {
          rescale_active_count = true;
        }
      }
    }
  }

  // Bump activities of active constraints in LP.
  if (solution_state != nullptr) {
    const glop::RowIndex num_rows(lp_constraints_.size());
    const glop::ColIndex num_cols =
        solution_state->statuses.size() - RowToColIndex(num_rows);

    for (int i = 0; i < num_rows; ++i) {
      const ConstraintIndex constraint_index = lp_constraints_[i];
      const glop::VariableStatus row_status =
          solution_state->statuses[num_cols + glop::ColIndex(i)];
      if (row_status != glop::VariableStatus::BASIC &&
          constraint_infos_[constraint_index].is_deletable) {
        constraint_infos_[constraint_index].active_count +=
            constraint_active_count_increase_;
        if (constraint_infos_[constraint_index].active_count >
            sat_parameters_.cut_max_active_count_value()) {
          rescale_active_count = true;
        }
      }
    }
  }

  if (rescale_active_count) {
    CHECK_GT(sat_parameters_.cut_max_active_count_value(), 0.0);
    RescaleActiveCounts(1.0 / sat_parameters_.cut_max_active_count_value());
  }

  // Update the increment counter.
  constraint_active_count_increase_ *=
      1.0 / sat_parameters_.cut_active_count_decay();

  // Remove constraints from the current LP that have been inactive for a while.
  // We do that after we computed new_constraints so we do not need to iterate
  // over the just deleted constraints.
  if (MaybeRemoveSomeInactiveConstraints(solution_state)) {
    current_lp_is_changed_ = true;
  }

  // Note that the algo below is in O(limit * new_constraint). In order to
  // limit spending too much time on this, we first sort all the constraints
  // with an imprecise score (no orthogonality), then limit the size of the
  // vector of constraints to precisely score, then we do the actual scoring.
  //
  // On problem crossword_opt_grid-19.05_dict-80_sat with linearization_level=2,
  // new_constraint.size() > 1.5M.
  //
  // TODO(user): This blowup factor could be adaptative w.r.t. the constraint
  // limit.
  const int kBlowupFactor = 4;
  int constraint_limit = std::min(sat_parameters_.new_constraints_batch_size(),
                                  static_cast<int>(new_constraints.size()));
  if (lp_constraints_.empty()) {
    constraint_limit = std::min(1000, static_cast<int>(new_constraints.size()));
  }
  VLOG(3) << "   - size = " << new_constraints.size()
          << ", limit = " << constraint_limit;

  std::stable_sort(new_constraints.begin(), new_constraints.end(),
                   [&](ConstraintIndex a, ConstraintIndex b) {
                     return constraint_infos_[a].current_score >
                            constraint_infos_[b].current_score;
                   });
  if (new_constraints.size() > kBlowupFactor * constraint_limit) {
    VLOG(3) << "Resize candidate constraints from " << new_constraints.size()
            << " down to " << kBlowupFactor * constraint_limit;
    new_constraints.resize(kBlowupFactor * constraint_limit);
  }

  int num_added = 0;
  int num_skipped_checks = 0;
  const int kCheckFrequency = 100;
  ConstraintIndex last_added_candidate = kInvalidConstraintIndex;
  for (int i = 0; i < constraint_limit; ++i) {
    // Iterate through all new constraints and select the one with the best
    // score.
    double best_score = 0.0;
    ConstraintIndex best_candidate = kInvalidConstraintIndex;
    for (int j = 0; j < new_constraints.size(); ++j) {
      // Checks the time limit, and returns if the lp has changed.
      if (++num_skipped_checks >= kCheckFrequency) {
        if (time_limit_->LimitReached()) return current_lp_is_changed_;
        num_skipped_checks = 0;
      }

      const ConstraintIndex new_constraint = new_constraints[j];
      if (constraint_infos_[new_constraint].is_in_lp) continue;

      if (last_added_candidate != kInvalidConstraintIndex) {
        const double current_orthogonality =
            1.0 - (std::abs(ScalarProduct(
                       constraint_infos_[last_added_candidate].constraint,
                       constraint_infos_[new_constraint].constraint)) /
                   (constraint_infos_[last_added_candidate].l2_norm *
                    constraint_infos_[new_constraint].l2_norm));
        new_constraints_orthogonalities[j] =
            std::min(new_constraints_orthogonalities[j], current_orthogonality);
      }

      // NOTE(user): It is safe to not add this constraint as the constraint
      // that is almost parallel to this constraint is present in the LP or is
      // inactive for a long time and is removed from the LP. In either case,
      // this constraint is not adding significant value and is only making the
      // LP larger.
      if (new_constraints_orthogonalities[j] <
          sat_parameters_.min_orthogonality_for_lp_constraints()) {
        continue;
      }

      // TODO(user): Experiment with different weights or different
      // functions for computing score.
      const double score = new_constraints_orthogonalities[j] +
                           constraint_infos_[new_constraint].current_score;
      CHECK_GE(score, 0.0);
      if (score > best_score || best_candidate == kInvalidConstraintIndex) {
        best_score = score;
        best_candidate = new_constraint;
      }
    }

    if (best_candidate != kInvalidConstraintIndex) {
      // Add the best constraint in the LP.
      constraint_infos_[best_candidate].is_in_lp = true;
      // Note that it is important for LP incremental solving that the old
      // constraints stays at the same position in this list (and thus in the
      // returned GetLp()).
      ++num_added;
      current_lp_is_changed_ = true;
      lp_constraints_.push_back(best_candidate);
      last_added_candidate = best_candidate;
    }
  }

  if (num_added > 0) {
    // We update the solution sate to match the new LP size.
    VLOG(2) << "Added " << num_added << " constraints.";
    solution_state->statuses.resize(solution_state->statuses.size() + num_added,
                                    glop::VariableStatus::BASIC);
  }

  // TODO(user): Instead of comparing num_deletable_constraints with cut
  // limit, compare number of deletable constraints not in lp against the limit.
  if (num_deletable_constraints_ > sat_parameters_.max_num_cuts()) {
    PermanentlyRemoveSomeConstraints();
  }

  time_limit_->AdvanceDeterministicTime(dtime_ - saved_dtime);

  // The LP changed only if we added new constraints or if some constraints
  // already inside changed (simplification or tighter bounds).
  if (current_lp_is_changed_) {
    current_lp_is_changed_ = false;
    return true;
  }
  return false;
}

void LinearConstraintManager::AddAllConstraintsToLp() {
  for (ConstraintIndex i(0); i < constraint_infos_.size(); ++i) {
    if (constraint_infos_[i].is_in_lp) continue;
    constraint_infos_[i].is_in_lp = true;
    lp_constraints_.push_back(i);
  }
}

bool LinearConstraintManager::DebugCheckConstraint(
    const LinearConstraint& cut) {
  if (model_->Get<DebugSolution>() == nullptr) return true;
  const auto& debug_solution = *(model_->Get<DebugSolution>());
  if (debug_solution.empty()) return true;

  IntegerValue activity(0);
  for (int i = 0; i < cut.vars.size(); ++i) {
    const IntegerVariable var = cut.vars[i];
    const IntegerValue coeff = cut.coeffs[i];
    activity += coeff * debug_solution[var];
  }
  if (activity > cut.ub || activity < cut.lb) {
    LOG(INFO) << "activity " << activity << " not in [" << cut.lb << ","
              << cut.ub << "]";
    return false;
  }
  return true;
}

void TopNCuts::AddCut(
    LinearConstraint ct, const std::string& name,
    const absl::StrongVector<IntegerVariable, double>& lp_solution) {
  if (ct.vars.empty()) return;
  const double activity = ComputeActivity(ct, lp_solution);
  const double violation =
      std::max(activity - ToDouble(ct.ub), ToDouble(ct.lb) - activity);
  const double l2_norm = ComputeL2Norm(ct);
  cuts_.Add({name, ct}, violation / l2_norm);
}

void TopNCuts::TransferToManager(
    const absl::StrongVector<IntegerVariable, double>& lp_solution,
    LinearConstraintManager* manager) {
  for (const CutCandidate& candidate : cuts_.UnorderedElements()) {
    manager->AddCut(candidate.cut, candidate.name, lp_solution);
  }
  cuts_.Clear();
}

}  // namespace sat
}  // namespace operations_research
