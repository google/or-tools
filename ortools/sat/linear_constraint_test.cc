// Copyright 2010-2024 Google LLC
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

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;

TEST(ComputeActivityTest, BasicBehavior) {
  // The bounds are not useful for this test.
  LinearConstraintBuilder ct(IntegerValue(0), IntegerValue(0));

  ct.AddTerm(IntegerVariable(0), IntegerValue(1));
  ct.AddTerm(IntegerVariable(2), IntegerValue(-2));
  ct.AddTerm(IntegerVariable(4), IntegerValue(3));

  util_intops::StrongVector<IntegerVariable, double> values = {0.5, 0.0,  1.4,
                                                               0.0, -2.1, 0.0};
  EXPECT_NEAR(ComputeActivity(ct.Build(), values), 1 * 0.5 - 2 * 1.4 - 3 * 2.1,
              1e-6);
}

TEST(ComputeActivityTest, EmptyConstraint) {
  // The bounds are not useful for this test.
  LinearConstraintBuilder ct(IntegerValue(-10), IntegerValue(10));
  util_intops::StrongVector<IntegerVariable, double> values;
  EXPECT_EQ(ComputeActivity(ct.Build(), values), 0.0);
}

TEST(ComputeInfinityNormTest, BasicTest) {
  IntegerVariable x(0);
  IntegerVariable y(2);
  IntegerVariable z(4);
  {
    LinearConstraint constraint;
    EXPECT_EQ(IntegerValue(0), ComputeInfinityNorm(constraint));
  }
  {
    LinearConstraintBuilder constraint;
    constraint.AddTerm(x, IntegerValue(3));
    constraint.AddTerm(y, IntegerValue(-4));
    constraint.AddTerm(z, IntegerValue(1));
    EXPECT_EQ(IntegerValue(4), ComputeInfinityNorm(constraint.Build()));
  }
  {
    LinearConstraintBuilder constraint;
    constraint.AddTerm(y, IntegerValue(std::numeric_limits<int64_t>::max()));
    EXPECT_EQ(IntegerValue(std::numeric_limits<int64_t>::max()),
              ComputeInfinityNorm(constraint.Build()));
  }
}

TEST(ComputeL2NormTest, BasicTest) {
  IntegerVariable x(0);
  IntegerVariable y(2);
  IntegerVariable z(4);
  {
    LinearConstraint constraint;
    EXPECT_EQ(0.0, ComputeL2Norm(constraint));
  }
  {
    LinearConstraintBuilder constraint;
    constraint.AddTerm(x, IntegerValue(3));
    constraint.AddTerm(y, IntegerValue(-4));
    constraint.AddTerm(z, IntegerValue(12));
    EXPECT_EQ(13.0, ComputeL2Norm(constraint.Build()));
  }
  {
    LinearConstraintBuilder constraint;
    constraint.AddTerm(x, kMaxIntegerValue);
    constraint.AddTerm(y, kMaxIntegerValue);
    EXPECT_EQ(std::numeric_limits<double>::infinity(),
              ComputeL2Norm(constraint.Build()));
  }
  {
    LinearConstraintBuilder constraint;
    constraint.AddTerm(x, IntegerValue(1LL << 60));
    constraint.AddTerm(y, IntegerValue(1LL << 60));
    EXPECT_NEAR(1.6304772e+18, ComputeL2Norm(constraint.Build()), 1e+16);
  }
}

TEST(ScalarProductTest, BasicTest) {
  IntegerVariable x(0);
  IntegerVariable y(2);
  IntegerVariable z(4);

  LinearConstraintBuilder ct_one(IntegerValue(0), IntegerValue(11));
  ct_one.AddTerm(x, IntegerValue(3));
  ct_one.AddTerm(y, IntegerValue(-4));

  LinearConstraintBuilder ct_two(IntegerValue(1), IntegerValue(2));
  ct_two.AddTerm(z, IntegerValue(-1));

  LinearConstraintBuilder ct_three(IntegerValue(0), IntegerValue(2));
  ct_three.AddTerm(x, IntegerValue(1));
  ct_three.AddTerm(y, IntegerValue(1));
  ct_three.AddTerm(z, IntegerValue(1));

  EXPECT_EQ(0.0, ScalarProduct(ct_one.Build(), ct_two.Build()));
  EXPECT_EQ(-1.0, ScalarProduct(ct_one.Build(), ct_three.Build()));
  EXPECT_EQ(-1.0, ScalarProduct(ct_two.Build(), ct_three.Build()));
}

