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
    const int index = gtl::FindOrDieNoPrint(equiv_constraints_, terms);
    if (canonicalized.lb > constraints_[index].lb) {
      some_constraint_changed_ = true;
      constraints_[index].lb = canonicalized.lb;
    }
    if (canonicalized.ub < constraints_[index].ub) {
      some_constraint_changed_ = true;
      constraints_[index].ub = canonicalized.ub;
    }
    ++num_merged_constraints_;
  } else {
    for (const IntegerVariable var : canonicalized.vars) {
      used_variables_.insert(var);
    }
    equiv_constraints_[terms] = constraints_.size();
    constraint_in_lp_.push_back(false);
    constraints_.push_back(std::move(canonicalized));
    some_constraint_changed_ = true;
  }
}

bool LinearConstraintManager::LpShouldBeChanged() {
  if (some_constraint_changed_) return true;

  // TODO(user): for now we limit the total number of constraint in the LP
  // to be twice the total number of variables.
  const size_t max_num_constraints = used_variables_.size() * 2;
  return num_constraints_in_lp_ <
         std::min(constraints_.size(), max_num_constraints);
}

// TODO(user): Implement, for now we just return all the constraint we know
// about. We could at least only return an "increasing" list of constraint, but
// never add the one currently satisfied by the given solution.
std::vector<const LinearConstraint*> LinearConstraintManager::GetLp(
    const gtl::ITIVector<IntegerVariable, double>& lp_solution) {
  std::vector<const LinearConstraint*> result;
  for (int i = 0; i < constraints_.size(); ++i) {
    constraint_in_lp_[i] = true;
    result.push_back(&constraints_[i]);
  }
  num_constraints_in_lp_ = result.size();
  some_constraint_changed_ = false;
  return result;
}

}  // namespace sat
}  // namespace operations_research
