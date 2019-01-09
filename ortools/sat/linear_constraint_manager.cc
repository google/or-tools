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

double ComputeL2Norm(const LinearConstraint& constraint) {
  double sum = 0.0;
  for (const IntegerValue coeff : constraint.coeffs) {
    sum += ToDouble(coeff) * ToDouble(coeff);
  }
  return std::sqrt(sum);
}

double ScalarProduct(const LinearConstraint& constraint1,
                     const LinearConstraint& constraint2) {
  DCHECK(std::is_sorted(constraint1.vars.begin(), constraint1.vars.end()));
  DCHECK(std::is_sorted(constraint2.vars.begin(), constraint2.vars.end()));
  double scalar_product = 0.0;
  int index_1 = 0;
  int index_2 = 0;
  while (index_1 < constraint1.vars.size() &&
         index_2 < constraint2.vars.size()) {
    if (constraint1.vars[index_1] == constraint2.vars[index_2]) {
      scalar_product += ToDouble(constraint1.coeffs[index_1]) *
                        ToDouble(constraint2.coeffs[index_2]);
      index_1++;
      index_2++;
    } else if (constraint1.vars[index_1] > constraint2.vars[index_2]) {
      index_2++;
    } else {
      index_1++;
    }
  }
  return scalar_product;
}

LinearConstraintManager::~LinearConstraintManager() {
  if (num_merged_constraints_ > 0) {
    VLOG(2) << "num_merged_constraints: " << num_merged_constraints_;
  }
  for (const auto entry : type_to_num_cuts_) {
    VLOG(1) << "Added " << entry.second << " cuts of type '" << entry.first
            << "'.";
  }
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
    for (const IntegerVariable var : canonicalized.vars) {
      used_variables_.insert(var);
    }
    constraint_l2_norms_.push_back(ComputeL2Norm(canonicalized));
    equiv_constraints_[terms] = constraints_.size();
    constraint_is_in_lp_.push_back(false);
    constraints_.push_back(std::move(canonicalized));
  }
}

// Same as Add(), but logs some information about the newly added constraint.
// Cuts are also handled slightly differently than normal constraints.
void LinearConstraintManager::AddCut(
    const LinearConstraint& ct, std::string type_name,
    const gtl::ITIVector<IntegerVariable, double>& lp_solution) {
  CHECK_NE(ct.vars.size(), 0);
  Add(ct);
  num_cuts_++;
  type_to_num_cuts_[type_name]++;

  if (VLOG_IS_ON(1)) {
    IntegerValue max_magnitude(0);
    double activity = 0.0;
    const int size = ct.vars.size();
    for (int i = 0; i < size; ++i) {
      max_magnitude = std::max(max_magnitude, IntTypeAbs(ct.coeffs[i]));
      activity += ToDouble(ct.coeffs[i]) * lp_solution[ct.vars[i]];
    }
    double violation = 0.0;
    violation = std::max(violation, activity - ToDouble(ct.ub));
    violation = std::max(violation, ToDouble(ct.lb) - activity);
    LOG(INFO) << "Cut '" << type_name << "' size: " << size
              << " max_magnitude: " << max_magnitude
              << " violation: " << violation;
  }
}

bool LinearConstraintManager::ChangeLp(
    const gtl::ITIVector<IntegerVariable, double>& lp_solution) {
  const int old_num_constraints = lp_constraints_.size();

  std::vector<ConstraintIndex> new_constraints;

  // We keep any constraints that is already present, and otherwise, we add the
  // ones that are currently not satisfied by at least "tolerance".
  const double tolerance = 1e-6;
  for (ConstraintIndex i(0); i < constraints_.size(); ++i) {
    if (constraint_is_in_lp_[i]) continue;

    const double activity = ComputeActivity(constraints_[i], lp_solution);
    if (activity > ToDouble(constraints_[i].ub) + tolerance ||
        activity < ToDouble(constraints_[i].lb) - tolerance) {
      new_constraints.push_back(i);
    }
  }

  std::vector<ConstraintIndex> selected_constraints;
  for (ConstraintIndex new_constraint : new_constraints) {
    double orthogonality = 1.0;
    if (sat_parameters_.min_orthogonality_for_lp_constraints() > 0.0) {
      for (ConstraintIndex selected_constraint : selected_constraints) {
        double current_orthogonality =
            1.0 - (std::abs(ScalarProduct(constraints_[selected_constraint],
                                          constraints_[new_constraint])) /
                   (constraint_l2_norms_[selected_constraint] *
                    constraint_l2_norms_[new_constraint]));
        orthogonality = std::min(orthogonality, current_orthogonality);
        if (orthogonality <
            sat_parameters_.min_orthogonality_for_lp_constraints()) {
          break;
        }
      }
    }
    if (orthogonality >=
        sat_parameters_.min_orthogonality_for_lp_constraints()) {
      selected_constraints.push_back(new_constraint);
      constraint_is_in_lp_[new_constraint] = true;
      // Note that it is important for LP incremental solving that the old
      // constraints stays at the same position in this list (and thus in the
      // returned GetLp()).
      lp_constraints_.push_back(new_constraint);
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