namespace {

// Creates an upper bounded LinearConstraintBuilder from a dense representation.
LinearConstraint CreateUbConstraintForTest(
    absl::Span<const int64_t> dense_coeffs, int64_t upper_bound) {
  LinearConstraint result;
  result.resize(dense_coeffs.size());
  int new_size = 0;
  for (int i = 0; i < dense_coeffs.size(); ++i) {
    if (dense_coeffs[i] != 0) {
      result.vars[new_size] = IntegerVariable(i);
      result.coeffs[new_size] = dense_coeffs[i];
      ++new_size;
    }
  }
  result.resize(new_size);
  result.lb = kMinIntegerValue;
  result.ub = upper_bound;
  return result;
}

}  // namespace

TEST(DivideByGCDTest, BasicBehaviorWithoughLowerBound) {
  LinearConstraint ct = CreateUbConstraintForTest({2, 4, -8}, 11);
  DivideByGCD(&ct);
  const LinearConstraint expected = CreateUbConstraintForTest({1, 2, -4}, 5);
  EXPECT_EQ(ct, expected);
}

TEST(DivideByGCDTest, BasicBehaviorWithLowerBound) {
  LinearConstraint ct = CreateUbConstraintForTest({2, 4, -8}, 11);
  ct.lb = IntegerValue(-3);
  DivideByGCD(&ct);
  LinearConstraint expected = CreateUbConstraintForTest({1, 2, -4}, 5);
  expected.lb = IntegerValue(-1);
  EXPECT_EQ(ct, expected);
}

TEST(RemoveZeroTermsTest, BasicBehavior) {
  LinearConstraint ct = CreateUbConstraintForTest({2, 4, -8}, 11);
  ct.coeffs[1] = IntegerValue(0);
  RemoveZeroTerms(&ct);
  EXPECT_EQ(ct, CreateUbConstraintForTest({2, 0, -8}, 11));
}

TEST(MakeAllCoefficientsPositiveTest, BasicBehavior) {
  // Note that this relies on the fact that the negation of an IntegerVariable
  // var is is the one with IntegerVariable(var.value() ^ 1);
  LinearConstraint ct = CreateUbConstraintForTest({-2, 0, -7, 0}, 10);
  MakeAllCoefficientsPositive(&ct);
  EXPECT_EQ(ct, CreateUbConstraintForTest({0, 2, 0, 7}, 10));
}

TEST(LinearConstraintBuilderTest, DuplicateCoefficient) {
  Model model;
  model.GetOrCreate<IntegerEncoder>();
  LinearConstraintBuilder builder(&model, kMinIntegerValue, IntegerValue(10));

  // Note that internally, positive variable have an even index, so we only
  // use those so that we don't remap a negated variable.
  builder.AddTerm(IntegerVariable(0), IntegerValue(100));
  builder.AddTerm(IntegerVariable(2), IntegerValue(10));
  builder.AddTerm(IntegerVariable(4), IntegerValue(7));
  builder.AddTerm(IntegerVariable(0), IntegerValue(-10));
  builder.AddTerm(IntegerVariable(2), IntegerValue(1));
  builder.AddTerm(IntegerVariable(4), IntegerValue(-7));
  builder.AddTerm(IntegerVariable(2), IntegerValue(3));

  EXPECT_EQ(builder.Build(), CreateUbConstraintForTest({90, 0, 14}, 10));
}

TEST(LinearConstraintBuilderTest, AffineExpression) {
  Model model;
  model.GetOrCreate<IntegerEncoder>();
  LinearConstraintBuilder builder(&model, kMinIntegerValue, IntegerValue(10));

  // Note that internally, positive variable have an even index, so we only
  // use those so that we don't remap a negated variable.
  const IntegerVariable var(0);
  builder.AddTerm(AffineExpression(var, IntegerValue(3), IntegerValue(2)),
                  IntegerValue(100));
  builder.AddTerm(AffineExpression(var, IntegerValue(-2), IntegerValue(1)),
                  IntegerValue(70));

  // Coeff is 3*100 - 2 * 70, ub is 10 - 2*100 - 1*70
  EXPECT_EQ(builder.Build(), CreateUbConstraintForTest({160}, -260))
      << builder.Build().DebugString();
}

