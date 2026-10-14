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

#include "ortools/math_opt/io/lp/model_utils.h"

#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"
#include "ortools/math_opt/io/lp/lp_model.h"
#include "ortools/math_opt/io/lp/matchers.h"

namespace operations_research::lp_format {
namespace {

using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

TEST(RemoveUnusedVariablesTest, LinearConstraintKeepsVariableAlive) {
  LpModel model;
  ASSERT_OK(model.AddVariable("x").status());
  ASSERT_OK_AND_ASSIGN(const VariableIndex y, model.AddVariable("y"));
  ASSERT_OK(model.AddConstraint(
      {.terms = {{2.0, y}}, .relation = Relation::kEqual, .rhs = 4.0}));

  LpModel expected;
  ASSERT_OK_AND_ASSIGN(const VariableIndex expected_y,
                       expected.AddVariable("y"));
  ASSERT_OK(expected.AddConstraint({.terms = {{2.0, expected_y}},
                                    .relation = Relation::kEqual,
                                    .rhs = 4.0}));
  EXPECT_THAT(RemoveUnusedVariables(model), ModelEquals(expected));
}

TEST(RemoveUnusedVariablesTest, EmptyModelOk) {
  EXPECT_THAT(RemoveUnusedVariables(LpModel{}), ModelEquals(LpModel{}));
}

TEST(RemoveUnusedVariablesTest, CreatesEmptyModelOk) {
  LpModel model;
  ASSERT_OK(model.AddVariable("x"));
  EXPECT_THAT(RemoveUnusedVariables(model), ModelEquals(LpModel{}));
}

TEST(PermuteVariablesTest, SimpleModelOk) {
  LpModel model;
  util_intops::StrongVector<VariableIndex, VariableIndex> new_to_old;
  util_intops::StrongVector<VariableIndex, std::string> order_by_name;
  {
    ASSERT_OK_AND_ASSIGN(const VariableIndex x, model.AddVariable("x"));
    ASSERT_OK_AND_ASSIGN(const VariableIndex y, model.AddVariable("y"));
    ASSERT_OK_AND_ASSIGN(const VariableIndex z, model.AddVariable("z"));
    ASSERT_OK(model.AddConstraint({.terms = {{1.0, z}, {3.0, y}},
                                   .relation = Relation::kEqual,
                                   .rhs = 1.0}));
    ASSERT_OK(model.AddConstraint({.terms = {{5.0, x}, {7.0, z}},
                                   .relation = Relation::kEqual,
                                   .rhs = 1.0}));
    new_to_old = {y, z, x};
    order_by_name = {"y", "z", "x"};
  }

  LpModel expected;
  ASSERT_OK_AND_ASSIGN(const VariableIndex y, expected.AddVariable("y"));
  ASSERT_OK_AND_ASSIGN(const VariableIndex z, expected.AddVariable("z"));
  ASSERT_OK_AND_ASSIGN(const VariableIndex x, expected.AddVariable("x"));
  ASSERT_OK(expected.AddConstraint({.terms = {{1.0, z}, {3.0, y}},
                                    .relation = Relation::kEqual,
                                    .rhs = 1.0}));
  ASSERT_OK(expected.AddConstraint({.terms = {{5.0, x}, {7.0, z}},
                                    .relation = Relation::kEqual,
                                    .rhs = 1.0}));
  EXPECT_THAT(PermuteVariables(model, new_to_old),
              IsOkAndHolds(ModelEquals(expected)));
  EXPECT_THAT(PermuteVariables(model, order_by_name),
              IsOkAndHolds(ModelEquals(expected)));
}

TEST(PermuteVariablesTest, EmptyModelOk) {
  LpModel model;
  const util_intops::StrongVector<VariableIndex, VariableIndex> new_to_old;
  const util_intops::StrongVector<VariableIndex, std::string> order_by_name;
  EXPECT_THAT(PermuteVariables(model, new_to_old),
              IsOkAndHolds(ModelEquals(LpModel{})));
  EXPECT_THAT(PermuteVariables(model, order_by_name),
              IsOkAndHolds(ModelEquals(LpModel{})));
}

TEST(PermuteVariablesTest, BadNameIsError) {
  LpModel model;
  ASSERT_OK(model.AddVariable("x"));
  const util_intops::StrongVector<VariableIndex, std::string> order_by_name = {
      "yYz"};
  EXPECT_THAT(PermuteVariables(model, order_by_name),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("yYz")));
}

TEST(PermuteVariablesTest, VarIndexOutOfBounds) {
  LpModel model;
  ASSERT_OK(model.AddVariable("x"));
  const util_intops::StrongVector<VariableIndex, VariableIndex> new_to_old = {
      VariableIndex(4)};
  EXPECT_THAT(PermuteVariables(model, new_to_old),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("4")));
}

TEST(PermuteVariablesTest, DuplicateVariable) {
  LpModel model;
  ASSERT_OK_AND_ASSIGN(const VariableIndex x, model.AddVariable("x"));
  ASSERT_OK(model.AddVariable("y"));
  const util_intops::StrongVector<VariableIndex, VariableIndex> new_to_old = {
      x, x};
  EXPECT_THAT(PermuteVariables(model, new_to_old),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("twice")));
}

TEST(PermuteVariablesTest, MissingVariable) {
  LpModel model;
  ASSERT_OK(model.AddVariable("x"));
  ASSERT_OK_AND_ASSIGN(const VariableIndex y, model.AddVariable("y"));
  const util_intops::StrongVector<VariableIndex, VariableIndex> new_to_old = {
      y};
  EXPECT_THAT(PermuteVariables(model, new_to_old),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("old VariableIndex: 0")));
}

}  // namespace

}  // namespace operations_research::lp_format
