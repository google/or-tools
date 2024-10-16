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

#include "ortools/sat/implied_bounds.h"

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(ImpliedBoundsTest, BasicTest) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);
  auto* ib = model.GetOrCreate<ImpliedBounds>();
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();

  const Literal enforcement(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var(model.Add(NewIntegerVariable(0, 10)));

  EXPECT_TRUE(ib->Add(enforcement,
                      IntegerLiteral::GreaterOrEqual(var, IntegerValue(3))));
  EXPECT_TRUE(ib->Add(enforcement.Negated(),
                      IntegerLiteral::GreaterOrEqual(var, IntegerValue(7))));

  // Here because we are at level-zero everything is propagated right away.
  EXPECT_EQ(integer_trail->LowerBound(var), IntegerValue(3));
  EXPECT_EQ(integer_trail->LevelZeroLowerBound(var), IntegerValue(3));
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_EQ(integer_trail->LowerBound(var), IntegerValue(3));
}

TEST(ImpliedBoundsTest, BasicTestPositiveLevel) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);
  auto* ib = model.GetOrCreate<ImpliedBounds>();
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();

  const Literal enforcement(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var(model.Add(NewIntegerVariable(0, 10)));

  // We can do the same at a positive level.
  const Literal to_enqueue(model.Add(NewBooleanVariable()), true);
  EXPECT_TRUE(sat_solver->ResetToLevelZero());
  EXPECT_TRUE(sat_solver->EnqueueDecisionIfNotConflicting(to_enqueue));
  EXPECT_GT(sat_solver->CurrentDecisionLevel(), 0);

  EXPECT_TRUE(ib->Add(enforcement,
                      IntegerLiteral::GreaterOrEqual(var, IntegerValue(3))));
  EXPECT_TRUE(ib->Add(enforcement.Negated(),
                      IntegerLiteral::GreaterOrEqual(var, IntegerValue(7))));

  // Now, only the level zero bound is up to date.
  EXPECT_EQ(integer_trail->LowerBound(var), IntegerValue(0));
  EXPECT_EQ(integer_trail->LevelZeroLowerBound(var), IntegerValue(3));

  // But on the next restart, nothing is lost.
  EXPECT_TRUE(sat_solver->ResetToLevelZero());
  EXPECT_EQ(integer_trail->LowerBound(var), IntegerValue(3));
}

// Same test as above but no deduction since parameter is false.
TEST(ImpliedBoundsTest, BasicTestWithFalseParameters) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(false);
  auto* ib = model.GetOrCreate<ImpliedBounds>();
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();

  const Literal enforcement(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var(model.Add(NewIntegerVariable(0, 10)));

  EXPECT_TRUE(ib->Add(enforcement,
                      IntegerLiteral::GreaterOrEqual(var, IntegerValue(3))));
  EXPECT_TRUE(ib->Add(enforcement.Negated(),
                      IntegerLiteral::GreaterOrEqual(var, IntegerValue(7))));

  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_EQ(integer_trail->LowerBound(var), IntegerValue(0));
}

TEST(ImpliedBoundsTest, ReadBoundsFromTrail) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  const Literal l(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var(model.Add(NewIntegerVariable(0, 100)));

  // Make sure l as a view.
  const IntegerVariable view(model.Add(NewIntegerVariable(0, 1)));
  model.GetOrCreate<IntegerEncoder>()->AssociateToIntegerEqualValue(
      l, view, IntegerValue(1));

  // So that there is a decision.
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  EXPECT_TRUE(sat_solver->EnqueueDecisionIfNotConflicting(l));
  EXPECT_TRUE(sat_solver->Propagate());

  // Enqueue a bunch of fact.
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(2)), {l.Negated()}, {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(4)), {l.Negated()}, {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(8)), {l.Negated()}, {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(9)), {l.Negated()}, {}));

  // Read from trail.
  auto* ib = model.GetOrCreate<ImpliedBounds>();
  ib->ProcessIntegerTrail(l);

  std::vector<ImpliedBoundEntry> result = ib->GetImpliedBounds(var);
  EXPECT_EQ(result.size(), 1);
  EXPECT_EQ(result[0].literal_view, view);
  EXPECT_EQ(result[0].lower_bound, IntegerValue(9));
  EXPECT_TRUE(result[0].is_positive);
}