TEST(LinearConstraintBuilderTest, AddLiterals) {
  Model model;
  model.GetOrCreate<IntegerEncoder>();
  const BooleanVariable b = model.Add(NewBooleanVariable());
  const BooleanVariable c = model.Add(NewBooleanVariable());
  const BooleanVariable d = model.Add(NewBooleanVariable());

  // Create integer views.
  model.Add(NewIntegerVariableFromLiteral(Literal(b, true)));   // X0
  model.Add(NewIntegerVariableFromLiteral(Literal(b, false)));  // X1
  model.Add(NewIntegerVariableFromLiteral(Literal(c, false)));  // X2
  model.Add(NewIntegerVariableFromLiteral(Literal(d, false)));  // X3
  model.Add(NewIntegerVariableFromLiteral(Literal(d, true)));   // X4

  // When we have both view, we use the lowest IntegerVariable.
  {
    LinearConstraintBuilder builder(&model, kMinIntegerValue, IntegerValue(1));
    EXPECT_TRUE(builder.AddLiteralTerm(Literal(b, true), IntegerValue(1)));
    EXPECT_EQ(builder.Build().DebugString(), "1*X0 <= 1");
  }
  {
    LinearConstraintBuilder builder(&model, kMinIntegerValue, IntegerValue(1));
    EXPECT_TRUE(builder.AddLiteralTerm(Literal(b, false), IntegerValue(1)));
    EXPECT_EQ(builder.Build().DebugString(), "-1*X0 <= 0");
  }
  {
    LinearConstraintBuilder builder(&model, kMinIntegerValue, IntegerValue(1));
    EXPECT_TRUE(builder.AddLiteralTerm(Literal(d, true), IntegerValue(1)));
    EXPECT_EQ(builder.Build().DebugString(), "-1*X3 <= 0");
  }
  {
    LinearConstraintBuilder builder(&model, kMinIntegerValue, IntegerValue(1));
    EXPECT_TRUE(builder.AddLiteralTerm(Literal(d, false), IntegerValue(1)));
    EXPECT_EQ(builder.Build().DebugString(), "1*X3 <= 1");
  }

  // When we have just one view, we use the one we have.
  {
    LinearConstraintBuilder builder(&model, kMinIntegerValue, IntegerValue(1));
    EXPECT_TRUE(builder.AddLiteralTerm(Literal(c, true), IntegerValue(1)));
    EXPECT_EQ(builder.Build().DebugString(), "-1*X2 <= 0");
  }
  {
    LinearConstraintBuilder builder(&model, kMinIntegerValue, IntegerValue(1));
    EXPECT_TRUE(builder.AddLiteralTerm(Literal(c, false), IntegerValue(1)));
    EXPECT_EQ(builder.Build().DebugString(), "1*X2 <= 1");
  }
}

TEST(LinearConstraintBuilderTest, AddConstant) {
  Model model;
  model.GetOrCreate<IntegerEncoder>();
  LinearConstraintBuilder builder1(&model, kMinIntegerValue, IntegerValue(10));
  builder1.AddTerm(IntegerVariable(0), IntegerValue(5));
  builder1.AddTerm(IntegerVariable(2), IntegerValue(10));
  builder1.AddConstant(IntegerValue(3));
  EXPECT_EQ(builder1.Build().DebugString(), "5*X0 10*X1 <= 7");

  LinearConstraintBuilder builder2(&model, IntegerValue(4), kMaxIntegerValue);
  builder2.AddTerm(IntegerVariable(0), IntegerValue(5));
  builder2.AddTerm(IntegerVariable(2), IntegerValue(10));
  builder2.AddConstant(IntegerValue(-3));
  EXPECT_EQ(builder2.Build().DebugString(), "7 <= 5*X0 10*X1");

  LinearConstraintBuilder builder3(&model, kMinIntegerValue, IntegerValue(10));
  builder3.AddTerm(IntegerVariable(0), IntegerValue(5));
  builder3.AddTerm(IntegerVariable(2), IntegerValue(10));
  builder3.AddConstant(IntegerValue(-3));
  EXPECT_EQ(builder3.Build().DebugString(), "5*X0 10*X1 <= 13");

  LinearConstraintBuilder builder4(&model, IntegerValue(4), kMaxIntegerValue);
  builder4.AddTerm(IntegerVariable(0), IntegerValue(5));
  builder4.AddTerm(IntegerVariable(2), IntegerValue(10));
  builder4.AddConstant(IntegerValue(3));
  EXPECT_EQ(builder4.Build().DebugString(), "1 <= 5*X0 10*X1");

  LinearConstraintBuilder builder5(&model, IntegerValue(4), IntegerValue(10));
  builder5.AddTerm(IntegerVariable(0), IntegerValue(5));
  builder5.AddTerm(IntegerVariable(2), IntegerValue(10));
  builder5.AddConstant(IntegerValue(3));
  EXPECT_EQ(builder5.Build().DebugString(), "1 <= 5*X0 10*X1 <= 7");
}

