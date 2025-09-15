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

#include "ortools/sat/linear_constraint_manager.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/hash.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

LinearConstraintSymmetrizer::~LinearConstraintSymmetrizer() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"symmetrizer/overflows", num_overflows_});
  shared_stats_->AddStats(stats);
}

void LinearConstraintSymmetrizer::AddSymmetryOrbit(
    IntegerVariable sum_var, absl::Span<const IntegerVariable> orbit) {
  CHECK_GT(orbit.size(), 1);

  // Store the orbit info.
  const int orbit_index = orbits_.size();
  has_symmetry_ = true;
  orbit_sum_vars_.push_back(sum_var);
  orbits_.Add(orbit);

  // And fill var_to_orbit_index_.
  const int new_size = integer_trail_->NumIntegerVariables().value() / 2;
  if (var_to_orbit_index_.size() < new_size) {
    var_to_orbit_index_.resize(new_size, -1);
  }
  DCHECK(VariableIsPositive(sum_var));
  DCHECK_EQ(var_to_orbit_index_[GetPositiveOnlyIndex(sum_var)], -1);
  var_to_orbit_index_[GetPositiveOnlyIndex(sum_var)] = orbit_index;
  for (const IntegerVariable var : orbit) {
    DCHECK(VariableIsPositive(var));
    DCHECK_EQ(var_to_orbit_index_[GetPositiveOnlyIndex(var)], -1);
    var_to_orbit_index_[GetPositiveOnlyIndex(var)] = orbit_index;
  }
}

// What we do here is basically equivalent to adding all the possible
// permutations (under the problem symmetry group) of the constraint together.
// When we do that all variables in the same orbit will have the same
// coefficient (TODO(user): think how to prove this properly and especially
// that the scaling is in 1/orbit_size) and will each appear once. We then
// substitute each sum by the sum over the orbit, and divide coefficient by
// their gcd.
//
// Any solution of the original LP can be transformed to a solution of the
// folded LP with the same objective. So the folded LP will give us a tight and
// valid objective lower bound but with a lot less variables! This is an
// adaptation of "LP folding" to use in a MIP context. Introducing the orbit sum
// allow to propagates and add cuts as these sum are still integer for us.
//
// The only issue is regarding scaling of the constraints. Basically each
// orbit sum variable will appear with a factor 1/orbit_size in the original
// constraint.
//
// We will remap & scale the constraint.
// If not possible, we will drop it for now.
bool LinearConstraintSymmetrizer::FoldLinearConstraint(LinearConstraint* ct,
                                                       bool* folded) {
  if (!has_symmetry_) return true;

  // We assume the constraint had basic preprocessing with tight lb/ub for
  // instance. First pass is to compute the scaling factor.
  int64_t scaling_factor = 1;
  for (int i = 0; i < ct->num_terms; ++i) {
    const IntegerVariable var = ct->vars[i];
    CHECK(VariableIsPositive(var));

    const int orbit_index = var_to_orbit_index_[GetPositiveOnlyIndex(var)];
    if (orbit_index == -1 || orbit_sum_vars_[orbit_index] == var) {
      // If we have an orbit of size one, or the variable is its own
      // representative (orbit sum), skip.
      continue;
    }

    // Update the scaling factor.
    const int orbit_size = orbits_[orbit_index].size();
    if (AtMinOrMaxInt64(CapProd(orbit_size, scaling_factor))) {
      ++num_overflows_;
      VLOG(2) << "SYMMETRY skip constraint due to overflow";
      return false;
    }
    scaling_factor = std::lcm(scaling_factor, orbit_size);
  }

  if (scaling_factor == 1) {
    // No symmetric variables.
    return true;
  }

  if (folded != nullptr) *folded = true;

  // We need to multiply each term by scaling_factor / orbit_size.
  //
  // TODO(user): Now that we know the actual coefficient we could scale less.
  // Maybe the coefficient of an orbit_var is already divisible by orbit_size.
  builder_.Clear();
  for (int i = 0; i < ct->num_terms; ++i) {
    const IntegerVariable var = ct->vars[i];
    const IntegerValue coeff = ct->coeffs[i];

    const int orbit_index = var_to_orbit_index_[GetPositiveOnlyIndex(var)];
    if (orbit_index == -1 || orbit_sum_vars_[orbit_index] == var) {
      const int64_t scaled_coeff = CapProd(coeff.value(), scaling_factor);
      if (AtMinOrMaxInt64(scaled_coeff)) {
        ++num_overflows_;
        VLOG(2) << "SYMMETRY skip constraint due to overflow";
        return false;
      }
      builder_.AddTerm(var, scaled_coeff);
    } else {
      const int64_t orbit_size = orbits_[orbit_index].size();
      const int64_t factor = scaling_factor / orbit_size;
      const int64_t scaled_coeff = CapProd(coeff.value(), factor);
      if (AtMinOrMaxInt64(scaled_coeff)) {
        ++num_overflows_;
        VLOG(2) << "SYMMETRY skip constraint due to overflow";
        return false;
      }
      builder_.AddTerm(orbit_sum_vars_[orbit_index], scaled_coeff);
    }
  }

  if (AtMinOrMaxInt64(CapProd(ct->lb.value(), scaling_factor)) ||
      AtMinOrMaxInt64(CapProd(ct->ub.value(), scaling_factor))) {
    ++num_overflows_;
    VLOG(2) << "SYMMETRY skip constraint due to lb/ub overflow";
    return false;
  }
  if (!builder_.BuildIntoConstraintAndCheckOverflow(
          ct->lb * scaling_factor, ct->ub * scaling_factor, ct)) {
    ++num_overflows_;
    VLOG(2) << "SYMMETRY skip constraint due to overflow";
    return false;
  }

  // Dividing by gcd can help.
  DivideByGCD(ct);
  if (PossibleOverflow(*integer_trail_, *ct)) {
    ++num_overflows_;
    VLOG(2) << "SYMMETRY skip constraint due to overflow factor = "
            << scaling_factor;
    return false;
  }

  // TODO(user): In some cases, this constraint will propagate/fix directly
  // the orbit sum variables, we might want to propagate this in the cp world?
  // This migth also remove bad scaling.
  return true;
}

