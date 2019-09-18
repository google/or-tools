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

#include "ortools/sat/linear_constraint.h"

#include "ortools/base/mathutil.h"

namespace operations_research {
namespace sat {

void LinearConstraintBuilder::AddTerm(IntegerVariable var, IntegerValue coeff) {
  // We can either add var or NegationOf(var), and we always choose the
  // positive one.
  if (VariableIsPositive(var)) {
    terms_.push_back({var, coeff});
  } else {
    const IntegerVariable negated_var = NegationOf(var);
    terms_.push_back({negated_var, -coeff});
  }
}

ABSL_MUST_USE_RESULT bool LinearConstraintBuilder::AddLiteralTerm(
    Literal lit, IntegerValue coeff) {
  bool has_direct_view = encoder_.GetLiteralView(lit) != kNoIntegerVariable;
  bool has_opposite_view =
      encoder_.GetLiteralView(lit.Negated()) != kNoIntegerVariable;

  // If a literal has both views, we want to always keep the same
  // representative: the smallest IntegerVariable. Note that AddTerm() will
  // also make sure to use the associated positive variable.
  if (has_direct_view && has_opposite_view) {
    if (encoder_.GetLiteralView(lit) <=
        encoder_.GetLiteralView(lit.Negated())) {
      has_opposite_view = false;
    } else {
      has_direct_view = false;
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

void CleanTermsAndFillConstraint(
    std::vector<std::pair<IntegerVariable, IntegerValue>>* terms,
    LinearConstraint* constraint) {
  constraint->vars.clear();
  constraint->coeffs.clear();

  // Sort and add coeff of duplicate variables.
  std::sort(terms->begin(), terms->end());
  IntegerVariable previous_var = kNoIntegerVariable;
  IntegerValue current_coeff(0);
  for (const auto entry : *terms) {
    if (previous_var == entry.first) {
      current_coeff += entry.second;
    } else {
      if (current_coeff != 0) {
        constraint->vars.push_back(previous_var);
        constraint->coeffs.push_back(current_coeff);
      }
      previous_var = entry.first;
      current_coeff = entry.second;
    }
  }
  if (current_coeff != 0) {
    constraint->vars.push_back(previous_var);
    constraint->coeffs.push_back(current_coeff);
  }
}

LinearConstraint LinearConstraintBuilder::Build() {
  LinearConstraint result;
  result.lb = lb_;
  result.ub = ub_;
  CleanTermsAndFillConstraint(&terms_, &result);
  return result;
}

double ComputeActivity(const LinearConstraint& constraint,
                       const gtl::ITIVector<IntegerVariable, double>& values) {
  double activity = 0;
  for (int i = 0; i < constraint.vars.size(); ++i) {
    const IntegerVariable var = constraint.vars[i];
    const IntegerValue coeff = constraint.coeffs[i];
    activity += coeff.value() * values[var];
  }
  return activity;
}

double ComputeL2Norm(const LinearConstraint& constraint) {
  double sum = 0.0;
  for (const IntegerValue coeff : constraint.coeffs) {
    sum += ToDouble(coeff) * ToDouble(coeff);
  }
  return std::sqrt(sum);
}

IntegerValue ComputeInfinityNorm(const LinearConstraint& constraint) {
  IntegerValue result(0);
  for (const IntegerValue coeff : constraint.coeffs) {
    result = std::max(result, IntTypeAbs(coeff));
  }
  return result;
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

namespace {

// TODO(user): Template for any integer type and expose this?
IntegerValue ComputeGcd(const std::vector<IntegerValue>& values) {
  if (values.empty()) return IntegerValue(1);
  int64 gcd = 0;
  for (const IntegerValue value : values) {
    gcd = MathUtil::GCD64(gcd, std::abs(value.value()));
    if (gcd == 1) break;
  }
  return IntegerValue(gcd);
}

}  // namespace

void DivideByGCD(LinearConstraint* constraint) {
  if (constraint->coeffs.empty()) return;
  const IntegerValue gcd = ComputeGcd(constraint->coeffs);
  if (gcd == 1) return;

  if (constraint->lb > kMinIntegerValue) {
    constraint->lb = CeilRatio(constraint->lb, gcd);
  }
  if (constraint->ub < kMaxIntegerValue) {
    constraint->ub = FloorRatio(constraint->ub, gcd);
  }
  for (IntegerValue& coeff : constraint->coeffs) coeff /= gcd;
}

void RemoveZeroTerms(LinearConstraint* constraint) {
  int new_size = 0;
  const int size = constraint->vars.size();
  for (int i = 0; i < size; ++i) {
    if (constraint->coeffs[i] == 0) continue;
    constraint->vars[new_size] = constraint->vars[i];
    constraint->coeffs[new_size] = constraint->coeffs[i];
    ++new_size;
  }
  constraint->vars.resize(new_size);
  constraint->coeffs.resize(new_size);
}

void MakeAllCoefficientsPositive(LinearConstraint* constraint) {
  const int size = constraint->vars.size();
  for (int i = 0; i < size; ++i) {
    const IntegerValue coeff = constraint->coeffs[i];
    if (coeff < 0) {
      constraint->coeffs[i] = -coeff;
      constraint->vars[i] = NegationOf(constraint->vars[i]);
    }
  }
}

void MakeAllVariablesPositive(LinearConstraint* constraint) {
  const int size = constraint->vars.size();
  for (int i = 0; i < size; ++i) {
    const IntegerVariable var = constraint->vars[i];
    if (!VariableIsPositive(var)) {
      constraint->coeffs[i] = -constraint->coeffs[i];
      constraint->vars[i] = NegationOf(var);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
