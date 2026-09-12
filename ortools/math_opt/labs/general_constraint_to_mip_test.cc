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

#include "ortools/math_opt/labs/general_constraint_to_mip.h"

#include <limits>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

using ::testing::HasSubstr;
using ::testing::status::StatusIs;

TEST(FormulateIndicatorConstraintAsMipTest, UnsetIndicatorConstraint) {
  // The only way to trigger this case from C++ is to create the indicator
  // constraint, and then delete the indicator variable.
  Model model;
  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddContinuousVariable(0, 1);
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, y <= 1);
  model.DeleteVariable(x);
  EXPECT_OK(FormulateIndicatorConstraintAsMip(model, c));
  EXPECT_EQ(model.num_linear_constraints(), 0);
  EXPECT_EQ(model.num_indicator_constraints(), 0);
}

TEST(FormulateIndicatorConstraintAsMipTest, ImpliedLowerBoundConstraint) {
  Model model;
  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddContinuousVariable(0, 1);
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, 2 * y + 3 >= 4);
  EXPECT_OK(FormulateIndicatorConstraintAsMip(model, c));
  EXPECT_EQ(model.num_indicator_constraints(), 0);
  ASSERT_EQ(model.num_linear_constraints(), 1);
  EXPECT_THAT(model.linear_constraint(0).AsBoundedLinearExpression(),
              IsNearlyEquivalent(2 * y >= x));
}

TEST(FormulateIndicatorConstraintAsMipTest, ImpliedUpperBoundConstraint) {
  Model model;
  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddContinuousVariable(0, 1);
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, 2 * y + 3 <= 4);
  EXPECT_OK(FormulateIndicatorConstraintAsMip(model, c));
  EXPECT_EQ(model.num_indicator_constraints(), 0);
  ASSERT_EQ(model.num_linear_constraints(), 1);
  EXPECT_THAT(model.linear_constraint(0).AsBoundedLinearExpression(),
              IsNearlyEquivalent(2 * y <= 2 - x));
}

TEST(FormulateIndicatorConstraintAsMipTest, ImpliedRangedConstraint) {
  Model model;
  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddContinuousVariable(0, 1);
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, 2 * y + 3 == 4);
  EXPECT_OK(FormulateIndicatorConstraintAsMip(model, c));
  EXPECT_EQ(model.num_indicator_constraints(), 0);
  ASSERT_EQ(model.num_linear_constraints(), 2);
  EXPECT_THAT(model.linear_constraint(0).AsBoundedLinearExpression(),
              IsNearlyEquivalent(2 * y >= x));
  EXPECT_THAT(model.linear_constraint(1).AsBoundedLinearExpression(),
              IsNearlyEquivalent(2 * y <= 2 - x));
}

TEST(FormulateIndicatorConstraintAsMipTest, ActivateOnZero) {
  Model model;
  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddContinuousVariable(0, 1);
  const IndicatorConstraint c = model.AddIndicatorConstraint(
      x, 2 * y + 3 >= 4, /*activate_on_zero=*/true);
  EXPECT_OK(FormulateIndicatorConstraintAsMip(model, c));
  EXPECT_EQ(model.num_indicator_constraints(), 0);
  ASSERT_EQ(model.num_linear_constraints(), 1);
  EXPECT_THAT(model.linear_constraint(0).AsBoundedLinearExpression(),
              IsNearlyEquivalent(2 * y >= 1 - x));
}

TEST(FormulateIndicatorConstraintAsMipTest, ContinuousIndicatorVariable) {
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1);
  const Variable y = model.AddContinuousVariable(0, 1);
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, 2 * y + 3 >= 4);
  EXPECT_THAT(
      FormulateIndicatorConstraintAsMip(model, c),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("binary")));
}

TEST(FormulateIndicatorConstraintAsMipTest, GeneralIntegerIndicatorVariable) {
  Model model;
  const Variable x = model.AddIntegerVariable(0, 2);
  const Variable y = model.AddContinuousVariable(0, 1);
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, 2 * y + 3 >= 4);
  EXPECT_THAT(
      FormulateIndicatorConstraintAsMip(model, c),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("binary")));
}

TEST(FormulateIndicatorConstraintAsMipTest,
     ImpliedConstraintUnboundedFromBelow) {
  Model model;
  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddContinuousVariable(-kInf, 1);
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, y >= 1);
  EXPECT_THAT(FormulateIndicatorConstraintAsMip(model, c),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("below")));
}

TEST(FormulateIndicatorConstraintAsMipTest,
     ImpliedConstraintUnboundedFromAbove) {
  Model model;
  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddContinuousVariable(0, kInf);
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, y <= 1);
  EXPECT_THAT(FormulateIndicatorConstraintAsMip(model, c),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("above")));
}

TEST(FormulateIndicatorConstraintAsMipTest,
     IndicatorConstraintFromAnotherModel) {
  Model model;
  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddContinuousVariable(0, kInf);
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, y <= 1);
  Model other;
  EXPECT_THAT(
      FormulateIndicatorConstraintAsMip(other, c),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("wrong model")));
}

}  // namespace
}  // namespace operations_research::math_opt
