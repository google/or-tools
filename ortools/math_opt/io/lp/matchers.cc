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

#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/io/lp/lp_model.h"

namespace operations_research::lp_format {
namespace {
using ::testing::AllOf;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Property;
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

}  // namespace operations_research::lp_format
