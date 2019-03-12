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
#include "ortools/sat/integer.h"

namespace operations_research {
namespace sat {

namespace {

const LinearConstraintManager::ConstraintIndex kInvalidConstraintIndex(-1);

// TODO(user): it would be better if LinearConstraint natively supported
// term and not two separated vectors. Fix?
//
// TODO(user): Divide by gcd? perform coefficient strengthening (note that as
// the search progress the variable bounds get tighter)?
std::vector<std::pair<IntegerVariable, IntegerValue>>
CanonicalizeConstraintAndGetTerms(LinearConstraint* ct) {
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
  return terms;
}

}  // namespace

LinearConstraintManager::~LinearConstraintManager() {
  if (num_merged_constraints_ > 0) {
    VLOG(2) << "num_merged_constraints: " << num_merged_constraints_;
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
      constraint_is_in_lp_[constraint] = false;
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
//
// TODO(user): Also compute GCD and divide by it.
void LinearConstraintManager::Add(const LinearConstraint& ct) {
  LinearConstraint canonicalized = ct;
  const Terms terms = CanonicalizeConstraintAndGetTerms(&canonicalized);
  CHECK(!terms.empty());

  if (gtl::ContainsKey(equiv_constraints_, terms)) {
    const ConstraintIndex index(
        gtl::FindOrDieNoPrint(equiv_constraints_, terms));
    if (canonicalized.lb > constraints_[index].lb) {
      if (constraint_is_in_lp_[index]) {
        some_lp_constraint_bounds_changed_ = true;
      }
      constraints_[index].lb = canonicalized.lb;
    }
    if (canonicalized.ub < constraints_[index].ub) {
      if (constraint_is_in_lp_[index]) {
        some_lp_constraint_bounds_changed_ = true;
      }
      constraints_[index].ub = canonicalized.ub;
    }
    ++num_merged_constraints_;
  } else {
    constraint_l2_norms_.push_back(ComputeL2Norm(canonicalized));
    equiv_constraints_[terms] = constraints_.size();
    constraint_is_in_lp_.push_back(false);
    constraint_is_cut_.push_back(false);
    constraint_inactive_count_.push_back(0);
    constraint_permanently_removed_.push_back(false);
    constraints_.push_back(std::move(canonicalized));
  }
}

// Same as Add(), but logs some information about the newly added constraint.
// Cuts are also handled slightly differently than normal constraints.
void LinearConstraintManager::AddCut(
    const LinearConstraint& ct, std::string type_name,
    const gtl::ITIVector<IntegerVariable, double>& lp_solution) {
  if (ct.vars.empty()) return;

  const double activity = ComputeActivity(ct, lp_solution);
  const double violation =
      std::max(activity - ToDouble(ct.ub), ToDouble(ct.lb) - activity);
  const double l2_norm = ComputeL2Norm(ct);

  // Only add cut with sufficient efficacy.
  if (violation / l2_norm < 1e-5) return;

  Add(ct);
  constraint_is_cut_.back() = true;
  num_cuts_++;
  type_to_num_cuts_[type_name]++;

  VLOG(2) << "Cut '" << type_name << "'"
          << " size=" << ct.vars.size()
          << " max_magnitude=" << ComputeInfinityNorm(ct) << " norm=" << l2_norm
          << " violation=" << violation << " eff=" << violation / l2_norm;
}

bool LinearConstraintManager::ChangeLp(
    const gtl::ITIVector<IntegerVariable, double>& lp_solution) {
  const int old_num_constraints = lp_constraints_.size();

  std::vector<ConstraintIndex> new_constraints;
  std::vector<double> new_constraints_efficacies;
  std::vector<double> new_constraints_orthogonalities;

  // We keep any constraints that is already present, and otherwise, we add the
  // ones that are currently not satisfied by at least "tolerance".
  const double tolerance = 1e-6;
  for (ConstraintIndex i(0); i < constraints_.size(); ++i) {
    if (constraint_permanently_removed_[i]) continue;

    // TODO(user,user): Use constraint status from GLOP for constraints
    // already in LP.
    const double activity = ComputeActivity(constraints_[i], lp_solution);

    const double lb_violation = ToDouble(constraints_[i].lb) - activity;
    const double ub_violation = activity - ToDouble(constraints_[i].ub);
    const double violation = std::max(lb_violation, ub_violation);
    if (constraint_is_in_lp_[i] && violation < tolerance) {
      constraint_inactive_count_[i]++;
      if (constraint_is_cut_[i] && constraint_inactive_count_[i] >
                                       sat_parameters_.max_inactive_count()) {
        // Mark cut for removal.
        constraints_removal_list_.insert(i);
      }
    } else if (!constraint_is_in_lp_[i] && violation >= tolerance) {
      constraint_inactive_count_[i] = 0;
      new_constraints.push_back(i);
      new_constraints_efficacies.push_back(violation / constraint_l2_norms_[i]);
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
      if (constraint_permanently_removed_[new_constraint]) continue;
      if (constraint_is_in_lp_[new_constraint]) continue;

      if (last_added_candidate != kInvalidConstraintIndex) {
        const double current_orthogonality =
            1.0 - (std::abs(ScalarProduct(constraints_[last_added_candidate],
                                          constraints_[new_constraint])) /
                   (constraint_l2_norms_[last_added_candidate] *
                    constraint_l2_norms_[new_constraint]));
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
        constraint_permanently_removed_[new_constraint] = true;
        VLOG(2) << "Constraint permanently removed: " << new_constraint;
        continue;
      }
      // TODO(user): Experiment with different weights or different
      // functions for computing score.
      const double score =
          new_constraints_orthogonalities[j] + new_constraints_efficacies[j];
      CHECK_GE(score, 0.0);
      if (score > best_score || best_candidate == kInvalidConstraintIndex) {
        best_score = score;
        best_candidate = new_constraint;
      }
    }

    if (best_candidate != kInvalidConstraintIndex) {
      // Add the best constraint in the LP.
      constraint_is_in_lp_[best_candidate] = true;
      // Note that it is important for LP incremental solving that the old
      // constraints stays at the same position in this list (and thus in the
      // returned GetLp()).
      lp_constraints_.push_back(best_candidate);
      last_added_candidate = best_candidate;
    }
  }

  // The LP changed only if we added new constraints or if the bounds of some
  // constraints already in the LP changed because of parallel constraints
  // merging during Add().
  if (some_lp_constraint_bounds_changed_ ||
      lp_constraints_.size() > old_num_constraints) {
    some_lp_constraint_bounds_changed_ = false;
    return true;
  }
  return false;
}

void LinearConstraintManager::AddAllConstraintsToLp() {
  for (ConstraintIndex i(0); i < constraints_.size(); ++i) {
    if (constraint_is_in_lp_[i]) continue;
    constraint_is_in_lp_[i] = true;
    lp_constraints_.push_back(i);
  }
}

}  // namespace sat
}  // namespace operations_research