TEST(ImpliedBoundsTest, DetectEqualityFromMin) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  const Literal literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var(model.Add(NewIntegerVariable(0, 100)));

  auto* ib = model.GetOrCreate<ImpliedBounds>();
  ib->Add(literal, IntegerLiteral::LowerOrEqual(var, IntegerValue(0)));

  EXPECT_THAT(
      ib->GetImpliedValues(literal),
      testing::UnorderedElementsAre(testing::Pair(var, IntegerValue(0))));
}

TEST(ImpliedBoundsTest, DetectEqualityFromMax) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  const Literal literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var(model.Add(NewIntegerVariable(0, 100)));

  auto* ib = model.GetOrCreate<ImpliedBounds>();
  ib->Add(literal, IntegerLiteral::GreaterOrEqual(var, IntegerValue(100)));

  EXPECT_THAT(ib->GetImpliedValues(literal),
              UnorderedElementsAre(Pair(var, IntegerValue(100))));
}

TEST(ImpliedBoundsTest, DetectEqualityFromBothInequalities) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  const Literal literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var(model.Add(NewIntegerVariable(0, 100)));

  auto* ib = model.GetOrCreate<ImpliedBounds>();
  ib->Add(literal, IntegerLiteral::LowerOrEqual(var, IntegerValue(7)));
  ib->Add(literal, IntegerLiteral::GreaterOrEqual(var, IntegerValue(7)));

  EXPECT_THAT(ib->GetImpliedValues(literal),
              UnorderedElementsAre(Pair(var, IntegerValue(7))));
}

TEST(ImpliedBoundsTest, NoEqualityDetection) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  const Literal literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var(model.Add(NewIntegerVariable(0, 100)));

  auto* ib = model.GetOrCreate<ImpliedBounds>();
  ib->Add(literal, IntegerLiteral::LowerOrEqual(var, IntegerValue(7)));
  ib->Add(literal, IntegerLiteral::GreaterOrEqual(var, IntegerValue(6)));

  EXPECT_TRUE(ib->GetImpliedValues(literal).empty());
}

TEST(DetectLinearEncodingOfProductsTest, MatchingElementEncodings) {
  Model model;
  const Literal l0(model.Add(NewBooleanVariable()), true);
  const Literal l1(model.Add(NewBooleanVariable()), true);
  const Literal l2(model.Add(NewBooleanVariable()), true);
  const Literal l3(model.Add(NewBooleanVariable()), true);

  model.Add(NewIntegerVariableFromLiteral(l0));
  model.Add(NewIntegerVariableFromLiteral(l1));
  model.Add(NewIntegerVariableFromLiteral(l2));
  model.Add(NewIntegerVariableFromLiteral(l3));

  const IntegerVariable x0(model.Add(NewIntegerVariable(0, 100)));
  const IntegerVariable x1(model.Add(NewIntegerVariable(0, 100)));
  auto* element_encodings = model.GetOrCreate<ElementEncodings>();
  element_encodings->Add(x0,
                         {{IntegerValue(2), l0},
                          {IntegerValue(4), l1},
                          {IntegerValue(2), l2},
                          {IntegerValue(10), l3}},
                         2);
  element_encodings->Add(x1,
                         {{IntegerValue(3), l0},
                          {IntegerValue(10), l1},
                          {IntegerValue(20), l2},
                          {IntegerValue(30), l3}},
                         2);
  LinearConstraintBuilder builder(&model);
  builder.AddConstant(IntegerValue(-1));  // To be cleared.
  EXPECT_TRUE(
      model.GetOrCreate<ProductDecomposer>()->TryToLinearize(x0, x1, &builder));
  EXPECT_EQ(builder.BuildExpression().DebugString(), "34*X1 34*X2 294*X3 + 6");

  builder.Clear();
  EXPECT_TRUE(
      model.GetOrCreate<ProductDecomposer>()->TryToLinearize(x1, x0, &builder));
  EXPECT_EQ(builder.BuildExpression().DebugString(), "34*X1 34*X2 294*X3 + 6");
}