int LinearConstraintSymmetrizer::OrbitIndex(IntegerVariable var) const {
  if (!has_symmetry_) return -1;
  return var_to_orbit_index_[GetPositiveOnlyIndex(var)];
}

bool LinearConstraintSymmetrizer::IsOrbitSumVar(IntegerVariable var) const {
  if (!has_symmetry_) return false;
  const int orbit_index = var_to_orbit_index_[GetPositiveOnlyIndex(var)];
  return orbit_index >= 0 && orbit_sum_vars_[orbit_index] == var;
}

bool LinearConstraintSymmetrizer::AppearInFoldedProblem(
    IntegerVariable var) const {
  if (!has_symmetry_) return true;
  const int orbit_index = var_to_orbit_index_[GetPositiveOnlyIndex(var)];
  return orbit_index == -1 || orbit_sum_vars_[orbit_index] == var;
}

namespace {

const LinearConstraintManager::ConstraintIndex kInvalidConstraintIndex(-1);

size_t ComputeHashOfTerms(const LinearConstraint& ct) {
  DCHECK(std::is_sorted(ct.vars.get(), ct.vars.get() + ct.num_terms));
  size_t hash = 0;
  const int num_terms = ct.num_terms;
  for (int i = 0; i < num_terms; ++i) {
    hash = util_hash::Hash(ct.vars[i].value(), hash);
    hash = util_hash::Hash(ct.coeffs[i].value(), hash);
  }
  return hash;
}

}  // namespace

LinearConstraintManager::~LinearConstraintManager() {
  if (!VLOG_IS_ON(1)) return;
  if (model_->Get<SharedStatistics>() == nullptr) return;

  std::vector<std::pair<std::string, int64_t>> cut_stats;
  for (const auto& entry : type_to_num_cuts_) {
    cut_stats.push_back({absl::StrCat("cut/", entry.first), entry.second});
  }
  model_->Mutable<SharedStatistics>()->AddStats(cut_stats);
}

