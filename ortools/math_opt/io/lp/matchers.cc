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

#include "ortools/math_opt/io/lp/matchers.h"

#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/io/lp/lp_model.h"
#include "ortools/math_opt/io/lp/model_utils.h"

namespace operations_research::lp_format {
namespace {
using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::status::IsOkAndHolds;

Matcher<LpModel> ModelEquivToLeaveUnusedNames(const LpModel& expected) {
  return ResultOf(
      "permute_variables",
      [var_order = expected.variables()](const LpModel& actual) {
        return PermuteVariables(actual, var_order);
      },
      IsOkAndHolds(ModelEquals(expected)));
}

}  // namespace

Matcher<Constraint> ConstraintEquals(const Constraint& expected) {
  return AllOf(
      Field("terms", &Constraint::terms, ElementsAreArray(expected.terms)),
      Field("relation", &Constraint::relation, expected.relation),
      Field("rhs", &Constraint::rhs, expected.rhs),
      Field("name", &Constraint::name, expected.name));
}

Matcher<LpModel> ModelEquals(const LpModel& expected) {
  std::vector<Matcher<Constraint>> expected_constraints;
  for (const Constraint& constraint : expected.constraints()) {
    expected_constraints.push_back(ConstraintEquals(constraint));
  }
  return AllOf(Property("variables", &LpModel::variables,
                        ElementsAreArray(expected.variables())),
               Property("constraints", &LpModel::constraints,
                        ElementsAreArray(expected_constraints)));
}

Matcher<LpModel> ModelEquivTo(const LpModel& expected,
                              const bool remove_unused_variables) {
  if (!remove_unused_variables) {
    return ModelEquivToLeaveUnusedNames(expected);
  }
  return ResultOf(
      "remove_unused_variables",
      [](const LpModel& actual) { return RemoveUnusedVariables(actual); },
      ModelEquivToLeaveUnusedNames(RemoveUnusedVariables(expected)));
}

}  // namespace operations_research::lp_format