TEST(DetectLinearEncodingOfProductsTest, MatchingEncodingAndSizeTwoEncoding) {
  Model model;
  const Literal l0(model.Add(NewBooleanVariable()), true);
  const Literal l1(model.Add(NewBooleanVariable()), true);
  const Literal l2(model.Add(NewBooleanVariable()), true);
  const Literal l3(model.Add(NewBooleanVariable()), true);
  const IntegerVariable x0(model.Add(NewIntegerVariable(0, 100)));
  const IntegerVariable x1(model.Add(NewIntegerVariable(6, 7)));
  auto* element_encodings = model.GetOrCreate<ElementEncodings>();
  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  element_encodings->Add(x0,
                         {{IntegerValue(2), l0},
                          {IntegerValue(4), l1},
                          {IntegerValue(2), l2},
                          {IntegerValue(10), l3}},
                         2);
  integer_encoder->AssociateToIntegerEqualValue(l2, x1, IntegerValue(7));
  model.Add(NewIntegerVariableFromLiteral(l0));
  model.Add(NewIntegerVariableFromLiteral(l1));
  model.Add(NewIntegerVariableFromLiteral(l2));
  model.Add(NewIntegerVariableFromLiteral(l3));

  LinearConstraintBuilder builder(&model);
  builder.AddConstant(IntegerValue(-1));  // To be cleared.
  EXPECT_TRUE(
      model.GetOrCreate<ProductDecomposer>()->TryToLinearize(x0, x1, &builder));
  EXPECT_EQ(builder.BuildExpression().DebugString(), "12*X3 2*X4 48*X5 + 12");

  EXPECT_TRUE(
      model.GetOrCreate<ProductDecomposer>()->TryToLinearize(x1, x0, &builder));
  EXPECT_EQ(builder.BuildExpression().DebugString(), "12*X3 2*X4 48*X5 + 12");
}

TEST(DetectLinearEncodingOfProductsTest, BooleanAffinePosPosProduct) {
  Model model;
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 1));
  const AffineExpression left(var, IntegerValue(2), IntegerValue(-1));
  const AffineExpression right(var, IntegerValue(3), IntegerValue(1));

  LinearConstraintBuilder builder(&model);
  util_intops::StrongVector<IntegerVariable, double> lp_values(2, 0.0);

  EXPECT_TRUE(model.GetOrCreate<ProductDecomposer>()->TryToLinearize(
      left, right, &builder));
  for (int value : {0, 1}) {
    lp_values[var] = static_cast<double>(value);
    lp_values[NegationOf(var)] = static_cast<double>(-value);
    EXPECT_EQ(builder.BuildExpression().LpValue(lp_values),
              left.LpValue(lp_values) * right.LpValue(lp_values));
  }

  builder.Clear();
  EXPECT_TRUE(model.GetOrCreate<ProductDecomposer>()->TryToLinearize(
      right, left, &builder));
  for (int value : {0, 1}) {
    lp_values[var] = static_cast<double>(value);
    lp_values[NegationOf(var)] = static_cast<double>(-value);
    EXPECT_EQ(builder.BuildExpression().LpValue(lp_values),
              left.LpValue(lp_values) * right.LpValue(lp_values));
  }
}

TEST(DetectLinearEncodingOfProductsTest, BooleanAffinePosNegProduct) {
  Model model;
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 1));
  const AffineExpression left(var, IntegerValue(2), IntegerValue(-1));
  const AffineExpression right(NegationOf(var), IntegerValue(3),
                               IntegerValue(1));

  LinearConstraintBuilder builder(&model);
  util_intops::StrongVector<IntegerVariable, double> lp_values(2, 0.0);

  EXPECT_TRUE(model.GetOrCreate<ProductDecomposer>()->TryToLinearize(
      left, right, &builder));
  for (int value : {0, 1}) {
    lp_values[var] = static_cast<double>(value);
    lp_values[NegationOf(var)] = static_cast<double>(-value);
    EXPECT_EQ(builder.BuildExpression().LpValue(lp_values),
              left.LpValue(lp_values) * right.LpValue(lp_values));
  }
  builder.Clear();
  EXPECT_TRUE(model.GetOrCreate<ProductDecomposer>()->TryToLinearize(
      right, left, &builder));
  for (int value : {0, 1}) {
    lp_values[var] = static_cast<double>(value);
    lp_values[NegationOf(var)] = static_cast<double>(-value);
    EXPECT_EQ(builder.BuildExpression().LpValue(lp_values),
              left.LpValue(lp_values) * right.LpValue(lp_values));
  }
}