TEST(CleanTermsAndFillConstraintTest, VarAndItsNegation) {
  std::vector<std::pair<IntegerVariable, IntegerValue>> terms;
  terms.push_back({IntegerVariable(4), IntegerValue(7)});
  terms.push_back({IntegerVariable(5), IntegerValue(4)});
  LinearConstraint constraint;
  CleanTermsAndFillConstraint(&terms, &constraint);
  EXPECT_EQ(constraint.DebugString(), "0 <= 3*X2 <= 0");
}

TEST(LinearConstraintBuilderTest, AddQuadraticLowerBound) {
  Model model;
  model.GetOrCreate<IntegerEncoder>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  IntegerVariable x0 = model.Add(NewIntegerVariable(2, 5));
  IntegerVariable x1 = model.Add(NewIntegerVariable(3, 6));
  LinearConstraintBuilder builder1(&model, kMinIntegerValue, IntegerValue(10));
  AffineExpression a0(x0, IntegerValue(3), IntegerValue(2));  // 3 * x0 + 2.
  builder1.AddQuadraticLowerBound(a0, x1, integer_trail);
  EXPECT_EQ(builder1.Build().DebugString(), "9*X0 8*X1 <= 28");
}

TEST(LinearConstraintBuilderTest, AddQuadraticLowerBoundAffineIsVar) {
  Model model;
  model.GetOrCreate<IntegerEncoder>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  IntegerVariable x0 = model.Add(NewIntegerVariable(2, 5));
  IntegerVariable x1 = model.Add(NewIntegerVariable(3, 6));
  LinearConstraintBuilder builder1(&model, kMinIntegerValue, IntegerValue(10));
  builder1.AddQuadraticLowerBound(x0, x1, integer_trail);
  EXPECT_EQ(builder1.Build().DebugString(), "3*X0 2*X1 <= 16");
}

TEST(LinearConstraintBuilderTest, AddQuadraticLowerBoundAffineIsConstant) {
  Model model;
  model.GetOrCreate<IntegerEncoder>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  IntegerVariable x0 = model.Add(NewIntegerVariable(2, 5));
  LinearConstraintBuilder builder1(&model, kMinIntegerValue, IntegerValue(10));
  builder1.AddQuadraticLowerBound(IntegerValue(4), x0, integer_trail);
  EXPECT_EQ(builder1.Build().DebugString(), "4*X0 <= 10");
}

TEST(LinExprTest, Bounds) {
  Model model;
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(1, 2)),
                                    model.Add(NewIntegerVariable(0, 3)),
                                    model.Add(NewIntegerVariable(-2, 4))};
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  LinearExpression expr1;  // 2x0 + 3x1 - 5
  expr1.vars = {vars[0], vars[1]};
  expr1.coeffs = {IntegerValue(2), IntegerValue(3)};
  expr1.offset = IntegerValue(-5);
  expr1 = CanonicalizeExpr(expr1);
  EXPECT_EQ(IntegerValue(-3), expr1.Min(*integer_trail));
  EXPECT_EQ(IntegerValue(8), expr1.Max(*integer_trail));

  LinearExpression expr2;  // 2x1 - 5x2 + 6
  expr2.vars = {vars[1], vars[2]};
  expr2.coeffs = {IntegerValue(2), IntegerValue(-5)};
  expr2.offset = IntegerValue(6);
  expr2 = CanonicalizeExpr(expr2);
  EXPECT_EQ(IntegerValue(-14), expr2.Min(*integer_trail));
  EXPECT_EQ(IntegerValue(22), expr2.Max(*integer_trail));

  LinearExpression expr3;  // 2x0 + 3x2
  expr3.vars = {vars[0], vars[2]};
  expr3.coeffs = {IntegerValue(2), IntegerValue(3)};
  expr3 = CanonicalizeExpr(expr3);
  EXPECT_EQ(IntegerValue(-4), expr3.Min(*integer_trail));
  EXPECT_EQ(IntegerValue(16), expr3.Max(*integer_trail));
}

