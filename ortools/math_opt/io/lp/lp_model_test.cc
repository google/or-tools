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

#include "ortools/math_opt/io/lp/lp_model.h"

#include <limits>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/io/lp/matchers.h"

namespace operations_research::lp_format {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(LpModelTest, EmptyModel) {
  LpModel model;
  EXPECT_THAT(model.variables(), IsEmpty());
  EXPECT_THAT(model.variable_names(), IsEmpty());
  EXPECT_THAT(model.constraints(), IsEmpty());
}

TEST(LpModelTest, AddVariableSuccess) {
  LpModel model;
  EXPECT_THAT(model.AddVariable("x"), IsOkAndHolds(VariableIndex{0}));
  EXPECT_THAT(model.AddVariable("y78"), IsOkAndHolds(VariableIndex{1}));
  EXPECT_THAT(model.variables(), ElementsAre("x", "y78"));
  EXPECT_THAT(model.variable_names(),
              UnorderedElementsAre(Pair("x", VariableIndex{0}),
                                   Pair("y78", VariableIndex{1})));
}

TEST(LpModelTest, AddVariableRepeatNameError) {
  LpModel model;
  EXPECT_OK(model.AddVariable("xyz"));
  EXPECT_THAT(model.AddVariable("xyz"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("duplicate"), HasSubstr("xyz"))));
}

TEST(LpModelTest, AddVariableInvalidNameError) {
  LpModel model;
  EXPECT_THAT(model.AddVariable("4x"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("invalid variable"), HasSubstr("4x"))));
}

TEST(LpModelTest, AddConstraintSuccess) {
  LpModel model;
  ASSERT_OK_AND_ASSIGN(const VariableIndex x, model.AddVariable("x"));
  const Constraint c = {.terms = {{4.0, x}},
                        .relation = Relation::kEqual,
                        .rhs = 2.5,
                        .name = "c"};
  EXPECT_THAT(model.AddConstraint(c), IsOkAndHolds(ConstraintIndex{0}));
  EXPECT_THAT(model.constraints(), ElementsAre(ConstraintEquals(c)));
}

TEST(LpModelTest, AddConstraintNoTermsError) {
  LpModel model;
  const Constraint c = {.relation = Relation::kEqual, .rhs = 2.5, .name = "c"};
  EXPECT_THAT(model.AddConstraint(c),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("term")));
}

TEST(LpModelTest, AddConstraintBadRelationError) {
  LpModel model;
  ASSERT_OK_AND_ASSIGN(const VariableIndex x, model.AddVariable("x"));
  const Constraint c = {.terms = {{4.0, x}},
                        .relation = static_cast<Relation>(-12),
                        .rhs = 2.5,
                        .name = "c"};
  EXPECT_THAT(
      model.AddConstraint(c),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("Relation")));
}

TEST(LpModelTest, AddConstraintNanInTermsError) {
  LpModel model;
  ASSERT_OK_AND_ASSIGN(const VariableIndex x, model.AddVariable("x"));
  const Constraint c = {
      .terms = {{std::numeric_limits<double>::quiet_NaN(), x}},
      .relation = Relation::kEqual,
      .rhs = 2.5,
      .name = "c"};
  EXPECT_THAT(model.AddConstraint(c),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("NaN")));
}

TEST(LpModelTest, AddConstraintInfInTermsError) {
  LpModel model;
  ASSERT_OK_AND_ASSIGN(const VariableIndex x, model.AddVariable("x"));
  const Constraint c = {.terms = {{kInf, x}},
                        .relation = Relation::kEqual,
                        .rhs = 2.5,
                        .name = "c"};
  EXPECT_THAT(
      model.AddConstraint(c),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("finite")));
}

TEST(LpModelTest, AddConstraintNanInRhsError) {
  LpModel model;
  ASSERT_OK_AND_ASSIGN(const VariableIndex x, model.AddVariable("x"));
  const Constraint c = {.terms = {{2.0, x}},
                        .relation = Relation::kEqual,
                        .rhs = std::numeric_limits<double>::quiet_NaN(),
                        .name = "c"};
  EXPECT_THAT(model.AddConstraint(c),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("NaN")));
}

TEST(LpModelTest, AddConstraintVariableNotInModelError) {
  LpModel model;
  const VariableIndex x(0);
  const Constraint c = {.terms = {{2.0, x}},
                        .relation = Relation::kEqual,
                        .rhs = 4.0,
                        .name = "c"};
  EXPECT_THAT(
      model.AddConstraint(c),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("variable ids")));
}

template <typename T>
std::string ToString(const T& t) {
  std::stringstream ss;
  ss << t;
  return ss.str();
}

TEST(RelationStream, PrintsString) {
  EXPECT_EQ(ToString(Relation::kEqual), "=");
  EXPECT_EQ(ToString(Relation::kGreaterOrEqual), ">=");
  EXPECT_EQ(ToString(Relation::kLessOrEqual), "<=");
  EXPECT_THAT(ToString(static_cast<Relation>(-1)), HasSubstr("invalid"));
}

TEST(ConstraintStream, PrintsString) {
  const Constraint c = {
      .terms = {{1.0, VariableIndex(0)}, {4.5, VariableIndex(3)}},
      .relation = Relation::kEqual,
      .rhs = 5.0,
      .name = "cat"};
  // StrEq gives better output than EXPECT_EQ on failure
  EXPECT_THAT(
      ToString(c),
      StrEq("terms: {{1, 0}, {4.5, 3}} relation: = rhs: 5 name: \"cat\""));
}

// These tests are intentionally not exhaustive. Better to test the stream
// operator by round tripping with parsing. We do not have a strong contract
// on the string produced, and testing against a large string is brittle.
TEST(LpModelTest, BasicStreamTest) {
  LpModel model;
  ASSERT_OK_AND_ASSIGN(const VariableIndex x, model.AddVariable("x"));
  const Constraint c = {.terms = {{4.0, x}},
                        .relation = Relation::kEqual,
                        .rhs = 2.5,
                        .name = "c"};
  ASSERT_OK(model.AddConstraint(c));
  // StrEq gives better output than EXPECT_EQ on failure
  EXPECT_THAT(ToString(model), StrEq("SUBJECT TO\n  c: 4 x = 2.5\nEND\n"));
}

}  // namespace
}  // namespace operations_research::lp_format