TEST(DetectLinearEncodingOfProductsTest, BooleanAffineNegNegProduct) {
  Model model;
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 1));
  const AffineExpression left(NegationOf(var), IntegerValue(2),
                              IntegerValue(-1));
  const AffineExpression right(NegationOf(var), IntegerValue(3),
                               IntegerValue(1));

  LinearConstraintBuilder builder(&model);
  util_intops::StrongVector<IntegerVariable, double> lp_values(2, 0.0);

  EXPECT_TRUE(model.GetOrCreate<ProductDecomposer>()->TryToLinearize(
      left, right, &builder));
  for (int value : {0, 1}) {
    lp_values[var] = static_cast<double>(value);
    lp_values[NegationOf(var)] = static_cast<double>(-value);
    EXPECT_EQ(builder.BuildExpression().LpValue(lp_values),
              left.LpValue(lp_values) * right.LpValue(lp_values));
  }

  builder.Clear();
  EXPECT_TRUE(model.GetOrCreate<ProductDecomposer>()->TryToLinearize(
      right, left, &builder));
  for (int value : {0, 1}) {
    lp_values[var] = static_cast<double>(value);
    lp_values[NegationOf(var)] = static_cast<double>(-value);
    EXPECT_EQ(builder.BuildExpression().LpValue(lp_values),
              left.LpValue(lp_values) * right.LpValue(lp_values));
  }
}

TEST(DetectLinearEncodingOfProductsTest, NoDetectionWhenNotBooleanA) {
  Model model;
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 2));
  const AffineExpression left(var, IntegerValue(2), IntegerValue(-1));
  const AffineExpression right(var, IntegerValue(3), IntegerValue(1));

  LinearConstraintBuilder builder(&model);
  EXPECT_FALSE(model.GetOrCreate<ProductDecomposer>()->TryToLinearize(
      left, right, &builder));
}

TEST(DetectLinearEncodingOfProductsTest, NoDetectionWhenNotBooleanB) {
  Model model;
  const IntegerVariable var = model.Add(NewIntegerVariable(-1, 1));
  const AffineExpression left(var, IntegerValue(2), IntegerValue(-1));
  const AffineExpression right(var, IntegerValue(3), IntegerValue(1));

  LinearConstraintBuilder builder(&model);
  EXPECT_FALSE(model.GetOrCreate<ProductDecomposer>()->TryToLinearize(
      left, right, &builder));
}

TEST(DetectLinearEncodingOfProductsTest, AffineTimesConstant) {
  Model model;
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 5));
  const AffineExpression left(var, IntegerValue(2), IntegerValue(-1));
  const AffineExpression right = IntegerValue(3);

  LinearConstraintBuilder builder(&model);
  EXPECT_TRUE(model.GetOrCreate<ProductDecomposer>()->TryToLinearize(
      left, right, &builder));
  EXPECT_EQ(builder.BuildExpression().DebugString(), "6*X0 + -3");

  EXPECT_TRUE(model.GetOrCreate<ProductDecomposer>()->TryToLinearize(
      right, left, &builder));
  EXPECT_EQ(builder.BuildExpression().DebugString(), "6*X0 + -3");
}

TEST(DecomposeProductTest, MatchingElementEncodings) {
  Model model;

  const Literal l0(model.Add(NewBooleanVariable()), true);
  const Literal l1(model.Add(NewBooleanVariable()), true);
  const Literal l2(model.Add(NewBooleanVariable()), true);
  const Literal l3(model.Add(NewBooleanVariable()), true);

  model.Add(NewIntegerVariableFromLiteral(l0));
  model.Add(NewIntegerVariableFromLiteral(l1));
  model.Add(NewIntegerVariableFromLiteral(l2));
  model.Add(NewIntegerVariableFromLiteral(l3));

  const IntegerVariable x0(model.Add(NewIntegerVariable(0, 100)));
  const IntegerVariable x1(model.Add(NewIntegerVariable(0, 100)));

  auto* element_encodings = model.GetOrCreate<ElementEncodings>();
  element_encodings->Add(x0,
                         {{IntegerValue(2), l0},
                          {IntegerValue(4), l1},
                          {IntegerValue(2), l2},
                          {IntegerValue(10), l3}},
                         2);
  element_encodings->Add(x1,
                         {{IntegerValue(3), l0},
                          {IntegerValue(10), l1},
                          {IntegerValue(20), l2},
                          {IntegerValue(30), l3}},
                         2);

  auto* decomposer = model.GetOrCreate<ProductDecomposer>();
  const std::vector<LiteralValueValue> terms_a =
      decomposer->TryToDecompose(x0, x1);
  const std::vector<LiteralValueValue> expected_terms_a = {
      {l0, IntegerValue(2), IntegerValue(3)},
      {l1, IntegerValue(4), IntegerValue(10)},
      {l2, IntegerValue(2), IntegerValue(20)},
      {l3, IntegerValue(10), IntegerValue(30)},
  };
  ASSERT_FALSE(terms_a.empty());
  EXPECT_EQ(terms_a, expected_terms_a);

  const std::vector<LiteralValueValue> terms_b =
      decomposer->TryToDecompose(x1, x0);
  const std::vector<LiteralValueValue> expected_terms_b = {
      {l0, IntegerValue(3), IntegerValue(2)},
      {l1, IntegerValue(10), IntegerValue(4)},
      {l2, IntegerValue(20), IntegerValue(2)},
      {l3, IntegerValue(30), IntegerValue(10)},
  };
  ASSERT_FALSE(terms_b.empty());
  EXPECT_EQ(terms_b, expected_terms_b);
}

