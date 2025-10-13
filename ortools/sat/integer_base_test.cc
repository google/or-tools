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

#include <utility>

#include "absl/log/check.h"
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

TEST(BestBinaryRelationBoundsTest, UpperBound) {
  LinearExpression2 expr;
  expr.vars[0] = IntegerVariable(0);
  expr.vars[1] = IntegerVariable(2);
  expr.coeffs[0] = IntegerValue(1);
  expr.coeffs[1] = IntegerValue(-1);

  using AddResult = BestBinaryRelationBounds::AddResult;
  BestBinaryRelationBounds best_bounds;
  EXPECT_EQ(best_bounds.Add(expr, IntegerValue(0), IntegerValue(5)),
            std::make_pair(AddResult::ADDED, AddResult::ADDED));

  EXPECT_EQ(best_bounds.GetUpperBound(expr), IntegerValue(5));

  expr.coeffs[0] *= 3;
  expr.coeffs[1] *= 3;
  EXPECT_EQ(best_bounds.GetUpperBound(expr), IntegerValue(15));

  expr.Negate();
  EXPECT_EQ(best_bounds.GetUpperBound(expr), IntegerValue(0));
}

}  // namespace
}  // namespace operations_research::sat
