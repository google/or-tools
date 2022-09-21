// Copyright 2010-2022 Google LLC
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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

void LinearConstraintBuilder::AddTerm(IntegerVariable var, IntegerValue coeff) {
  if (coeff == 0) return;
  // We can either add var or NegationOf(var), and we always choose the
  // positive one.
  if (VariableIsPositive(var)) {
    terms_.push_back({var, coeff});
  } else {
    terms_.push_back({NegationOf(var), -coeff});
  }
}

void LinearConstraintBuilder::AddTerm(AffineExpression expr,
                                      IntegerValue coeff) {
  if (coeff == 0) return;
  // We can either add var or NegationOf(var), and we always choose the
  // positive one.
  if (expr.var != kNoIntegerVariable) {
    if (VariableIsPositive(expr.var)) {
      terms_.push_back({expr.var, coeff * expr.coeff});
    } else {
      terms_.push_back({NegationOf(expr.var), -coeff * expr.coeff});
    }
  }
  offset_ += coeff * expr.constant;
}

void LinearConstraintBuilder::AddLinearExpression(
    const LinearExpression& expr) {
  AddLinearExpression(expr, IntegerValue(1));
}

void LinearConstraintBuilder::AddLinearExpression(const LinearExpression& expr,
                                                  IntegerValue coeff) {
  for (int i = 0; i < expr.vars.size(); ++i) {
    // We must use positive variables.
    if (VariableIsPositive(expr.vars[i])) {
      terms_.push_back({expr.vars[i], expr.coeffs[i] * coeff});
    } else {
      terms_.push_back({NegationOf(expr.vars[i]), -expr.coeffs[i] * coeff});
    }
  }
  offset_ += expr.offset * coeff;
}

ABSL_MUST_USE_RESULT bool LinearConstraintBuilder::AddDecomposedProduct(
    const std::vector<LiteralValueValue>& product) {
  if (product.empty()) return true;

  IntegerValue product_min = kMaxIntegerValue;
  // TODO(user): Checks the value of literals.
  for (const LiteralValueValue& term : product) {
    product_min = std::min(product_min, term.left_value * term.right_value);
  }

  for (const LiteralValueValue& term : product) {
    IntegerValue coeff = term.left_value * term.right_value - product_min;
    if (coeff == 0) continue;
    if (!AddLiteralTerm(term.literal, coeff)) {
      return false;
    }
  }
  AddConstant(product_min);
  return true;
}

void LinearConstraintBuilder::AddQuadraticLowerBound(
    AffineExpression left, AffineExpression right, IntegerTrail* integer_trail,
    bool* is_quadratic) {
  if (integer_trail->IsFixed(left)) {
    AddTerm(right, integer_trail->FixedValue(left));
  } else if (integer_trail->IsFixed(right)) {
    AddTerm(left, integer_trail->FixedValue(right));
  } else {
    const IntegerValue left_min = integer_trail->LowerBound(left);
    const IntegerValue right_min = integer_trail->LowerBound(right);
    AddTerm(left, right_min);
    AddTerm(right, left_min);
    // Substract the energy counted twice.
    AddConstant(-left_min * right_min);
    if (is_quadratic != nullptr) *is_quadratic = true;
  }
}

void LinearConstraintBuilder::AddConstant(IntegerValue value) {
  offset_ += value;
}

ABSL_MUST_USE_RESULT bool LinearConstraintBuilder::AddLiteralTerm(
    Literal lit, IntegerValue coeff) {
  DCHECK(encoder_ != nullptr);
  IntegerVariable var = kNoIntegerVariable;
  bool view_is_direct = true;
  if (!encoder_->LiteralOrNegationHasView(lit, &var, &view_is_direct)) {
    return false;
  }

  if (view_is_direct) {
    AddTerm(var, coeff);
  } else {
    AddTerm(var, -coeff);
    offset_ += coeff;
  }
  return true;
}

LinearConstraint LinearConstraintBuilder::Build() {
  return BuildConstraint(lb_, ub_);
}

LinearConstraint LinearConstraintBuilder::BuildConstraint(IntegerValue lb,
                                                          IntegerValue ub) {
  LinearConstraint result;
  result.lb = lb > kMinIntegerValue ? lb - offset_ : lb;
  result.ub = ub < kMaxIntegerValue ? ub - offset_ : ub;
  CleanTermsAndFillConstraint(&terms_, &result);
  return result;
}

LinearExpression LinearConstraintBuilder::BuildExpression() {
  LinearExpression result;
  CleanTermsAndFillConstraint(&terms_, &result);
  result.offset = offset_;
  return result;
}

