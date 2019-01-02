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

namespace operations_research {
namespace sat {

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

namespace {

// TODO(user): Template for any integer type and expose this?
IntegerValue ComputeGcd(const std::vector<IntegerValue>& values) {
  if (values.empty()) return IntegerValue(1);
  IntegerValue gcd = IntTypeAbs(values.front());
  const int size = values.size();
  for (int i = 1; i < size; ++i) {
    // GCD(gcd, value) = GCD(value, gcd % value);
    IntegerValue value = IntTypeAbs(values[i]);
    while (value != 0) {
      const IntegerValue r = gcd % value;
      gcd = value;
      value = r;
    }
    if (gcd == 1) break;
  }
  return gcd;
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

}  // namespace sat
}  // namespace operations_research
