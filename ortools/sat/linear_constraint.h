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

#ifndef OR_TOOLS_SAT_LINEAR_CONSTRAINT_H_
#define OR_TOOLS_SAT_LINEAR_CONSTRAINT_H_

#include <vector>

#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

// One linear constraint on a set of Integer variables.
// Important: there should be no duplicate variables.
//
// We also assume that we never have integer overflow when evaluating such
// constraint. This should be enforced by the checker for user given
// constraints, and we must enforce it ourselves for the newly created
// constraint. We requires:
//  -  sum_i max(0, max(c_i * lb_i, c_i * ub_i)) < kMaxIntegerValue.
//  -  sum_i min(0, min(c_i * lb_i, c_i * ub_i)) > kMinIntegerValue
// so that in whichever order we compute the sum, we have no overflow. Note
// that this condition invoves the bounds of the variables.
//
// TODO(user): Add DCHECKs for the no-overflow property? but we need access
// to the variable bounds.
struct LinearConstraint {
  IntegerValue lb;
  IntegerValue ub;
  std::vector<IntegerVariable> vars;
  std::vector<IntegerValue> coeffs;

  LinearConstraint() {}
  LinearConstraint(IntegerValue _lb, IntegerValue _ub) : lb(_lb), ub(_ub) {}

  void AddTerm(IntegerVariable var, IntegerValue coeff) {
    vars.push_back(var);
    coeffs.push_back(coeff);
  }

  std::string DebugString() const {
    std::string result;
    if (lb.value() > kMinIntegerValue) {
      absl::StrAppend(&result, lb.value(), " <= ");
    }
    for (int i = 0; i < vars.size(); ++i) {
      const IntegerValue coeff =
          VariableIsPositive(vars[i]) ? coeffs[i] : -coeffs[i];
      absl::StrAppend(&result, coeff.value(), "*X", vars[i].value() / 2, " ");
    }
    if (ub.value() < kMaxIntegerValue) {
      absl::StrAppend(&result, "<= ", ub.value());
    }
    return result;
  }

  bool operator==(const LinearConstraint other) const {
    if (this->lb != other.lb) return false;
    if (this->ub != other.ub) return false;
    if (this->vars != other.vars) return false;
    if (this->coeffs != other.coeffs) return false;
    return true;
  }
};

// Allow to build a LinearConstraint while making sure there is no duplicate
// variables.
//
// TODO(user): Storing all coeff in the vector then sorting and merging
// duplicates might be more efficient. Change if required.
class LinearConstraintBuilder {
 public:
  // We support "sticky" kMinIntegerValue for lb and kMaxIntegerValue for ub
  // for one-sided constraints.
  LinearConstraintBuilder(const Model* model, IntegerValue lb, IntegerValue ub)
      : assignment_(model->Get<Trail>()->Assignment()),
        encoder_(*model->Get<IntegerEncoder>()),
        lb_(lb),
        ub_(ub) {}

  int size() const { return terms_.size(); }
  bool IsEmpty() const { return terms_.empty(); }

  // Adds var * coeff to the constraint.
  void AddTerm(IntegerVariable var, IntegerValue coeff) {
    // We can either add var or NegationOf(var), and we always choose the
    // positive one.
    if (VariableIsPositive(var)) {
      terms_[var] += coeff;
      if (terms_[var] == 0) terms_.erase(var);
    } else {
      const IntegerVariable minus_var = NegationOf(var);
      terms_[minus_var] -= coeff;
      if (terms_[minus_var] == 0) terms_.erase(minus_var);
    }
  }

  // Add literal * coeff to the constaint. Returns false and do nothing if the
  // given literal didn't have an integer view.
  ABSL_MUST_USE_RESULT bool AddLiteralTerm(Literal lit, IntegerValue coeff) {
    if (assignment_.LiteralIsTrue(lit)) {
      if (lb_ > kMinIntegerValue) lb_ -= coeff;
      if (ub_ < kMaxIntegerValue) ub_ -= coeff;
      return true;
    }
    if (assignment_.LiteralIsFalse(lit)) {
      return true;
    }

    bool has_direct_view = encoder_.GetLiteralView(lit) != kNoIntegerVariable;
    bool has_opposite_view =
        encoder_.GetLiteralView(lit.Negated()) != kNoIntegerVariable;

    // If a literal has both views, we want to always keep the same
    // representative: the smallest IntegerVariable. Note that AddTerm() will
    // also make sure to use the associated positive variable.
    if (has_direct_view && has_opposite_view) {
      if (encoder_.GetLiteralView(lit) <=
          encoder_.GetLiteralView(lit.Negated())) {
        has_direct_view = true;
        has_opposite_view = false;
      } else {
        has_direct_view = false;
        has_opposite_view = true;
      }
    }
    if (has_direct_view) {
      AddTerm(encoder_.GetLiteralView(lit), coeff);
      return true;
    }
    if (has_opposite_view) {
      AddTerm(encoder_.GetLiteralView(lit.Negated()), -coeff);
      if (lb_ > kMinIntegerValue) lb_ -= coeff;
      if (ub_ < kMaxIntegerValue) ub_ -= coeff;
      return true;
    }
    return false;
  }

  LinearConstraint Build() {
    LinearConstraint result;
    result.lb = lb_;
    result.ub = ub_;
    for (const auto entry : terms_) {
      result.vars.push_back(entry.first);
      result.coeffs.push_back(entry.second);
    }
    return result;
  }

 private:
  const VariablesAssignment& assignment_;
  const IntegerEncoder& encoder_;
  IntegerValue lb_;
  IntegerValue ub_;
  IntegerValue offset_;
  std::map<IntegerVariable, IntegerValue> terms_;
};

// Returns the activity of the given constraint. That is the current value of
// the linear terms.
double ComputeActivity(const LinearConstraint& constraint,
                       const gtl::ITIVector<IntegerVariable, double>& values);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_CONSTRAINT_H_
