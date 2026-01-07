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
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"

namespace operations_research::sat {
namespace {

TEST(CanonicalizeAffinePrecedenceTest, Basic) {
  LinearExpression2 expr;
  CHECK(expr.IsCanonicalized()) << expr;
  expr.vars[0] = IntegerVariable(0);
  expr.vars[1] = IntegerVariable(2);
  expr.coeffs[0] = IntegerValue(4);
  expr.coeffs[1] = IntegerValue(2);

  IntegerValue lb(0);
  IntegerValue ub(11);
  expr.CanonicalizeAndUpdateBounds(lb, ub);
  CHECK(expr.IsCanonicalized());

  EXPECT_EQ(expr.vars[0], IntegerVariable(0));
  EXPECT_EQ(expr.vars[1], IntegerVariable(2));
  EXPECT_EQ(expr.coeffs[0], IntegerValue(2));
  EXPECT_EQ(expr.coeffs[1], IntegerValue(1));
  EXPECT_EQ(lb, 0);
  EXPECT_EQ(ub, 5);
}

TEST(CanonicalizeAffinePrecedenceTest, OneSingleVariable) {
  LinearExpression2 expr;
  expr.vars[0] = IntegerVariable(0);
  expr.vars[1] = IntegerVariable(0);
  expr.coeffs[0] = IntegerValue(2);
  expr.coeffs[1] = IntegerValue(2);

  expr.SimpleCanonicalization();
  CHECK(expr.IsCanonicalized());

  EXPECT_EQ(expr.vars[0], kNoIntegerVariable);
  EXPECT_EQ(expr.vars[1], IntegerVariable(0));
  EXPECT_EQ(expr.coeffs[0], IntegerValue(0));
  EXPECT_EQ(expr.coeffs[1], IntegerValue(4));
}

TEST(BestBinaryRelationBoundsTest, Basic) {
  LinearExpression2 expr;
  expr.vars[0] = IntegerVariable(0);
  expr.vars[1] = IntegerVariable(2);
  expr.coeffs[0] = IntegerValue(1);
  expr.coeffs[1] = IntegerValue(-1);

  using AddResult = BestBinaryRelationBounds::AddResult;

  BestBinaryRelationBounds best_bounds;
  EXPECT_EQ(best_bounds.Add(expr, IntegerValue(0), IntegerValue(5)),
            std::make_pair(AddResult::ADDED, AddResult::ADDED));
  EXPECT_EQ(best_bounds.Add(expr, IntegerValue(3), IntegerValue(8)),
            std::make_pair(AddResult::UPDATED, AddResult::NOT_BETTER));
  EXPECT_EQ(best_bounds.Add(expr, IntegerValue(-1), IntegerValue(4)),
            std::make_pair(AddResult::NOT_BETTER, AddResult::UPDATED));
  EXPECT_EQ(best_bounds.Add(expr, IntegerValue(3), IntegerValue(4)),  // best
            std::make_pair(AddResult::NOT_BETTER, AddResult::NOT_BETTER));

  EXPECT_EQ(RelationStatus::IS_TRUE,
            best_bounds.GetStatus(expr, IntegerValue(-10), IntegerValue(4)));
  EXPECT_EQ(RelationStatus::IS_TRUE,
            best_bounds.GetStatus(expr, IntegerValue(0), IntegerValue(20)));
  EXPECT_EQ(RelationStatus::IS_FALSE,
            best_bounds.GetStatus(expr, IntegerValue(5), IntegerValue(20)));
  EXPECT_EQ(RelationStatus::IS_FALSE,
            best_bounds.GetStatus(expr, IntegerValue(-5), IntegerValue(2)));
  EXPECT_EQ(RelationStatus::IS_UNKNOWN,
            best_bounds.GetStatus(expr, IntegerValue(-5), IntegerValue(3)));
}

AffineExpression OtherAffineLowerBound(LinearExpression2 expr, int var_index,
                                       IntegerValue expr_lb,
                                       IntegerValue other_var_lb) {
  const IntegerValue coeff = expr.coeffs[var_index];
  const IntegerVariable var = expr.vars[var_index];
  DCHECK_NE(var, kNoIntegerVariable);
  const IntegerVariable other_var = expr.vars[1 - var_index];
  const IntegerValue other_coeff = expr.coeffs[1 - var_index];

  const IntegerValue ceil_coeff_ratio = CeilRatio(other_coeff, coeff);
  // a * x >= expr_lb - (y-lby +lby)*b
  // a * x >= (expr_lb - b * lby) - (y - lby) * b , with (y - lby) positive here
  //     x >= Floor(expr_lb  - b * lby, a) + Floor(-b, a) * (y -lby)
  //     x >= Floor(-b, a) * y  +  [ Floor(expr_lb - b * lby, a)
  //                                 - Floor(-b,a)*lby]

  return AffineExpression(
      other_var, -ceil_coeff_ratio,
      FloorRatio(expr_lb - other_coeff * other_var_lb, coeff) -
          FloorRatio(-other_coeff, coeff) * other_var_lb);
}

TEST(Linear2BoundAffineRelaxationTest, Random) {
  absl::BitGen random;
  for (int i = 0; i < 10000; ++i) {
    LinearExpression2 expr;
    expr.vars[0] = IntegerVariable(0);
    expr.vars[1] = IntegerVariable(2);
    expr.coeffs[0] = absl::Uniform<int64_t>(random, 1, 30);
    expr.coeffs[1] = absl::Uniform<int64_t>(random, 1, 30);
    expr.SimpleCanonicalization();
    expr.DivideByGcd();
    const IntegerValue lb = absl::Uniform<int64_t>(random, -100, 100);
    const IntegerValue other_lb = absl::Uniform<int64_t>(random, -100, 50);
    const IntegerValue other_ub =
        other_lb + absl::Uniform<int64_t>(random, 0, 50);
    IntegerValue computed_slack = kMaxIntegerValue;
    bool trivial = true;
    bool logged = false;

    // Find a tight lower bound for expr.vars[1]. This is not necessary for
    // correctness, but it is necessary if we want to prove that the bound
    // returned by GetAffineLowerBound() is tight.
    IntegerValue tight_lb = kMaxIntegerValue;
    for (IntegerValue var1_val = other_lb; var1_val <= other_ub; ++var1_val) {
      for (IntegerValue var0_val = -100; var0_val < 100; ++var0_val) {
        const IntegerValue expr_value =
            expr.coeffs[0] * var0_val + expr.coeffs[1] * var1_val;
        if (expr_value >= lb) {
          tight_lb = var1_val;
          break;
        }
      }
      if (tight_lb != kMaxIntegerValue) {
        break;
      }
    }

    if (tight_lb == kMaxIntegerValue) {
      // Unsat linear2
      continue;
    }
    const AffineExpression affine = expr.GetAffineLowerBound(0, lb, tight_lb);

    // Compare with the other formula that only differs in the constant term.
    const AffineExpression other_affine =
        OtherAffineLowerBound(expr, 0, lb, tight_lb);
    CHECK_EQ(affine.coeff, other_affine.coeff);
    CHECK_NE(affine.coeff, 0);
    CHECK_EQ(affine.var, other_affine.var);

    CHECK_EQ(PositiveVariable(affine.var), expr.vars[1]);
    const IntegerValue affine_coeff =
        VariableIsPositive(affine.var) ? affine.coeff : -affine.coeff;

    for (IntegerValue var0_val = -100; var0_val < 100; ++var0_val) {
      for (IntegerValue var1_val = other_lb; var1_val <= other_ub; ++var1_val) {
        const IntegerValue expr_value =
            expr.coeffs[0] * var0_val + expr.coeffs[1] * var1_val;
        if (expr_value < lb) {
          // Our affine bound only works if expr >= lb.
          trivial = false;
          continue;
        }
        const IntegerValue affine_value =
            affine.constant + var1_val * affine_coeff;
        const IntegerValue other_affine_value =
            other_affine.constant + var1_val * affine_coeff;
        CHECK_GE(var0_val, affine_value);  // expr.vars[var_index] >= affine
        CHECK_GE(var0_val, other_affine_value);  // Other expr is valid too.

        // The formula we use dominates the other one.
        CHECK_GE(affine_value, other_affine_value);
        if (affine_value != other_affine_value && !logged) {
          LOG_FIRST_N(INFO, 100)
              << "The two formulas differ: " << affine << " " << other_affine;
          logged = true;
        }
        computed_slack = std::min(computed_slack, var0_val - affine_value);
      }
    }
    if (computed_slack != kMaxIntegerValue && computed_slack != 0 && !trivial) {
      LOG(FATAL) << "Affine bound was not tight: computed_slack="
                 << computed_slack << " expr_bound=(" << expr << " >= " << lb
                 << ") affine_bound=(I" << expr.vars[0] << " >= " << affine
                 << ") other_var=[" << other_lb << ", " << other_ub << "]";
    }
  }
}

}  // namespace
}  // namespace operations_research::sat