TEST(LinExprTest, Canonicalization) {
  Model model;
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(1, 2)),
                                    model.Add(NewIntegerVariable(0, 3))};
  LinearExpression expr;  // 2x0 - 3x1 - 5
  expr.vars = vars;
  expr.coeffs = {IntegerValue(2), IntegerValue(-3)};
  expr.offset = IntegerValue(-5);

  LinearExpression canonical_expr = CanonicalizeExpr(expr);
  EXPECT_THAT(canonical_expr.vars, ElementsAre(vars[0], NegationOf(vars[1])));
  EXPECT_THAT(canonical_expr.coeffs,
              ElementsAre(IntegerValue(2), IntegerValue(3)));
  EXPECT_EQ(canonical_expr.offset, IntegerValue(-5));
}

TEST(NoDuplicateVariable, BasicBehavior) {
  LinearConstraint ct;
  ct.lb = kMinIntegerValue;
  ct.ub = IntegerValue(10);

  ct.resize(3);
  ct.num_terms = 1;
  ct.vars[0] = IntegerVariable(4);
  ct.coeffs[0] = IntegerValue(1);
  EXPECT_TRUE(NoDuplicateVariable(ct));

  ct.num_terms = 2;
  ct.vars[1] = IntegerVariable(2);
  ct.coeffs[1] = IntegerValue(5);
  EXPECT_TRUE(NoDuplicateVariable(ct));

  ct.num_terms = 3;
  ct.vars[2] = IntegerVariable(4);
  ct.coeffs[2] = IntegerValue(1);
  EXPECT_FALSE(NoDuplicateVariable(ct));
}

TEST(NoDuplicateVariable, BasicBehaviorNegativeVar) {
  LinearConstraint ct;

  ct.lb = kMinIntegerValue;
  ct.ub = IntegerValue(10);

  ct.resize(3);
  ct.num_terms = 1;
  ct.vars[0] = IntegerVariable(4);
  ct.coeffs[0] = IntegerValue(1);
  EXPECT_TRUE(NoDuplicateVariable(ct));

  ct.num_terms = 2;
  ct.vars[1] = IntegerVariable(2);
  ct.coeffs[1] = IntegerValue(5);
  EXPECT_TRUE(NoDuplicateVariable(ct));

  ct.num_terms = 3;
  ct.vars[2] = IntegerVariable(5);
  ct.coeffs[2] = IntegerValue(1);
  EXPECT_FALSE(NoDuplicateVariable(ct));
}

TEST(PositiveVarExpr, BasicBehaviorNegativeVar) {
  LinearExpression ct;
  ct.offset = IntegerValue(10);
  ct.vars.push_back(IntegerVariable(4));
  ct.coeffs.push_back(IntegerValue(1));

  ct.vars.push_back(IntegerVariable(1));
  ct.coeffs.push_back(IntegerValue(5));

  LinearExpression positive_var_expr = PositiveVarExpr(ct);
  EXPECT_THAT(positive_var_expr.vars,
              ElementsAre(ct.vars[0], NegationOf(ct.vars[1])));
  EXPECT_THAT(positive_var_expr.coeffs,
              ElementsAre(ct.coeffs[0], -ct.coeffs[1]));
  EXPECT_EQ(positive_var_expr.offset, ct.offset);
}

TEST(GetCoefficient, BasicBehavior) {
  LinearExpression ct;
  ct.offset = IntegerValue(10);
  ct.vars.push_back(IntegerVariable(4));
  ct.coeffs.push_back(IntegerValue(2));

  EXPECT_EQ(IntegerValue(2), GetCoefficient(IntegerVariable(4), ct));
  EXPECT_EQ(IntegerValue(-2), GetCoefficient(IntegerVariable(5), ct));
  EXPECT_EQ(IntegerValue(0), GetCoefficient(IntegerVariable(2), ct));
}

TEST(GetCoefficientOfPositiveVar, BasicBehavior) {
  LinearExpression ct;
  ct.offset = IntegerValue(10);
  ct.vars.push_back(IntegerVariable(4));
  ct.coeffs.push_back(IntegerValue(2));

  EXPECT_EQ(IntegerValue(2),
            GetCoefficientOfPositiveVar(IntegerVariable(4), ct));
  EXPECT_EQ(IntegerValue(0),
            GetCoefficientOfPositiveVar(IntegerVariable(2), ct));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
