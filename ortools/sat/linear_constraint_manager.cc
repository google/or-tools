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

// Because sometimes we split a == constraint in two (>= and <=), it makes sense
// to detect duplicate constraints and merge bounds. This is also relevant if
// we regenerate identical cuts for some reason.
void LinearConstraintManager::Add(const LinearConstraint& ct) {
  LinearConstraint canonicalized = ct;
  const Terms terms = CanonicalizeConstraintAndGetTerms(&canonicalized);

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
    equiv_constraints_[terms] = constraints_.size();
    constraint_is_in_lp_.push_back(false);
    constraints_.push_back(std::move(canonicalized));
  }
}

bool LinearConstraintManager::ChangeLp(
    const gtl::ITIVector<IntegerVariable, double>& lp_solution) {
  const int old_num_constraints = lp_constraints_.size();

  // We keep any constraints that is already present, and otherwise, we add the
  // ones that are currently not satisfied by at least "tolerance".
  const double tolerance = 1e-6;
  for (ConstraintIndex i(0); i < constraints_.size(); ++i) {
    if (constraint_is_in_lp_[i]) continue;

    const double activity = ComputeActivity(constraints_[i], lp_solution);
    if (activity > ToDouble(constraints_[i].ub) + tolerance ||
        activity < ToDouble(constraints_[i].lb) - tolerance) {
      constraint_is_in_lp_[i] = true;

      // Note that it is important for LP incremental solving that the old
      // constraints stays at the same position in this list (and thus in the
      // returned GetLp()).
      lp_constraints_.push_back(i);
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
