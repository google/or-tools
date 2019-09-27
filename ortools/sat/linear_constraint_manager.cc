// Copyright 2010-2018 Google LLC
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
#include <utility>

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

// TODO(user): it would be better if LinearConstraint natively supported
// term and not two separated vectors. Fix?
void CanonicalizeConstraint(LinearConstraint* ct) {
  std::vector<std::pair<IntegerVariable, IntegerValue>> terms;

  const int size = ct->vars.size();
  for (int i = 0; i < size; ++i) {
    if (VariableIsPositive(ct->vars[i])) {
      terms.push_back({ct->vars[i], ct->coeffs[i]});
    } else {
      terms.push_back({NegationOf(ct->vars[i]), -ct->coeffs[i]});
    }
  }
  std::sort(terms.begin(), terms.end());

  ct->vars.clear();
  ct->coeffs.clear();
  for (const auto& term : terms) {
    ct->vars.push_back(term.first);
    ct->coeffs.push_back(term.second);
  }
}

}  // namespace

LinearConstraintManager::~LinearConstraintManager() {
  if (num_merged_constraints_ > 0) {
    VLOG(2) << "num_merged_constraints: " << num_merged_constraints_;
  }
  if (num_shortened_constraints_ > 0) {
    VLOG(2) << "num_shortened_constraints: " << num_shortened_constraints_;
  }
  if (num_splitted_constraints_ > 0) {
    VLOG(2) << "num_splitted_constraints: " << num_splitted_constraints_;
  }
  if (num_coeff_strenghtening_ > 0) {
    VLOG(2) << "num_coeff_strenghtening: " << num_coeff_strenghtening_;
  }
  for (const auto entry : type_to_num_cuts_) {
    VLOG(1) << "Added " << entry.second << " cuts of type '" << entry.first
            << "'.";
  }
}

// TODO(user,user): Also update the revised simplex starting basis for the
// next solve.
void LinearConstraintManager::RemoveMarkedConstraints() {
  int new_index = 0;
  for (int i = 0; i < lp_constraints_.size(); ++i) {
    const ConstraintIndex constraint = lp_constraints_[i];
    if (constraints_removal_list_.contains(constraint)) {
      constraint_infos_[constraint].is_in_lp = false;
      continue;
    }
    lp_constraints_[new_index] = constraint;
    new_index++;
  }
  lp_constraints_.resize(new_index);
  VLOG(2) << "Removed " << constraints_removal_list_.size() << " constraints.";
  constraints_removal_list_.clear();
}

// Because sometimes we split a == constraint in two (>= and <=), it makes sense
// to detect duplicate constraints and merge bounds. This is also relevant if
// we regenerate identical cuts for some reason.
LinearConstraintManager::ConstraintIndex LinearConstraintManager::Add(
    LinearConstraint ct) {
  CHECK(!ct.vars.empty());
  SimplifyConstraint(&ct);
  DivideByGCD(&ct);
  CanonicalizeConstraint(&ct);

  // If an identical constraint exists, only updates its bound.
  const size_t key = ComputeHashOfTerms(ct);
  if (gtl::ContainsKey(equiv_constraints_, key)) {
    const ConstraintIndex ct_index = equiv_constraints_[key];
    if (constraint_infos_[ct_index].constraint.vars == ct.vars &&
        constraint_infos_[ct_index].constraint.coeffs == ct.coeffs) {
      if (ct.lb > constraint_infos_[ct_index].constraint.lb) {
        if (constraint_infos_[ct_index].is_in_lp) current_lp_is_changed_ = true;
        constraint_infos_[ct_index].constraint.lb = ct.lb;
      }
      if (ct.ub < constraint_infos_[ct_index].constraint.ub) {
        if (constraint_infos_[ct_index].is_in_lp) current_lp_is_changed_ = true;
        constraint_infos_[ct_index].constraint.ub = ct.ub;
      }
      ++num_merged_constraints_;
      return ct_index;
    }
  }

  const ConstraintIndex ct_index(constraint_infos_.size());
  ConstraintInfo ct_info;
  ct_info.constraint = std::move(ct);
  ct_info.l2_norm = ComputeL2Norm(ct_info.constraint);
  ct_info.is_in_lp = false;
  ct_info.is_cut = false;
  ct_info.objective_parallelism_computed = false;
  ct_info.objective_parallelism = 0.0;
  ct_info.inactive_count = 0;
  ct_info.permanently_removed = false;
  ct_info.hash = key;
  equiv_constraints_[key] = ct_index;

  constraint_infos_.push_back(std::move(ct_info));
  return ct_index;
}

