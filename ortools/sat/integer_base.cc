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

#include "ortools/sat/integer_base.h"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"

namespace operations_research::sat {

void LinearExpression2::SimpleCanonicalization() {
  if (coeffs[0] == 0) vars[0] = kNoIntegerVariable;
  if (coeffs[1] == 0) vars[1] = kNoIntegerVariable;

  // Corner case when the underlying variable is the same.
  if (PositiveVariable(vars[0]) == PositiveVariable(vars[1])) {
    // Make sure variable are positive before merging.
    for (int i = 0; i < 2; ++i) {
      if (!VariableIsPositive(vars[i])) {
        coeffs[i] = -coeffs[i];
        vars[i] = NegationOf(vars[i]);
      }
    }

    coeffs[0] += coeffs[1];
    coeffs[1] = 0;
    vars[1] = kNoIntegerVariable;
    if (coeffs[0] == 0) vars[0] = kNoIntegerVariable;
  }

  // Make sure coeff are positive.
  for (int i = 0; i < 2; ++i) {
    if (coeffs[i] < 0) {
      coeffs[i] = -coeffs[i];
      vars[i] = NegationOf(vars[i]);
    }
  }

  // Make sure variable are sorted.
  if (vars[0] > vars[1]) {
    std::swap(vars[0], vars[1]);
    std::swap(coeffs[0], coeffs[1]);
  }
}

IntegerValue LinearExpression2::DivideByGcd() {
  const uint64_t gcd = std::gcd(coeffs[0].value(), coeffs[1].value());
  if (gcd > 1) {
    coeffs[0] /= gcd;
    coeffs[1] /= gcd;
    return IntegerValue(gcd);
  }
  return IntegerValue(1);
}

bool LinearExpression2::NegateForCanonicalization() {
  bool negate = false;
  if (coeffs[0] == 0) {
    if (coeffs[1] != 0) {
      negate = !VariableIsPositive(vars[1]);
    }
  } else {
    negate = !VariableIsPositive(vars[0]);
  }
  if (negate) Negate();
  return negate;
}

bool LinearExpression2::CanonicalizeAndUpdateBounds(IntegerValue& lb,
                                                    IntegerValue& ub,
                                                    bool allow_negation) {
  SimpleCanonicalization();
  if (coeffs[0] == 0 || coeffs[1] == 0) return false;  // abort.

  bool negated = false;
  if (allow_negation) {
    negated = NegateForCanonicalization();
    if (negated) {
      // We need to be able to negate without overflow.
      CHECK_GE(lb, kMinIntegerValue);
      CHECK_LE(ub, kMaxIntegerValue);
      std::swap(lb, ub);
      lb = -lb;
      ub = -ub;
    }
  }

  // Do gcd division.
  const uint64_t gcd = std::gcd(coeffs[0].value(), coeffs[1].value());
  if (gcd > 1) {
    coeffs[0] /= gcd;
    coeffs[1] /= gcd;
    ub = FloorRatio(ub, IntegerValue(gcd));
    lb = CeilRatio(lb, IntegerValue(gcd));
  }

  CHECK(coeffs[0] != 0 || vars[0] == kNoIntegerVariable);
  CHECK(coeffs[1] != 0 || vars[1] == kNoIntegerVariable);

  return negated;
}

bool LinearExpression2::IsCanonicalized() const {
  for (int i : {0, 1}) {
    if ((vars[i] == kNoIntegerVariable) != (coeffs[i] == 0)) {
      return false;
    }
  }
  if (vars[0] >= vars[1]) return false;

  if (vars[0] == kNoIntegerVariable) return true;

  return coeffs[0] > 0 && coeffs[1] > 0;
}

void LinearExpression2::MakeVariablesPositive() {
  SimpleCanonicalization();
  for (int i = 0; i < 2; ++i) {
    if (vars[i] != kNoIntegerVariable && !VariableIsPositive(vars[i])) {
      coeffs[i] = -coeffs[i];
      vars[i] = NegationOf(vars[i]);
    }
  }
}

std::pair<BestBinaryRelationBounds::AddResult,
          BestBinaryRelationBounds::AddResult>
BestBinaryRelationBounds::Add(LinearExpression2 expr, IntegerValue lb,
                              IntegerValue ub) {
  const bool negated =
      expr.CanonicalizeAndUpdateBounds(lb, ub, /*allow_negation=*/true);

  // We only store proper linear2.
  if (expr.coeffs[0] == 0 || expr.coeffs[1] == 0) {
    return {AddResult::INVALID, AddResult::INVALID};
  }

  auto [it, inserted] = best_bounds_.insert({expr, {lb, ub}});
  if (inserted) {
    std::pair<AddResult, AddResult> result = {
        lb > kMinIntegerValue ? AddResult::ADDED : AddResult::INVALID,
        ub < kMaxIntegerValue ? AddResult::ADDED : AddResult::INVALID};
    if (negated) std::swap(result.first, result.second);
    return result;
  }

  const auto [known_lb, known_ub] = it->second;

  std::pair<AddResult, AddResult> result = {
      lb > kMinIntegerValue ? AddResult::NOT_BETTER : AddResult::INVALID,
      ub < kMaxIntegerValue ? AddResult::NOT_BETTER : AddResult::INVALID};
  if (lb > known_lb) {
    result.first = (it->second.first == kMinIntegerValue) ? AddResult::ADDED
                                                          : AddResult::UPDATED;
    it->second.first = lb;
  }
  if (ub < known_ub) {
    result.second = (it->second.second == kMaxIntegerValue)
                        ? AddResult::ADDED
                        : AddResult::UPDATED;
    it->second.second = ub;
  }
  if (negated) std::swap(result.first, result.second);
  return result;
}

RelationStatus BestBinaryRelationBounds::GetStatus(LinearExpression2 expr,
                                                   IntegerValue lb,
                                                   IntegerValue ub) const {
  expr.CanonicalizeAndUpdateBounds(lb, ub, /*allow_negation=*/true);
  if (expr.coeffs[0] == 0 || expr.coeffs[1] == 0) {
    return RelationStatus::IS_UNKNOWN;
  }

  const auto it = best_bounds_.find(expr);
  if (it != best_bounds_.end()) {
    const auto [known_lb, known_ub] = it->second;
    if (lb <= known_lb && ub >= known_ub) return RelationStatus::IS_TRUE;
    if (lb > known_ub || ub < known_lb) return RelationStatus::IS_FALSE;
  }
  return RelationStatus::IS_UNKNOWN;
}

IntegerValue BestBinaryRelationBounds::GetUpperBound(
    LinearExpression2 expr) const {
  expr.SimpleCanonicalization();
  const IntegerValue gcd = expr.DivideByGcd();
  const bool negated = expr.NegateForCanonicalization();
  const auto it = best_bounds_.find(expr);
  if (it != best_bounds_.end()) {
    const auto [known_lb, known_ub] = it->second;
    if (negated) {
      return CapProdI(gcd, -known_lb);
    } else {
      return CapProdI(gcd, known_ub);
    }
  }
  return kMaxIntegerValue;
}

// TODO(user): Maybe introduce a CanonicalizedLinear2 class so we automatically
// get the better function, and it documents when we have canonicalized
// expression.
IntegerValue BestBinaryRelationBounds::UpperBoundWhenCanonicalized(
    LinearExpression2 expr) const {
  DCHECK_EQ(expr.DivideByGcd(), 1);
  DCHECK(expr.IsCanonicalized());
  const bool negated = expr.NegateForCanonicalization();
  const auto it = best_bounds_.find(expr);
  if (it != best_bounds_.end()) {
    const auto [known_lb, known_ub] = it->second;
    if (negated) {
      return -known_lb;
    } else {
      return known_ub;
    }
  }
  return kMaxIntegerValue;
}

std::vector<std::pair<LinearExpression2, IntegerValue>>
BestBinaryRelationBounds::GetSortedNonTrivialUpperBounds() const {
  std::vector<std::pair<LinearExpression2, IntegerValue>> root_relations_sorted;
  root_relations_sorted.reserve(2 * best_bounds_.size());
  for (const auto& [expr, bounds] : best_bounds_) {
    if (bounds.first != kMinIntegerValue) {
      LinearExpression2 negated_expr = expr;
      negated_expr.Negate();
      root_relations_sorted.push_back({negated_expr, -bounds.first});
    }
    if (bounds.second != kMaxIntegerValue) {
      root_relations_sorted.push_back({expr, bounds.second});
    }
  }
  std::sort(root_relations_sorted.begin(), root_relations_sorted.end());
  return root_relations_sorted;
}

std::vector<std::tuple<LinearExpression2, IntegerValue, IntegerValue>>
BestBinaryRelationBounds::GetSortedNonTrivialBounds() const {
  std::vector<std::tuple<LinearExpression2, IntegerValue, IntegerValue>>
      root_relations_sorted;
  root_relations_sorted.reserve(best_bounds_.size());
  for (const auto& [expr, bounds] : best_bounds_) {
    root_relations_sorted.push_back({expr, bounds.first, bounds.second});
  }
  std::sort(root_relations_sorted.begin(), root_relations_sorted.end());
  return root_relations_sorted;
}

}  // namespace operations_research::sat
