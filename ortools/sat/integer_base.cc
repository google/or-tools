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

#include <cstdint>
#include <numeric>
#include <utility>

#include "absl/log/check.h"

namespace operations_research::sat {

void LinearExpression2::SimpleCanonicalization() {
  if (coeffs[0] == 0) vars[0] = kNoIntegerVariable;
  if (coeffs[1] == 0) vars[1] = kNoIntegerVariable;

  // Corner case when the underlying variable is the same.
  if (vars[0] == vars[1]) {
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

void LinearExpression2::CanonicalizeAndUpdateBounds(IntegerValue& lb,
                                                    IntegerValue& ub) {
  // We need to be able to negate without overflow.
  CHECK_GE(lb, kMinIntegerValue);
  CHECK_LE(ub, kMaxIntegerValue);

  SimpleCanonicalization();
  if (coeffs[0] == 0 || coeffs[1] == 0) return;  // abort.

  bool negate = false;
  if (coeffs[0] == 0) {
    if (coeffs[1] != 0) {
      negate = !VariableIsPositive(vars[1]);
    }
  } else {
    negate = !VariableIsPositive(vars[0]);
  }
  if (negate) {
    Negate();
    std::swap(lb, ub);
    lb = -lb;
    ub = -ub;
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
}

bool BestBinaryRelationBounds::Add(LinearExpression2 expr, IntegerValue lb,
                                   IntegerValue ub) {
  expr.CanonicalizeAndUpdateBounds(lb, ub);
  if (expr.coeffs[0] == 0 || expr.coeffs[1] == 0) return false;

  auto [it, inserted] = best_bounds_.insert({expr, {lb, ub}});
  if (inserted) return true;

  const auto [known_lb, known_ub] = it->second;
  bool restricted = false;
  if (lb > known_lb) {
    it->second.first = lb;
    restricted = true;
  }
  if (ub < known_ub) {
    it->second.second = ub;
    restricted = true;
  }
  return restricted;
}

RelationStatus BestBinaryRelationBounds::GetStatus(LinearExpression2 expr,
                                                   IntegerValue lb,
                                                   IntegerValue ub) {
  expr.CanonicalizeAndUpdateBounds(lb, ub);
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

}  // namespace operations_research::sat