void LinearConstraintManager::ComputeObjectiveParallelism(
    const ConstraintIndex ct_index) {
  CHECK(objective_is_defined_);
  // lazy computation of objective norm.
  if (!objective_norm_computed_) {
    DivideByGCD(&objective_);
    CanonicalizeConstraint(&objective_);
    objective_l2_norm_ = ComputeL2Norm(objective_);
    objective_norm_computed_ = true;
  }
  CHECK_GT(objective_l2_norm_, 0.0);

  constraint_infos_[ct_index].objective_parallelism_computed = true;
  if (constraint_infos_[ct_index].l2_norm == 0.0) {
    constraint_infos_[ct_index].objective_parallelism = 0.0;
    return;
  }
  const double objective_parallelism =
      ScalarProduct(constraint_infos_[ct_index].constraint, objective_) /
      (constraint_infos_[ct_index].l2_norm * objective_l2_norm_);
  constraint_infos_[ct_index].objective_parallelism =
      std::abs(objective_parallelism);
}

// Same as Add(), but logs some information about the newly added constraint.
// Cuts are also handled slightly differently than normal constraints.
void LinearConstraintManager::AddCut(
    LinearConstraint ct, std::string type_name,
    const gtl::ITIVector<IntegerVariable, double>& lp_solution) {
  if (ct.vars.empty()) return;

  const double activity = ComputeActivity(ct, lp_solution);
  const double violation =
      std::max(activity - ToDouble(ct.ub), ToDouble(ct.lb) - activity);
  const double l2_norm = ComputeL2Norm(ct);

  // Only add cut with sufficient efficacy.
  if (violation / l2_norm < 1e-5) return;

  VLOG(2) << "Cut '" << type_name << "'"
          << " size=" << ct.vars.size()
          << " max_magnitude=" << ComputeInfinityNorm(ct) << " norm=" << l2_norm
          << " violation=" << violation << " eff=" << violation / l2_norm;

  // Add the constraint. We only mark the constraint as a cut if it is not an
  // update of an already existing one.
  const int64 prev_size = constraint_infos_.size();
  const ConstraintIndex ct_index = Add(std::move(ct));
  if (prev_size + 1 == constraint_infos_.size()) {
    num_cuts_++;
    type_to_num_cuts_[type_name]++;
    constraint_infos_[ct_index].is_cut = true;
  }
}