void LinearConstraintManager::RescaleActiveCounts(const double scaling_factor) {
  for (ConstraintIndex i(0); i < constraint_infos_.size(); ++i) {
    constraint_infos_[i].active_count *= scaling_factor;
  }
  constraint_active_count_increase_ *= scaling_factor;
  VLOG(3) << "Rescaled active counts by " << scaling_factor;
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
    if (constraint_infos_[constraint_index].constraint.num_terms == 0) {
      // Remove empty constraint.
      //
      // TODO(user): If the constraint is infeasible we could detect unsat
      // right away, but hopefully this is a case where the propagation part
      // of the solver can detect that too.
      constraint_infos_[constraint_index].is_in_lp = false;
      continue;
    }

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
    VLOG(3) << "Removed " << num_removed_constraints << " constraints";
  }
  return num_removed_constraints > 0;
}

// Because sometimes we split a == constraint in two (>= and <=), it makes sense
// to detect duplicate constraints and merge bounds. This is also relevant if
// we regenerate identical cuts for some reason.
LinearConstraintManager::ConstraintIndex LinearConstraintManager::Add(
    LinearConstraint ct, bool* added, bool* folded) {
  DCHECK_GT(ct.num_terms, 0);
  DCHECK(!PossibleOverflow(integer_trail_, ct)) << ct.DebugString();
  DCHECK(NoDuplicateVariable(ct));
  SimplifyConstraint(&ct);
  DivideByGCD(&ct);
  MakeAllVariablesPositive(&ct);
  DCHECK(DebugCheckConstraint(ct));

  // If configured, store instead the folded version of this constraint.
  // TODO(user): Shall we simplify again?
  if (symmetrizer_->HasSymmetry() &&
      !symmetrizer_->FoldLinearConstraint(&ct, folded)) {
    return kInvalidConstraintIndex;
  }
  CHECK(std::is_sorted(ct.VarsAsSpan().begin(), ct.VarsAsSpan().end()));

  // If an identical constraint exists, only updates its bound.
  const size_t key = ComputeHashOfTerms(ct);
  if (equiv_constraints_.contains(key)) {
    const ConstraintIndex ct_index = equiv_constraints_[key];
    if (constraint_infos_[ct_index].constraint.IsEqualIgnoringBounds(ct)) {
      bool tightened = false;
      if (ct.lb > constraint_infos_[ct_index].constraint.lb) {
        tightened = true;
        if (constraint_infos_[ct_index].is_in_lp) current_lp_is_changed_ = true;
        constraint_infos_[ct_index].constraint.lb = ct.lb;
      }
      if (ct.ub < constraint_infos_[ct_index].constraint.ub) {
        tightened = true;
        if (constraint_infos_[ct_index].is_in_lp) current_lp_is_changed_ = true;
        constraint_infos_[ct_index].constraint.ub = ct.ub;
      }
      if (added != nullptr) *added = tightened;
      if (tightened) {
        ++num_merged_constraints_;
        FillDerivedFields(&constraint_infos_[ct_index]);
      }
      return ct_index;
    }
  }

  if (added != nullptr) *added = true;
  const ConstraintIndex ct_index(constraint_infos_.size());
  ConstraintInfo ct_info;
  ct_info.constraint = std::move(ct);
  ct_info.l2_norm = ComputeL2Norm(ct_info.constraint);
  FillDerivedFields(&ct_info);
  ct_info.hash = key;
  equiv_constraints_[key] = ct_index;
  ct_info.active_count = constraint_active_count_increase_;
  constraint_infos_.push_back(std::move(ct_info));
  return ct_index;
}

bool LinearConstraintManager::UpdateConstraintLb(glop::RowIndex index_in_lp,
                                                 IntegerValue new_lb) {
  const ConstraintIndex index = lp_constraints_[index_in_lp.value()];
  ConstraintInfo& info = constraint_infos_[index];
  if (new_lb <= info.constraint.lb) return false;
  ++num_constraint_updates_;
  current_lp_is_changed_ = true;
  info.constraint.lb = new_lb;
  return true;
}