TEST(DecomposeProductTest, MatchingEncodingAndSizeTwoEncoding) {
  Model model;

  const Literal l0(model.Add(NewBooleanVariable()), true);
  const Literal l1(model.Add(NewBooleanVariable()), true);
  const Literal l2(model.Add(NewBooleanVariable()), true);
  const Literal l3(model.Add(NewBooleanVariable()), true);
  const IntegerVariable x0(model.Add(NewIntegerVariable(0, 100)));
  const IntegerVariable x1(model.Add(NewIntegerVariable(6, 7)));

  auto* element_encodings = model.GetOrCreate<ElementEncodings>();
  element_encodings->Add(x0,
                         {{IntegerValue(2), l0},
                          {IntegerValue(4), l1},
                          {IntegerValue(2), l2},
                          {IntegerValue(10), l3}},
                         2);

  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  integer_encoder->AssociateToIntegerEqualValue(l2, x1, IntegerValue(7));
  model.Add(NewIntegerVariableFromLiteral(l0));
  model.Add(NewIntegerVariableFromLiteral(l1));
  model.Add(NewIntegerVariableFromLiteral(l2));
  model.Add(NewIntegerVariableFromLiteral(l3));

  auto* decomposer = model.GetOrCreate<ProductDecomposer>();
  const std::vector<LiteralValueValue> terms_a =
      decomposer->TryToDecompose(x0, x1);
  const std::vector<LiteralValueValue> expected_terms_a = {
      {l0, IntegerValue(2), IntegerValue(6)},
      {l1, IntegerValue(4), IntegerValue(6)},
      {l2, IntegerValue(2), IntegerValue(7)},
      {l3, IntegerValue(10), IntegerValue(6)},
  };
  EXPECT_EQ(terms_a, expected_terms_a);

  const std::vector<LiteralValueValue> terms_b =
      decomposer->TryToDecompose(x1, x0);
  const std::vector<LiteralValueValue> expected_terms_b = {
      {l0, IntegerValue(6), IntegerValue(2)},
      {l1, IntegerValue(6), IntegerValue(4)},
      {l2, IntegerValue(7), IntegerValue(2)},
      {l3, IntegerValue(6), IntegerValue(10)},
  };
  EXPECT_EQ(terms_b, expected_terms_b);
}

TEST(DecomposeProductTest, MatchingSizeTwoEncodingsFirstFirst) {
  Model model;

  const Literal l0(model.Add(NewBooleanVariable()), true);
  const IntegerVariable x0(model.Add(NewIntegerVariable(5, 6)));
  const IntegerVariable x1(model.Add(NewIntegerVariable(6, 7)));

  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  integer_encoder->AssociateToIntegerEqualValue(l0, x0, IntegerValue(5));
  integer_encoder->AssociateToIntegerEqualValue(l0, x1, IntegerValue(6));

  auto* decomposer = model.GetOrCreate<ProductDecomposer>();
  const std::vector<LiteralValueValue> terms_a =
      decomposer->TryToDecompose(x0, x1);
  const std::vector<LiteralValueValue> expected_terms_a = {
      {l0, IntegerValue(5), IntegerValue(6)},
      {l0.Negated(), IntegerValue(6), IntegerValue(7)},
  };
  EXPECT_EQ(terms_a, expected_terms_a);
}

