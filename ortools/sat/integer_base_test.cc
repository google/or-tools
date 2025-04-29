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

#include "gtest/gtest.h"

namespace operations_research::sat {
namespace {

TEST(CanonicalizeAffinePrecedenceTest, Basic) {
  LinearExpression2 expr;
  expr.vars[0] = IntegerVariable(0);
  expr.vars[1] = IntegerVariable(2);
  expr.coeffs[0] = IntegerValue(4);
  expr.coeffs[1] = IntegerValue(2);

  IntegerValue lb(0);
  IntegerValue ub(11);
  expr.CanonicalizeAndUpdateBounds(lb, ub);

  EXPECT_EQ(expr.vars[0], IntegerVariable(0));
  EXPECT_EQ(expr.vars[1], IntegerVariable(2));
  EXPECT_EQ(expr.coeffs[0], IntegerValue(2));
  EXPECT_EQ(expr.coeffs[1], IntegerValue(1));
  EXPECT_EQ(lb, 0);
  EXPECT_EQ(ub, 5);
}

TEST(BestBinaryRelationBoundsTest, Basic) {
  LinearExpression2 expr;
  expr.vars[0] = IntegerVariable(0);
  expr.vars[1] = IntegerVariable(2);
  expr.coeffs[0] = IntegerValue(1);
  expr.coeffs[1] = IntegerValue(-1);

  BestBinaryRelationBounds best_bounds;
  EXPECT_TRUE(best_bounds.Add(expr, IntegerValue(0), IntegerValue(5)));
  EXPECT_TRUE(best_bounds.Add(expr, IntegerValue(3), IntegerValue(8)));
  EXPECT_TRUE(best_bounds.Add(expr, IntegerValue(-1), IntegerValue(4)));
  EXPECT_FALSE(
      best_bounds.Add(expr, IntegerValue(3), IntegerValue(4)));  // best

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

}  // namespace
}  // namespace operations_research::sat
