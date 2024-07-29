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

#include "ortools/math_opt/io/lp/matchers.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/io/lp/lp_model.h"

namespace operations_research::lp_format {
namespace {

TEST(ConstraintEqualsTest, Equal) {
  const Constraint c = {
      .terms = {{1.0, VariableIndex(0)}, {4.0, VariableIndex(3)}},
      .relation = Relation::kEqual,
      .rhs = 5.0,
      .name = "cat"};
  EXPECT_THAT(c, ConstraintEquals(c));
}

TEST(ConstraintEqualsTest, WrongNameNoMatch) {
  const Constraint c = {
      .terms = {{1.0, VariableIndex(0)}, {4.0, VariableIndex(3)}},
      .relation = Relation::kEqual,
      .rhs = 5.0,
      .name = "cat"};
  Constraint d = c;
  d.name = "dog";
  EXPECT_THAT(c, Not(ConstraintEquals(d)));
}

TEST(ConstraintEqualsTest, WrongRhsNoMatch) {
  const Constraint c = {
      .terms = {{1.0, VariableIndex(0)}, {4.0, VariableIndex(3)}},
      .relation = Relation::kEqual,
      .rhs = 5.0,
      .name = "cat"};
  Constraint d = c;
  d.rhs = 4.0;
  EXPECT_THAT(c, Not(ConstraintEquals(d)));
}

TEST(ConstraintEqualsTest, WrongRelationNoMatch) {
  const Constraint c = {
      .terms = {{1.0, VariableIndex(0)}, {4.0, VariableIndex(3)}},
      .relation = Relation::kEqual,
      .rhs = 5.0,
      .name = "cat"};
  Constraint d = c;
  d.relation = Relation::kGreaterOrEqual;
  EXPECT_THAT(c, Not(ConstraintEquals(d)));
}

TEST(ConstraintEqualsTest, WrongTermsNoMatch) {
  const Constraint c = {
      .terms = {{1.0, VariableIndex(0)}, {4.0, VariableIndex(3)}},
      .relation = Relation::kEqual,
      .rhs = 5.0,
      .name = "cat"};
  Constraint d = c;
  d.terms.clear();
  EXPECT_THAT(c, Not(ConstraintEquals(d)));
}

TEST(ModelEqualsTest, ModelEqualsSelf) {
  LpModel model;
  ASSERT_OK_AND_ASSIGN(const VariableIndex x, model.AddVariable("x"));
  ASSERT_OK(model.AddConstraint({.terms = {{2.0, x}},
                                 .relation = Relation::kLessOrEqual,
                                 .rhs = 4.0,
                                 .name = "c"}));
  EXPECT_THAT(model, ModelEquals(model));
}

TEST(ModelEqualsTest, EmptyModelsEqual) {
  LpModel actual;
  LpModel expected;
  EXPECT_THAT(actual, ModelEquals(expected));
}

TEST(ModelEqualsTest, DifferentVariablesNotEqual) {
  LpModel actual;
  ASSERT_OK(actual.AddVariable("x"));

  LpModel expected;
  ASSERT_OK(expected.AddVariable("y"));

  EXPECT_THAT(actual, Not(ModelEquals(expected)));
}

TEST(ModelEqualsTest, DifferentConstraintsNotEqual) {
  LpModel actual;
  ASSERT_OK_AND_ASSIGN(const VariableIndex x_actual, actual.AddVariable("x"));
  ASSERT_OK(actual.AddConstraint({.terms = {{2.0, x_actual}},
                                  .relation = Relation::kLessOrEqual,
                                  .rhs = 4.0,
                                  .name = "c"}));

  LpModel expected;
  ASSERT_OK_AND_ASSIGN(const VariableIndex x_expected,
                       expected.AddVariable("x"));
  // RHS is different
  ASSERT_OK(actual.AddConstraint({.terms = {{2.0, x_expected}},
                                  .relation = Relation::kLessOrEqual,
                                  .rhs = 5.0,
                                  .name = "c"}));

  EXPECT_THAT(actual, Not(ModelEquals(expected)));
}

}  // namespace
}  // namespace operations_research::lp_format