TEST(DecomposeProductTest, MatchingSizeTwoEncodingsFirstLast) {
  Model model;

  const Literal l0(model.Add(NewBooleanVariable()), true);
  const IntegerVariable x0(model.Add(NewIntegerVariable(5, 6)));
  const IntegerVariable x1(model.Add(NewIntegerVariable(6, 7)));

  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  integer_encoder->AssociateToIntegerEqualValue(l0, x0, IntegerValue(5));
  integer_encoder->AssociateToIntegerEqualValue(l0, x1, IntegerValue(7));

  auto* decomposer = model.GetOrCreate<ProductDecomposer>();
  const std::vector<LiteralValueValue> terms_a =
      decomposer->TryToDecompose(x0, x1);
  const std::vector<LiteralValueValue> expected_terms_a = {
      {l0, IntegerValue(5), IntegerValue(7)},
      {l0.Negated(), IntegerValue(6), IntegerValue(6)},
  };
  EXPECT_EQ(terms_a, expected_terms_a);
}

TEST(DecomposeProductTest, MatchingSizeTwoEncodingslastFirst) {
  Model model;

  const Literal l0(model.Add(NewBooleanVariable()), true);
  const IntegerVariable x0(model.Add(NewIntegerVariable(5, 6)));
  const IntegerVariable x1(model.Add(NewIntegerVariable(6, 7)));

  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  integer_encoder->AssociateToIntegerEqualValue(l0, x0, IntegerValue(6));
  integer_encoder->AssociateToIntegerEqualValue(l0, x1, IntegerValue(6));

  auto* decomposer = model.GetOrCreate<ProductDecomposer>();
  const std::vector<LiteralValueValue> terms_a =
      decomposer->TryToDecompose(x0, x1);
  const std::vector<LiteralValueValue> expected_terms_a = {
      {l0.Negated(), IntegerValue(5), IntegerValue(7)},
      {l0, IntegerValue(6), IntegerValue(6)},
  };
  EXPECT_EQ(terms_a, expected_terms_a);
}

TEST(DecomposeProductTest, MatchingSizeTwoEncodingsLastLast) {
  Model model;

  const Literal l0(model.Add(NewBooleanVariable()), true);
  const IntegerVariable x0(model.Add(NewIntegerVariable(5, 6)));
  const IntegerVariable x1(model.Add(NewIntegerVariable(6, 7)));

  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  integer_encoder->AssociateToIntegerEqualValue(l0, x0, IntegerValue(6));
  integer_encoder->AssociateToIntegerEqualValue(l0, x1, IntegerValue(7));

  auto* decomposer = model.GetOrCreate<ProductDecomposer>();
  const std::vector<LiteralValueValue> terms_a =
      decomposer->TryToDecompose(x0, x1);
  const std::vector<LiteralValueValue> expected_terms_a = {
      {l0.Negated(), IntegerValue(5), IntegerValue(6)},
      {l0, IntegerValue(6), IntegerValue(7)},
  };
  EXPECT_EQ(terms_a, expected_terms_a);
}

TEST(ProductDetectorTest, BasicCases) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_detect_linearized_product(true);
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  auto* detector = model.GetOrCreate<ProductDetector>();
  detector->ProcessTernaryClause(Literals({+1, +2, +3}));
  detector->ProcessBinaryClause(Literals({-1, -2}));
  detector->ProcessBinaryClause(Literals({-1, -3}));
  EXPECT_EQ(kNoLiteralIndex, detector->GetProduct(Literal(-1), Literal(-2)));
  EXPECT_EQ(kNoLiteralIndex, detector->GetProduct(Literal(-1), Literal(-3)));
  EXPECT_EQ(Literal(+1).Index(),
            detector->GetProduct(Literal(-2), Literal(-3)));
}

TEST(ProductDetectorTest, BasicIntCase1) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_detect_linearized_product(true);
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  auto* detector = model.GetOrCreate<ProductDetector>();

  IntegerVariable x(10);
  IntegerVariable y(20);
  detector->ProcessConditionalZero(Literal(+1), x);
  detector->ProcessConditionalEquality(Literal(-1), x, y);

  EXPECT_EQ(x, detector->GetProduct(Literal(-1), y));
  EXPECT_EQ(kNoIntegerVariable, detector->GetProduct(Literal(-1), x));
  EXPECT_EQ(kNoIntegerVariable, detector->GetProduct(Literal(1), x));
  EXPECT_EQ(kNoIntegerVariable, detector->GetProduct(Literal(1), y));
}