void LinearConstraintManager::SetObjectiveCoefficient(IntegerVariable var,
                                                      IntegerValue coeff) {
  if (coeff == IntegerValue(0)) return;
  objective_is_defined_ = true;
  objective_.AddTerm(var, coeff);
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
    const gtl::ITIVector<IntegerVariable, double>& lp_solution) {
  std::vector<ConstraintIndex> new_constraints;
  std::vector<double> new_constraints_efficacies;
  std::vector<double> new_constraints_orthogonalities;

  const bool simplify_constraints =
      integer_trail_.num_level_zero_enqueues() > last_simplification_timestamp_;
  last_simplification_timestamp_ = integer_trail_.num_level_zero_enqueues();

  // We keep any constraints that is already present, and otherwise, we add the
  // ones that are currently not satisfied by at least "tolerance".
  const double tolerance = 1e-6;
  for (ConstraintIndex i(0); i < constraint_infos_.size(); ++i) {
    if (constraint_infos_[i].permanently_removed) continue;

    // Inprocessing of the constraint.
    if (simplify_constraints &&
        SimplifyConstraint(&constraint_infos_[i].constraint)) {
      DivideByGCD(&constraint_infos_[i].constraint);
      constraint_infos_[i].l2_norm =
          ComputeL2Norm(constraint_infos_[i].constraint);

      if (constraint_infos_[i].is_in_lp) current_lp_is_changed_ = true;
      equiv_constraints_.erase(constraint_infos_[i].hash);
      constraint_infos_[i].hash =
          ComputeHashOfTerms(constraint_infos_[i].constraint);
      equiv_constraints_[constraint_infos_[i].hash] = i;
    }

    // TODO(user,user): Use constraint status from GLOP for constraints
    // already in LP.
    const double activity =
        ComputeActivity(constraint_infos_[i].constraint, lp_solution);

    const double lb_violation =
        ToDouble(constraint_infos_[i].constraint.lb) - activity;
    const double ub_violation =
        activity - ToDouble(constraint_infos_[i].constraint.ub);
    const double violation = std::max(lb_violation, ub_violation);
    if (constraint_infos_[i].is_in_lp && violation < tolerance) {
      constraint_infos_[i].inactive_count++;
      if (constraint_infos_[i].is_cut &&
          constraint_infos_[i].inactive_count >
              sat_parameters_.max_inactive_count()) {
        // Mark cut for removal.
        constraints_removal_list_.insert(i);
      }
    } else if (!constraint_infos_[i].is_in_lp && violation >= tolerance) {
      constraint_infos_[i].inactive_count = 0;
      new_constraints.push_back(i);
      new_constraints_efficacies.push_back(violation /
                                           constraint_infos_[i].l2_norm);
      new_constraints_orthogonalities.push_back(1.0);
    }
  }

  // Remove constraints in a batch to avoid changing the LP too frequently.
  if (constraints_removal_list_.size() >=
      sat_parameters_.constraint_removal_batch_size()) {
    RemoveMarkedConstraints();
  }

  // Note that the algo below is in O(limit * new_constraint), so this limit
  // should stay low.
  int constraint_limit = std::min(50, static_cast<int>(new_constraints.size()));
  if (lp_constraints_.empty()) {
    constraint_limit = std::min(1000, static_cast<int>(new_constraints.size()));
  }
  ConstraintIndex last_added_candidate = kInvalidConstraintIndex;
  for (int i = 0; i < constraint_limit; ++i) {
    // Iterate through all new constraints and select the one with the best
    // score.
    double best_score = 0.0;
    ConstraintIndex best_candidate = kInvalidConstraintIndex;
    for (int j = 0; j < new_constraints.size(); ++j) {
      const ConstraintIndex new_constraint = new_constraints[j];
      if (constraint_infos_[new_constraint].permanently_removed) continue;
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

      // NOTE(user): It is safe to permanently remove this constraint as the
      // constraint that is almost parallel to this constraint is present in the
      // LP or is inactive for a long time and is removed from the LP. In either
      // case, this constraint is not adding significant value and is only
      // making the LP larger.
      if (new_constraints_orthogonalities[j] <
          sat_parameters_.min_orthogonality_for_lp_constraints()) {
        constraint_infos_[new_constraint].permanently_removed = true;
        VLOG(2) << "Constraint permanently removed: " << new_constraint;
        continue;
      }

      if (objective_is_defined_ &&
          !constraint_infos_[new_constraint].objective_parallelism_computed) {
        ComputeObjectiveParallelism(new_constraint);
      }

      // TODO(user): Experiment with different weights or different
      // functions for computing score.
      const double score =
          new_constraints_orthogonalities[j] + new_constraints_efficacies[j] +
          constraint_infos_[new_constraint].objective_parallelism;
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
      current_lp_is_changed_ = true;
      lp_constraints_.push_back(best_candidate);
      last_added_candidate = best_candidate;
    }
  }

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

}  // namespace sat
}  // namespace operations_research