bool LinearConstraintManager::UpdateConstraintUb(glop::RowIndex index_in_lp,
                                                 IntegerValue new_ub) {
  const ConstraintIndex index = lp_constraints_[index_in_lp.value()];
  ConstraintInfo& info = constraint_infos_[index];
  if (new_ub >= info.constraint.ub) return false;
  ++num_constraint_updates_;
  current_lp_is_changed_ = true;
  info.constraint.ub = new_ub;
  return true;
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
  for (int i = 0; i < lc.num_terms; ++i) {
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
bool LinearConstraintManager::AddCut(LinearConstraint ct, std::string type_name,
                                     std::string extra_info) {
  ++num_add_cut_calls_;
  if (ct.num_terms == 0) return false;

  const double activity = ComputeActivity(ct, expanded_lp_solution_);
  const double violation =
      std::max(activity - ToDouble(ct.ub), ToDouble(ct.lb) - activity);
  const double l2_norm = ComputeL2Norm(ct);

  // Only add cut with sufficient efficacy.
  if (violation / l2_norm < 1e-4) {
    VLOG(3) << "BAD Cut '" << type_name << "'"
            << " size=" << ct.num_terms
            << " max_magnitude=" << ComputeInfinityNorm(ct)
            << " norm=" << l2_norm << " violation=" << violation
            << " eff=" << violation / l2_norm << " " << extra_info;
    return false;
  }

  // TODO(user): We could prevent overflow by dividing more. Note that mainly
  // happen with super large variable domain since we usually restrict the size
  // of the generated coefficients in our cuts. So it shouldn't be that
  // important.
  if (PossibleOverflow(integer_trail_, ct)) return false;

  bool added = false;
  const ConstraintIndex ct_index = Add(std::move(ct), &added);

  // We only mark the constraint as a cut if it is not an update of an already
  // existing one.
  if (!added) return false;

  // TODO(user): Use better heuristic here for detecting good cuts and mark
  // them undeletable.
  constraint_infos_[ct_index].is_deletable = true;

  VLOG(2) << "Cut '" << type_name << "'"
          << " size=" << constraint_infos_[ct_index].constraint.num_terms
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
  util_intops::StrongVector<ConstraintIndex, ConstraintIndex> index_mapping(
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
    VLOG(3) << "Constraint manager cleanup: #deleted:"
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

// TODO(user): Also consider partial gcd simplification? see presolve.
bool LinearConstraintManager::SimplifyConstraint(LinearConstraint* ct) {
  bool term_changed = false;

  IntegerValue min_sum(0);
  IntegerValue max_sum(0);
  IntegerValue max_magnitude(0);
  IntegerValue min_magnitude = kMaxIntegerValue;
  int new_size = 0;
  const int num_terms = ct->num_terms;
  for (int i = 0; i < num_terms; ++i) {
    const IntegerVariable var = ct->vars[i];
    const IntegerValue coeff = ct->coeffs[i];
    const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
    const IntegerValue ub = integer_trail_.LevelZeroUpperBound(var);

    // For now we do not change ct, but just compute its new_size if we where
    // to remove a fixed term.
    if (lb == ub) continue;
    ++new_size;

    const IntegerValue magnitude = IntTypeAbs(coeff);
    max_magnitude = std::max(max_magnitude, magnitude);
    min_magnitude = std::min(min_magnitude, magnitude);
    if (coeff > 0.0) {
      min_sum += coeff * lb;
      max_sum += coeff * ub;
    } else {
      min_sum += coeff * ub;
      max_sum += coeff * lb;
    }
  }

  // Shorten the constraint if needed.
  IntegerValue fixed_part = 0;
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
        fixed_part += coeff * lb;
        continue;
      }
      ct->vars[new_size] = var;
      ct->coeffs[new_size] = coeff;
      ++new_size;
    }
    ct->resize(new_size);
  }

  // Clear constraints that are always true.
  // We rely on the deletion code to remove them eventually.
  if (min_sum + fixed_part >= ct->lb && max_sum + fixed_part <= ct->ub) {
    ct->resize(0);
    ct->lb = 0;
    ct->ub = 0;
    return true;
  }

  // Make sure bounds are finite.
  ct->lb = std::max(ct->lb, min_sum + fixed_part);
  ct->ub = std::min(ct->ub, max_sum + fixed_part);
  ct->lb -= fixed_part;
  ct->ub -= fixed_part;

  // The variable can be shifted and complemented so we have constraints of
  // the form:
  // ... + |coeff| * X  >= threshold_ub
  // ... + |coeff| * X' >= threshold_lb
  // In both case if coeff is big, we can reduce it and update the rhs
  // accordingly.
  const IntegerValue threshold_ub = max_sum - ct->ub;
  const IntegerValue threshold_lb = ct->lb - min_sum;
  const IntegerValue threshold = std::max(threshold_lb, threshold_ub);
  CHECK_GT(threshold, 0);  // Since we aborted for trivial constraint.

  // TODO(user): In some case, we could split the constraint to reduce one of
  // them further. But not sure that is a good thing.
  if (threshold_ub > 0 && threshold_lb > 0 && threshold_lb != threshold_ub) {
    if (max_magnitude > std::min(threshold_lb, threshold_ub)) {
      ++num_split_constraints_;
    }
  }

  // TODO(user): For constraint with both bound, we could reduce further for
  // coefficient between threshold - min_magnitude and min(t_lb, t_ub).
  const IntegerValue second_threshold = std::max(
      {CeilRatio(threshold, IntegerValue(2)), threshold - min_magnitude,
       std::min(threshold_lb, threshold_ub)});
  if (max_magnitude > second_threshold) {
    const int num_terms = ct->num_terms;
    for (int i = 0; i < num_terms; ++i) {
      // In all cases, we reason on a transformed constraint where the term
      // is max_value - |coeff| * positive_X. If we change coeff, and
      // retransform the constraint, we need to change the rhs by the
      // constant term left.
      const IntegerValue coeff = ct->coeffs[i];
      const IntegerVariable var = ct->vars[i];
      const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
      const IntegerValue ub = integer_trail_.LevelZeroUpperBound(var);
      if (coeff > threshold) {
        term_changed = true;
        ++num_coeff_strenghtening_;
        ct->coeffs[i] = threshold;
        ct->ub -= (coeff - threshold) * ub;
        ct->lb -= (coeff - threshold) * lb;
      } else if (coeff > second_threshold && coeff < threshold) {
        term_changed = true;
        ++num_coeff_strenghtening_;
        ct->coeffs[i] = second_threshold;
        ct->ub -= (coeff - second_threshold) * ub;
        ct->lb -= (coeff - second_threshold) * lb;
      } else if (coeff < -threshold) {
        term_changed = true;
        ++num_coeff_strenghtening_;
        ct->coeffs[i] = -threshold;
        ct->ub -= (coeff + threshold) * lb;
        ct->lb -= (coeff + threshold) * ub;
      } else if (coeff < -second_threshold && coeff > -threshold) {
        term_changed = true;
        ++num_coeff_strenghtening_;
        ct->coeffs[i] = -second_threshold;
        ct->ub -= (coeff + second_threshold) * lb;
        ct->lb -= (coeff + second_threshold) * ub;
      }
    }
  }

  return term_changed;
}

void LinearConstraintManager::FillDerivedFields(ConstraintInfo* info) {
  IntegerValue min_sum(0);
  IntegerValue max_sum(0);
  const int num_terms = info->constraint.num_terms;
  for (int i = 0; i < num_terms; ++i) {
    const IntegerVariable var = info->constraint.vars[i];
    const IntegerValue coeff = info->constraint.coeffs[i];
    const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
    const IntegerValue ub = integer_trail_.LevelZeroUpperBound(var);
    if (coeff > 0.0) {
      min_sum += coeff * lb;
      max_sum += coeff * ub;
    } else {
      min_sum += coeff * ub;
      max_sum += coeff * lb;
    }
  }
  info->constraint.lb = std::max(min_sum, info->constraint.lb);
  info->constraint.ub = std::min(max_sum, info->constraint.ub);
  CHECK_NE(CapSub(info->constraint.ub.value(), info->constraint.lb.value()),
           std::numeric_limits<int64_t>::max());
  info->lb_is_trivial = min_sum >= info->constraint.lb;
  info->ub_is_trivial = max_sum <= info->constraint.ub;
}

bool LinearConstraintManager::ChangeLp(glop::BasisState* solution_state,
                                       int* num_new_constraints) {
  VLOG(3) << "Enter ChangeLP, scan " << constraint_infos_.size()
          << " constraints";
  const double saved_dtime = dtime_;
  std::vector<std::pair<ConstraintIndex, double>> new_constraints_by_score;
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
      FillDerivedFields(&constraint_infos_[i]);

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
    dtime_ +=
        1.7e-9 * static_cast<double>(constraint_infos_[i].constraint.num_terms);
    const double activity =
        ComputeActivity(constraint_infos_[i].constraint, expanded_lp_solution_);
    const double lb_violation =
        ToDouble(constraint_infos_[i].constraint.lb) - activity;
    const double ub_violation =
        activity - ToDouble(constraint_infos_[i].constraint.ub);
    const double violation = std::max(lb_violation, ub_violation);
    if (violation >= tolerance) {
      constraint_infos_[i].inactive_count = 0;
      if (objective_is_defined_ &&
          !constraint_infos_[i].objective_parallelism_computed) {
        ComputeObjectiveParallelism(i);
      } else if (!objective_is_defined_) {
        constraint_infos_[i].objective_parallelism = 0.0;
      }

      const double score = violation / constraint_infos_[i].l2_norm +
                           constraint_infos_[i].objective_parallelism;
      new_constraints_by_score.push_back({i, score});

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
  const int current_size = static_cast<int>(new_constraints_by_score.size());
  int constraint_limit =
      std::min(sat_parameters_.new_constraints_batch_size(), current_size);
  if (lp_constraints_.empty()) {
    constraint_limit = std::min(1000, current_size);
  }
  VLOG(3) << "   - size = " << current_size << ", limit = " << constraint_limit;

  std::stable_sort(new_constraints_by_score.begin(),
                   new_constraints_by_score.end(),
                   [&](std::pair<ConstraintIndex, double> a,
                       std::pair<ConstraintIndex, double> b) {
                     return a.second > b.second;
                   });
  if (new_constraints_by_score.size() > kBlowupFactor * constraint_limit) {
    VLOG(3) << "Resize candidate constraints from "
            << new_constraints_by_score.size() << " down to "
            << kBlowupFactor * constraint_limit;
    new_constraints_by_score.resize(kBlowupFactor * constraint_limit);
  }

  int num_added = 0;
  TimeLimitCheckEveryNCalls time_limit_check(1000, time_limit_);
  ConstraintIndex last_added_candidate = kInvalidConstraintIndex;
  std::vector<double> orthogonality_score(new_constraints_by_score.size(), 1.0);
  for (int i = 0; i < constraint_limit; ++i) {
    // Iterate through all new constraints and select the one with the best
    // score.
    //
    // TODO(user): find better algo, this does 1000 * 4000 scalar product!
    double best_score = 0.0;
    ConstraintIndex best_candidate = kInvalidConstraintIndex;
    for (int j = 0; j < new_constraints_by_score.size(); ++j) {
      // Checks the time limit, and returns if the lp has changed.
      if (time_limit_check.LimitReached()) return current_lp_is_changed_;

      const ConstraintIndex new_index = new_constraints_by_score[j].first;
      if (constraint_infos_[new_index].is_in_lp) continue;

      if (last_added_candidate != kInvalidConstraintIndex) {
        const double current_orthogonality =
            1.0 - (std::abs(ScalarProduct(
                       constraint_infos_[last_added_candidate].constraint,
                       constraint_infos_[new_index].constraint)) /
                   (constraint_infos_[last_added_candidate].l2_norm *
                    constraint_infos_[new_index].l2_norm));
        orthogonality_score[j] =
            std::min(orthogonality_score[j], current_orthogonality);
      }

      // NOTE(user): It is safe to not add this constraint as the constraint
      // that is almost parallel to this constraint is present in the LP or is
      // inactive for a long time and is removed from the LP. In either case,
      // this constraint is not adding significant value and is only making the
      // LP larger.
      if (orthogonality_score[j] <
          sat_parameters_.min_orthogonality_for_lp_constraints()) {
        continue;
      }

      // TODO(user): Experiment with different weights or different
      // functions for computing score.
      const double score =
          orthogonality_score[j] + new_constraints_by_score[j].second;
      CHECK_GE(score, 0.0);
      if (score > best_score || best_candidate == kInvalidConstraintIndex) {
        best_score = score;
        best_candidate = new_index;
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
    } else {
      // Abort the addition loop.
      break;
    }
  }

  if (num_new_constraints != nullptr) {
    *num_new_constraints = num_added;
  }
  if (num_added > 0) {
    // We update the solution sate to match the new LP size.
    VLOG(3) << "Added " << num_added << " constraints.";
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
    if (constraint_infos_[i].constraint.num_terms == 0) continue;

    constraint_infos_[i].is_in_lp = true;
    lp_constraints_.push_back(i);
  }
}

bool LinearConstraintManager::DebugCheckConstraint(
    const LinearConstraint& cut) {
  if (model_->Get<DebugSolution>() == nullptr) return true;
  const auto& debug_solution = *(model_->Get<DebugSolution>());

  IntegerValue activity(0);
  for (int i = 0; i < cut.num_terms; ++i) {
    const IntegerVariable var = cut.vars[i];
    const IntegerValue coeff = cut.coeffs[i];
    CHECK(debug_solution.ivar_has_value[var]);
    activity += coeff * debug_solution.ivar_values[var];
  }
  if (activity > cut.ub || activity < cut.lb) {
    LOG(INFO) << cut.DebugString();
    LOG(INFO) << "activity " << activity << " not in [" << cut.lb << ","
              << cut.ub << "]";
    return false;
  }
  return true;
}

void LinearConstraintManager::CacheReducedCostsInfo() {
  if (reduced_costs_is_cached_) return;
  reduced_costs_is_cached_ = true;

  const absl::int128 ub = reduced_cost_constraint_.ub.value();
  absl::int128 level_zero_lb = 0;
  for (int i = 0; i < reduced_cost_constraint_.num_terms; ++i) {
    IntegerVariable var = reduced_cost_constraint_.vars[i];
    IntegerValue coeff = reduced_cost_constraint_.coeffs[i];
    if (coeff < 0) {
      coeff = -coeff;
      var = NegationOf(var);
    }

    const IntegerValue lb = integer_trail_.LevelZeroLowerBound(var);
    level_zero_lb += absl::int128(coeff.value()) * absl::int128(lb.value());

    if (lb == integer_trail_.LevelZeroUpperBound(var)) continue;
    const LiteralIndex lit = integer_encoder_.GetAssociatedLiteral(
        IntegerLiteral::GreaterOrEqual(var, lb + 1));
    if (lit != kNoLiteralIndex) {
      reduced_costs_map_[Literal(lit)] = coeff;
    }
  }
  const absl::int128 gap = ub - level_zero_lb;
  if (gap > 0) {
    reduced_costs_gap_ = gap;
  } else {
    reduced_costs_gap_ = 0;
    reduced_costs_map_.clear();
  }
}

void TopNCuts::AddCut(
    LinearConstraint ct, absl::string_view name,
    const util_intops::StrongVector<IntegerVariable, double>& lp_solution) {
  if (ct.num_terms == 0) return;
  const double normalized_violation = ct.NormalizedViolation(lp_solution);
  cuts_.Add({std::string(name), std::move(ct)}, normalized_violation);
}

void TopNCuts::TransferToManager(LinearConstraintManager* manager) {
  for (CutCandidate& candidate : *cuts_.MutableUnorderedElements()) {
    manager->AddCut(std::move(candidate.cut), candidate.name);
  }
  cuts_.Clear();
}

}  // namespace sat
}  // namespace operations_research