TEST(ProductDetectorTest, BasicIntCase2) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_detect_linearized_product(true);
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  auto* detector = model.GetOrCreate<ProductDetector>();

  IntegerVariable x(10);
  IntegerVariable y(20);
  detector->ProcessConditionalEquality(Literal(-1), x, y);
  detector->ProcessConditionalZero(Literal(+1), x);

  EXPECT_EQ(x, detector->GetProduct(Literal(-1), y));
  EXPECT_EQ(kNoIntegerVariable, detector->GetProduct(Literal(-1), x));
  EXPECT_EQ(kNoIntegerVariable, detector->GetProduct(Literal(1), x));
  EXPECT_EQ(kNoIntegerVariable, detector->GetProduct(Literal(1), y));
}

TEST(ProductDetectorTest, RLT) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_add_rlt_cuts(true);
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);
  auto* detector = model.GetOrCreate<ProductDetector>();
  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();

  const Literal l0(model.Add(NewBooleanVariable()), true);
  const IntegerVariable x(model.Add(NewIntegerVariable(0, 1)));
  integer_encoder->AssociateToIntegerEqualValue(l0, x, IntegerValue(1));

  const Literal l1(model.Add(NewBooleanVariable()), true);
  const IntegerVariable y(model.Add(NewIntegerVariable(0, 1)));
  integer_encoder->AssociateToIntegerEqualValue(l1, y, IntegerValue(1));

  const Literal l2(model.Add(NewBooleanVariable()), true);
  const IntegerVariable z(model.Add(NewIntegerVariable(0, 1)));
  integer_encoder->AssociateToIntegerEqualValue(l2, z, IntegerValue(1));

  // X + (1 - Y) + Z >= 1
  detector->ProcessTernaryClause(Literals({+1, -2, +3}));

  // Lets choose value so that X + Z >= Y is tight.
  util_intops::StrongVector<IntegerVariable, double> lp_values(10, 0.0);
  lp_values[x] = 0.7;
  lp_values[y] = 0.9;
  lp_values[z] = 0.2;
  const absl::flat_hash_map<IntegerVariable, glop::ColIndex> lp_vars = {
      {x, glop::ColIndex(0)}, {y, glop::ColIndex(1)}, {z, glop::ColIndex(2)}};
  detector->InitializeBooleanRLTCuts(lp_vars, lp_values);

  // (1 - X) * Y <= Z,   0.3 * 0.9 == 0.27 <= 0.2,   interesting!
  // (1 - X) * (1 - Z) <= (1 - Y), 0.3 * 0.8 == 0.24 <= 0.1,   interesting !
  // Y * (1 - Z) <= X,  0.9 * 0.8 == 0.72 <= 0.7,  interesting !
  EXPECT_EQ(detector->BoolRLTCandidates().size(), 3);
  EXPECT_THAT(detector->BoolRLTCandidates().at(NegationOf(x)),
              UnorderedElementsAre(y, NegationOf(z)));
  EXPECT_THAT(detector->BoolRLTCandidates().at(y),
              UnorderedElementsAre(NegationOf(x), NegationOf(z)));
  EXPECT_THAT(detector->BoolRLTCandidates().at(NegationOf(z)),
              UnorderedElementsAre(y, NegationOf(x)));

  // And we can recover the literal ub.
  EXPECT_EQ(detector->LiteralProductUpperBound(NegationOf(x), y), z);
  EXPECT_EQ(detector->LiteralProductUpperBound(NegationOf(x), NegationOf(z)),
            NegationOf(y));
  EXPECT_EQ(detector->LiteralProductUpperBound(y, NegationOf(z)), x);

  // If we change values, we might get less candidates though
  lp_values[x] = 0.0;
  lp_values[y] = 0.2;
  lp_values[z] = 0.2;
  detector->InitializeBooleanRLTCuts(lp_vars, lp_values);

  // (1 - X) * Y <= Z,   1.0 * 0.2 <= 0.2,  tight, but not interesting.
  // (1 - X) * (1 - Z) <= (1 - Y), 1.0 * 0.8 <= 0.8  tight, but not interesting.
  // Y * (1 - Z) <= X,  0.2 * 0.8 <= 0.0,  interesting !
  EXPECT_EQ(detector->BoolRLTCandidates().size(), 2);
  EXPECT_THAT(detector->BoolRLTCandidates().at(y),
              UnorderedElementsAre(NegationOf(z)));
  EXPECT_THAT(detector->BoolRLTCandidates().at(NegationOf(z)),
              UnorderedElementsAre(y));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