double ComputeActivity(
    const LinearConstraint& constraint,
    const absl::StrongVector<IntegerVariable, double>& values) {
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
  int64_t gcd = 0;
  for (const IntegerValue value : values) {
    gcd = MathUtil::GCD64(gcd, std::abs(value.value()));
    if (gcd == 1) break;
  }
  if (gcd < 0) return IntegerValue(1);  // Can happen with kint64min.
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

double LinearExpression::LpValue(
    const absl::StrongVector<IntegerVariable, double>& lp_values) const {
  double result = ToDouble(offset);
  for (int i = 0; i < vars.size(); ++i) {
    result += ToDouble(coeffs[i]) * lp_values[vars[i]];
  }
  return result;
}

IntegerValue LinearExpression::LevelZeroMin(IntegerTrail* integer_trail) const {
  IntegerValue result = offset;
  for (int i = 0; i < vars.size(); ++i) {
    DCHECK_GE(coeffs[i], 0);
    result += coeffs[i] * integer_trail->LevelZeroLowerBound(vars[i]);
  }
  return result;
}

IntegerValue LinearExpression::Min(const IntegerTrail& integer_trail) const {
  IntegerValue result = offset;
  for (int i = 0; i < vars.size(); ++i) {
    if (coeffs[i] > 0) {
      result += coeffs[i] * integer_trail.LowerBound(vars[i]);
    } else {
      result += coeffs[i] * integer_trail.UpperBound(vars[i]);
    }
  }
  return result;
}

IntegerValue LinearExpression::Max(const IntegerTrail& integer_trail) const {
  IntegerValue result = offset;
  for (int i = 0; i < vars.size(); ++i) {
    if (coeffs[i] > 0) {
      result += coeffs[i] * integer_trail.UpperBound(vars[i]);
    } else {
      result += coeffs[i] * integer_trail.LowerBound(vars[i]);
    }
  }
  return result;
}

std::string LinearExpression::DebugString() const {
  if (vars.empty()) return absl::StrCat(offset.value());
  std::string result;
  for (int i = 0; i < vars.size(); ++i) {
    absl::StrAppend(&result, i > 0 ? " " : "",
                    IntegerTermDebugString(vars[i], coeffs[i]));
  }
  if (offset != 0) {
    absl::StrAppend(&result, " + ", offset.value());
  }
  return result;
}

// TODO(user): it would be better if LinearConstraint natively supported
// term and not two separated vectors. Fix?
//
// TODO(user): This is really similar to CleanTermsAndFillConstraint(), maybe
// we should just make the later switch negative variable to positive ones to
// avoid an extra linear scan on each new cuts.
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

bool NoDuplicateVariable(const LinearConstraint& ct) {
  absl::flat_hash_set<IntegerVariable> seen_variables;
  const int size = ct.vars.size();
  for (int i = 0; i < size; ++i) {
    if (VariableIsPositive(ct.vars[i])) {
      if (!seen_variables.insert(ct.vars[i]).second) return false;
    } else {
      if (!seen_variables.insert(NegationOf(ct.vars[i])).second) return false;
    }
  }
  return true;
}

LinearExpression CanonicalizeExpr(const LinearExpression& expr) {
  LinearExpression canonical_expr;
  canonical_expr.offset = expr.offset;
  for (int i = 0; i < expr.vars.size(); ++i) {
    if (expr.coeffs[i] < 0) {
      canonical_expr.vars.push_back(NegationOf(expr.vars[i]));
      canonical_expr.coeffs.push_back(-expr.coeffs[i]);
    } else {
      canonical_expr.vars.push_back(expr.vars[i]);
      canonical_expr.coeffs.push_back(expr.coeffs[i]);
    }
  }
  return canonical_expr;
}

// TODO(user): Avoid duplication with PossibleIntegerOverflow() in the checker?
// At least make sure the code is the same.
bool ValidateLinearConstraintForOverflow(const LinearConstraint& constraint,
                                         const IntegerTrail& integer_trail) {
  int64_t positive_sum(0);
  int64_t negative_sum(0);
  for (int i = 0; i < constraint.vars.size(); ++i) {
    const IntegerVariable var = constraint.vars[i];
    const IntegerValue coeff = constraint.coeffs[i];
    const IntegerValue lb = integer_trail.LevelZeroLowerBound(var);
    const IntegerValue ub = integer_trail.LevelZeroUpperBound(var);

    int64_t min_prod = CapProd(coeff.value(), lb.value());
    int64_t max_prod = CapProd(coeff.value(), ub.value());
    if (min_prod > max_prod) std::swap(min_prod, max_prod);

    positive_sum = CapAdd(positive_sum, std::max(int64_t{0}, max_prod));
    negative_sum = CapAdd(negative_sum, std::min(int64_t{0}, min_prod));
  }

  const int64_t limit = std::numeric_limits<int64_t>::max();
  if (positive_sum >= limit) return false;
  if (negative_sum <= -limit) return false;
  if (CapSub(positive_sum, negative_sum) >= limit) return false;

  return true;
}

LinearExpression NegationOf(const LinearExpression& expr) {
  LinearExpression result;
  result.vars = NegationOf(expr.vars);
  result.coeffs = expr.coeffs;
  result.offset = -expr.offset;
  return result;
}

LinearExpression PositiveVarExpr(const LinearExpression& expr) {
  LinearExpression result;
  result.offset = expr.offset;
  for (int i = 0; i < expr.vars.size(); ++i) {
    if (VariableIsPositive(expr.vars[i])) {
      result.vars.push_back(expr.vars[i]);
      result.coeffs.push_back(expr.coeffs[i]);
    } else {
      result.vars.push_back(NegationOf(expr.vars[i]));
      result.coeffs.push_back(-expr.coeffs[i]);
    }
  }
  return result;
}

IntegerValue GetCoefficient(const IntegerVariable var,
                            const LinearExpression& expr) {
  for (int i = 0; i < expr.vars.size(); ++i) {
    if (expr.vars[i] == var) {
      return expr.coeffs[i];
    } else if (expr.vars[i] == NegationOf(var)) {
      return -expr.coeffs[i];
    }
  }
  return IntegerValue(0);
}

IntegerValue GetCoefficientOfPositiveVar(const IntegerVariable var,
                                         const LinearExpression& expr) {
  CHECK(VariableIsPositive(var));
  for (int i = 0; i < expr.vars.size(); ++i) {
    if (expr.vars[i] == var) {
      return expr.coeffs[i];
    }
  }
  return IntegerValue(0);
}

}  // namespace sat
}  // namespace operations_research
