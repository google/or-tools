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

#include "ortools/math_opt/labs/solution_feasibility_checker.h"

#include <limits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::EqualsProto;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::ResultOf;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

MATCHER(ModelSubsetIsEmpty, "") { return arg.empty(); }

testing::Matcher<ModelSubset> ModelSubsetIs(const ModelSubset& expected) {
  const ModelSubsetProto expected_proto = expected.Proto();
  return ResultOf([](const ModelSubset& actual) { return actual.Proto(); },
                  EqualsProto(expected_proto));
}

// Returns all subsets of the integers {0, ..., n-1}.
std::vector<absl::flat_hash_set<int>> AllSubsets(const int n) {
  std::vector<absl::flat_hash_set<int>> subsets;
  for (int k = 0; k < n; ++k) {
    std::vector<bool> perm(n, false);
    for (int i = 0; i < k; ++i) {
      perm[i] = true;
    }
    do {
      absl::flat_hash_set<int> subset;
      for (int i = 0; i < n; ++i) {
        if (perm[i]) {
          subset.insert(i);
        }
      }
      subsets.push_back(std::move(subset));
    } while (absl::c_prev_permutation(perm));
  }
  return subsets;
}

// Difference between largest and smallest elements of `set`.
int DiameterOfSet(const absl::flat_hash_set<int>& set) {
  if (set.empty()) {
    return 0;
  }
  return *absl::c_max_element(set) - *absl::c_min_element(set);
}

TEST(CheckPrimalSolutionFeasibilityTest, EmptyModelOk) {
  EXPECT_THAT(CheckPrimalSolutionFeasibility(Model(), {}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
}

TEST(CheckPrimalSolutionFeasibilityTest, VariableBounds) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0);
  {
    const VariableMap<double> values = {{x, 0.0 - 1.0e-5}};
    const ModelSubset violates_lower{
        .variable_bounds = {{x, {.lower = true, .upper = false}}}};
    EXPECT_THAT(CheckPrimalSolutionFeasibility(model, values),
                IsOkAndHolds(ModelSubsetIs(violates_lower)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-6}),
                IsOkAndHolds(ModelSubsetIs(violates_lower)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-4}),
                IsOkAndHolds(ModelSubsetIsEmpty()));
  }
  {
    const VariableMap<double> values = {{x, 1.0 + 1.0e-5}};
    const ModelSubset violates_upper{
        .variable_bounds = {{x, {.lower = false, .upper = true}}}};
    EXPECT_THAT(CheckPrimalSolutionFeasibility(model, values),
                IsOkAndHolds(ModelSubsetIs(violates_upper)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-6}),
                IsOkAndHolds(ModelSubsetIs(violates_upper)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-4}),
                IsOkAndHolds(ModelSubsetIsEmpty()));
  }
}

TEST(CheckPrimalSolutionFeasibilityTest, VariableIntegrality) {
  Model model;
  const Variable x = model.AddIntegerVariable(-1.0, 2.0);
  const ModelSubset violates{.variable_integrality = {x}};
  {
    const VariableMap<double> values = {{x, 0.0 - 1.0e-4}};
    EXPECT_THAT(CheckPrimalSolutionFeasibility(model, values),
                IsOkAndHolds(ModelSubsetIs(violates)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.integrality_tolerance = 1.0e-5}),
                IsOkAndHolds(ModelSubsetIs(violates)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.integrality_tolerance = 1.0e-3}),
                IsOkAndHolds(ModelSubsetIsEmpty()));
  }
  {
    const VariableMap<double> values = {{x, 1.0 + 1.0e-4}};
    EXPECT_THAT(CheckPrimalSolutionFeasibility(model, values),
                IsOkAndHolds(ModelSubsetIs(violates)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.integrality_tolerance = 1.0e-5}),
                IsOkAndHolds(ModelSubsetIs(violates)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.integrality_tolerance = 1.0e-3}),
                IsOkAndHolds(ModelSubsetIsEmpty()));
  }
}

TEST(CheckPrimalSolutionFeasibilityTest, LinearConstraint) {
  Model model;
  const Variable x = model.AddVariable();
  const LinearConstraint c = model.AddLinearConstraint(0.0 <= x <= 1.0);
  {
    const VariableMap<double> values = {{x, 0.0 - 1.0e-5}};
    const ModelSubset violates_lower{
        .linear_constraints = {{c, {.lower = true, .upper = false}}}};
    EXPECT_THAT(CheckPrimalSolutionFeasibility(model, values),
                IsOkAndHolds(ModelSubsetIs(violates_lower)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-6}),
                IsOkAndHolds(ModelSubsetIs(violates_lower)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-4}),
                IsOkAndHolds(ModelSubsetIsEmpty()));
  }
  {
    const VariableMap<double> values = {{x, 1.0 + 1.0e-5}};
    const ModelSubset violates_upper{
        .linear_constraints = {{c, {.lower = false, .upper = true}}}};
    EXPECT_THAT(CheckPrimalSolutionFeasibility(model, values),
                IsOkAndHolds(ModelSubsetIs(violates_upper)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-6}),
                IsOkAndHolds(ModelSubsetIs(violates_upper)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-4}),
                IsOkAndHolds(ModelSubsetIsEmpty()));
  }
}

TEST(CheckPrimalSolutionFeasibilityTest, QuadraticConstraint) {
  Model model;
  const Variable x = model.AddVariable();
  const QuadraticConstraint c =
      model.AddQuadraticConstraint(1.0 <= x * x <= 1.0);
  {
    // (1 - 1.0e-4)^2 == 0.99980001 == 1 - 1.9999e-4
    const VariableMap<double> values = {{x, 1.0 - 1.0e-4}};
    const ModelSubset violates_lower{
        .quadratic_constraints = {{c, {.lower = true, .upper = false}}}};
    EXPECT_THAT(CheckPrimalSolutionFeasibility(model, values),
                IsOkAndHolds(ModelSubsetIs(violates_lower)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-5}),
                IsOkAndHolds(ModelSubsetIs(violates_lower)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-3}),
                IsOkAndHolds(ModelSubsetIsEmpty()));
  }
  {
    // (1 + 1.0e-4)^2 == 1.00020001 == 1 + 2.0001e-4
    const VariableMap<double> values = {{x, 1.0 + 1.0e-4}};
    const ModelSubset violates_upper{
        .quadratic_constraints = {{c, {.lower = false, .upper = true}}}};
    EXPECT_THAT(CheckPrimalSolutionFeasibility(model, values),
                IsOkAndHolds(ModelSubsetIs(violates_upper)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-5}),
                IsOkAndHolds(ModelSubsetIs(violates_upper)));
    EXPECT_THAT(CheckPrimalSolutionFeasibility(
                    model, values, {.absolute_constraint_tolerance = 1.0e-3}),
                IsOkAndHolds(ModelSubsetIsEmpty()));
  }
}

TEST(CheckPrimalSolutionFeasibilityTest, SecondOrderConeConstraint) {
  Model model;
  const Variable x = model.AddVariable();
  // sqrt((3x)^2 + (4x)^2) == 5|x|
  const SecondOrderConeConstraint c =
      model.AddSecondOrderConeConstraint({3.0 * x, 4.0 * x}, 5.0 * x - 1.0e-6);
  const VariableMap<double> values = {{x, 1.0}};
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, values),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  EXPECT_THAT(CheckPrimalSolutionFeasibility(
                  model, values, {.absolute_constraint_tolerance = 1.0e-5}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  EXPECT_THAT(CheckPrimalSolutionFeasibility(
                  model, values, {.absolute_constraint_tolerance = 1.0e-7}),
              IsOkAndHolds(ModelSubsetIs(
                  ModelSubset{.second_order_cone_constraints = {c}})));
}

TEST(CheckPrimalSolutionFeasibilityTest, Sos1Constraint) {
  Model model;
  const Variable x = model.AddVariable();
  const Sos1Constraint c =
      model.AddSos1Constraint({0.0, x, 0.0, 100.0 * x, 0.0});
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 0.0}}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 1.0e-8}}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  // Values (0 if zero, 1 if nonzero w.r.t. tolerance): {0, 0, 0, 0, 0}
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 1.0e-8}},
                                             {.nonzero_tolerance = 1.0e-5}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  // Values (0 if zero, 1 if nonzero w.r.t. tolerance): {0, 0, 0, 1, 0}
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 1.0e-8}},
                                             {.nonzero_tolerance = 1.0e-7}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  // Values (0 if zero, 1 if nonzero w.r.t. tolerance): {0, 1, 0, 1, 0}
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 1.0e-8}},
                                     {.nonzero_tolerance = 1.0e-9}),
      IsOkAndHolds(ModelSubsetIs(ModelSubset{.sos1_constraints = {c}})));
}

TEST(CheckPrimalSolutionFeasibilityTest, Sos1ConstraintAllSubsets) {
  Model model;
  constexpr int kN = 5;
  constexpr double kTol = 1.0e-5;
  constexpr double kNonzeroAboveTol = 1.0e-4;
  constexpr double kNonzeroBelowTol = 1.0e-6;
  std::vector<Variable> variables;
  std::vector<LinearExpression> expressions;
  for (int i = 0; i < kN; ++i) {
    variables.push_back(model.AddVariable());
    expressions.push_back(variables.back());
  }
  const Sos1Constraint c = model.AddSos1Constraint(expressions);
  for (const absl::flat_hash_set<int> subset : AllSubsets(kN)) {
    VariableMap<double> values;
    for (int i = 0; i < kN; ++i) {
      values[variables[i]] =
          subset.contains(i) ? kNonzeroAboveTol : kNonzeroBelowTol;
    }
    if (subset.size() <= 1) {
      EXPECT_THAT(CheckPrimalSolutionFeasibility(model, values,
                                                 {.nonzero_tolerance = kTol}),
                  IsOkAndHolds(ModelSubsetIsEmpty()));
    } else {
      EXPECT_THAT(
          CheckPrimalSolutionFeasibility(model, values,
                                         {.nonzero_tolerance = kTol}),
          IsOkAndHolds(ModelSubsetIs(ModelSubset{.sos1_constraints = {c}})));
    }
  }
}

TEST(CheckPrimalSolutionFeasibilityTest, Sos2Constraint) {
  Model model;
  const Variable x = model.AddVariable();
  const Sos2Constraint c =
      model.AddSos2Constraint({0.0, x, 0.0, 20.0 * x, 100.0 * x, 0.0});
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 0.0}}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 1.0e-8}}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  // Values (0 if zero, 1 if nonzero w.r.t. tolerance): {0, 0, 0, 0, 0, 0}
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 1.0e-8}},
                                             {.nonzero_tolerance = 1.0e-5}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  // Values (0 if zero, 1 if nonzero w.r.t. tolerance): {0, 0, 0, 1, 1, 0}
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 1.0e-8}},
                                             {.nonzero_tolerance = 1.0e-7}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  // Values (0 if zero, 1 if nonzero w.r.t. tolerance): {0, 1, 0, 1, 1, 0}
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 1.0e-8}},
                                     {.nonzero_tolerance = 1.0e-9}),
      IsOkAndHolds(ModelSubsetIs(ModelSubset{.sos2_constraints = {c}})));
}

TEST(CheckPrimalSolutionFeasibilityTest, Sos2ConstraintAllSubsets) {
  Model model;
  constexpr int kN = 5;
  constexpr double kTol = 1.0e-5;
  constexpr double kNonzeroAboveTol = 1.0e-4;
  constexpr double kNonzeroBelowTol = 1.0e-6;
  std::vector<Variable> variables;
  std::vector<LinearExpression> expressions;
  for (int i = 0; i < kN; ++i) {
    variables.push_back(model.AddVariable());
    expressions.push_back(variables.back());
  }
  const Sos2Constraint c = model.AddSos2Constraint(expressions);
  for (const absl::flat_hash_set<int> subset : AllSubsets(kN)) {
    VariableMap<double> values;
    for (int i = 0; i < kN; ++i) {
      values[variables[i]] =
          subset.contains(i) ? kNonzeroAboveTol : kNonzeroBelowTol;
    }
    if (DiameterOfSet(subset) <= 1) {
      EXPECT_THAT(CheckPrimalSolutionFeasibility(model, values,
                                                 {.nonzero_tolerance = kTol}),
                  IsOkAndHolds(ModelSubsetIsEmpty()));
    } else {
      EXPECT_THAT(
          CheckPrimalSolutionFeasibility(model, values,
                                         {.nonzero_tolerance = kTol}),
          IsOkAndHolds(ModelSubsetIs(ModelSubset{.sos2_constraints = {c}})));
    }
  }
}

TEST(CheckPrimalSolutionFeasibilityTest, IndicatorConstraintNullIndicator) {
  Model model;
  // We add a binary variable, use it as the indicator variable, then delete it.
  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddVariable();
  model.AddIndicatorConstraint(x, 0.0 <= y <= 1.0);
  model.DeleteVariable(x);
  // The implied constraint is violated, but that's OK since the indicator
  // variable is null.
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{y, -1.0}}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{y, 2.0}}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
}

TEST(CheckPrimalSolutionFeasibilityTest, IndicatorConstraint) {
  Model model;
  // This should really be binary, but we don't want to test variable
  // integrality checking here.
  const Variable x = model.AddVariable();
  const Variable y = model.AddVariable();
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, 0 <= y <= 1.0);
  // Indicator variable is not at its activation value.
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 0.0}, {y, -1.0}}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 0.0}, {y, 2.0}}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  // Indicator variable is exactly at its activation value.
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 1.0}, {y, -1.0}}),
      IsOkAndHolds(ModelSubsetIs(ModelSubset{.indicator_constraints = {c}})));
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 1.0}, {y, 2.0}}),
      IsOkAndHolds(ModelSubsetIs(ModelSubset{.indicator_constraints = {c}})));
  // Indicator variable is sufficiently close to its activation value.
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 1.0 - 1.0e-6}, {y, 2.0}},
                                     {.nonzero_tolerance = 1.0e-5}),
      IsOkAndHolds(ModelSubsetIs(ModelSubset{.indicator_constraints = {c}})));
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 1.0 + 1.0e-6}, {y, 2.0}},
                                     {.nonzero_tolerance = 1.0e-5}),
      IsOkAndHolds(ModelSubsetIs(ModelSubset{.indicator_constraints = {c}})));
  // Indicator variable is sufficiently far away from its activation value.
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 1.0 - 1.0e-4}, {y, 2.0}},
                                     {.nonzero_tolerance = 1.0e-5}),
      IsOkAndHolds(ModelSubsetIsEmpty()));
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 1.0 + 1.0e-4}, {y, 2.0}},
                                     {.nonzero_tolerance = 1.0e-5}),
      IsOkAndHolds(ModelSubsetIsEmpty()));
}

TEST(CheckPrimalSolutionFeasibilityTest, IndicatorConstraintActivateAtZero) {
  Model model;
  // This should really be binary, but we don't want to test variable
  // integrality checking here.
  const Variable x = model.AddVariable();
  const Variable y = model.AddVariable();
  const IndicatorConstraint c =
      model.AddIndicatorConstraint(x, 0 <= y <= 1.0, /*activate_on_zero=*/true);
  // Indicator variable is not at its activation value.
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 1.0}, {y, -1.0}}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  EXPECT_THAT(CheckPrimalSolutionFeasibility(model, {{x, 1.0}, {y, 2.0}}),
              IsOkAndHolds(ModelSubsetIsEmpty()));
  // Indicator variable is exactly at its activation value.
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 0.0}, {y, -1.0}}),
      IsOkAndHolds(ModelSubsetIs(ModelSubset{.indicator_constraints = {c}})));
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 0.0}, {y, 2.0}}),
      IsOkAndHolds(ModelSubsetIs(ModelSubset{.indicator_constraints = {c}})));
  // Indicator variable is sufficiently close to its activation value.
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 0.0 - 1.0e-6}, {y, 2.0}},
                                     {.nonzero_tolerance = 1.0e-5}),
      IsOkAndHolds(ModelSubsetIs(ModelSubset{.indicator_constraints = {c}})));
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 0.0 + 1.0e-6}, {y, 2.0}},
                                     {.nonzero_tolerance = 1.0e-5}),
      IsOkAndHolds(ModelSubsetIs(ModelSubset{.indicator_constraints = {c}})));
  // Indicator variable is sufficiently far away from its activation value.
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 0.0 - 1.0e-4}, {y, 2.0}},
                                     {.nonzero_tolerance = 1.0e-5}),
      IsOkAndHolds(ModelSubsetIsEmpty()));
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x, 0.0 + 1.0e-4}, {y, 2.0}},
                                     {.nonzero_tolerance = 1.0e-5}),
      IsOkAndHolds(ModelSubsetIsEmpty()));
}

TEST(CheckPrimalSolutionFeasibilityTest, VariableInValuesButNotModel) {
  Model model;
  Model other;
  const Variable x_other = other.AddVariable("x_other");
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {{x_other, 1.0}}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("x_other")));
}

TEST(CheckPrimalSolutionFeasibilityTest, VariableInModelButNotValues) {
  Model model;
  model.AddVariable("x_model");
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(model, {}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("x_model")));
}

TEST(CheckPrimalSolutionFeasibilityTest, InvalidAbsoluteErrorTolerance) {
  EXPECT_THAT(CheckPrimalSolutionFeasibility(
                  Model(), {}, {.absolute_constraint_tolerance = -1.0e-6}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("absolute_constraint_tolerance")));
  EXPECT_THAT(CheckPrimalSolutionFeasibility(
                  Model(), {}, {.absolute_constraint_tolerance = kNaN}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("absolute_constraint_tolerance")));
}

TEST(CheckPrimalSolutionFeasibilityTest, InvalidIntegralityTolerance) {
  EXPECT_THAT(CheckPrimalSolutionFeasibility(
                  Model(), {}, {.integrality_tolerance = -1.0e-6}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("integrality_tolerance")));
  EXPECT_THAT(CheckPrimalSolutionFeasibility(Model(), {},
                                             {.integrality_tolerance = kNaN}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("integrality_tolerance")));
}

TEST(CheckPrimalSolutionFeasibilityTest, InvalidNonzeroAbsoluteTolerance) {
  EXPECT_THAT(CheckPrimalSolutionFeasibility(Model(), {},
                                             {.nonzero_tolerance = -1.0e-6}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("nonzero_tolerance")));
  EXPECT_THAT(
      CheckPrimalSolutionFeasibility(Model(), {}, {.nonzero_tolerance = kNaN}),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("nonzero_tolerance")));
}

TEST(ViolatedConstraintsAsStringsTest, NoViolatedConstraints) {
  Model model;
  EXPECT_THAT(ViolatedConstraintsAsStrings(model, {}, {}),
              IsOkAndHolds(IsEmpty()));
}

TEST(ViolatedConstraintsAsStringsTest, ViolatedVariableBounds) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  EXPECT_THAT(
      ViolatedConstraintsAsStrings(
          model, {.variable_bounds = {{x, {.upper = true}}}}, {{x, 1.1}}),
      IsOkAndHolds(ElementsAre(
          "violated variable bound: 0 ≤ x ≤ 1, with variable value 1.1")));
}

TEST(ViolatedConstraintsAsStringsTest, ViolatedVariableIntegrality) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  EXPECT_THAT(
      ViolatedConstraintsAsStrings(model, {.variable_integrality = {x}},
                                   {{x, 1.1}}),
      IsOkAndHolds(ElementsAre(
          "violated variable integrality: x, with variable value 1.1")));
}

TEST(ViolatedConstraintsAsStringsTest, ViolatedLinearConstraint) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c = model.AddLinearConstraint(x <= 1.0, "c");
  EXPECT_THAT(
      ViolatedConstraintsAsStrings(
          model, {.linear_constraints = {{c, {.upper = true}}}}, {{x, 1.1}}),
      IsOkAndHolds(ElementsAre("violated linear constraint c: x ≤ 1, with "
                               "variable values {{x, 1.1}}")));
}

TEST(ViolatedConstraintsAsStringsTest, ViolatedQuadraticConstraint) {
  Model model;
  const Variable x = model.AddVariable("x");
  const QuadraticConstraint c = model.AddQuadraticConstraint(x * x <= 1.0, "c");
  EXPECT_THAT(
      ViolatedConstraintsAsStrings(
          model, {.quadratic_constraints = {{c, {.upper = true}}}}, {{x, 1.1}}),
      IsOkAndHolds(ElementsAre("violated quadratic constraint c: x² ≤ 1, with "
                               "variable values {{x, 1.1}}")));
}

TEST(ViolatedConstraintsAsStringsTest, ViolatedSecondOrderConeConstraint) {
  Model model;
  const Variable x = model.AddVariable("x");
  const SecondOrderConeConstraint c =
      model.AddSecondOrderConeConstraint({x}, 1.0, "c");
  EXPECT_THAT(ViolatedConstraintsAsStrings(
                  model, {.second_order_cone_constraints = {c}}, {{x, 1.1}}),
              IsOkAndHolds(ElementsAre(
                  "violated second-order cone constraint c: ||{x}||₂ ≤ 1, with "
                  "variable values {{x, 1.1}}")));
}

TEST(ViolatedConstraintsAsStringsTest, ViolatedSos1Constraint) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Sos1Constraint c = model.AddSos1Constraint({x, 1.0}, {}, "c");
  EXPECT_THAT(ViolatedConstraintsAsStrings(model, {.sos1_constraints = {c}},
                                           {{x, 1.0}}),
              IsOkAndHolds(ElementsAre(
                  "violated SOS1 constraint c: {x, 1} is SOS1, with "
                  "variable values {{x, 1}}")));
}

TEST(ViolatedConstraintsAsStringsTest, ViolatedSos2Constraint) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Sos2Constraint c = model.AddSos2Constraint({x, 0.0, 1.0}, {}, "c");
  EXPECT_THAT(ViolatedConstraintsAsStrings(model, {.sos2_constraints = {c}},
                                           {{x, 1.0}}),
              IsOkAndHolds(ElementsAre(
                  "violated SOS2 constraint c: {x, 0, 1} is SOS2, with "
                  "variable values {{x, 1}}")));
}

TEST(ViolatedConstraintsAsStringsTest, ViolatedIndicatorConstraint) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const IndicatorConstraint c =
      model.AddIndicatorConstraint(y, x <= 1.0, false, "c");
  EXPECT_THAT(ViolatedConstraintsAsStrings(
                  model, {.indicator_constraints = {c}}, {{x, 1.1}, {y, 1.0}}),
              IsOkAndHolds(ElementsAre(
                  "violated indicator constraint c: y = 1 ⇒ x ≤ 1, with "
                  "variable values {{x, 1.1}, {y, 1}}")));
}

}  // namespace
}  // namespace operations_research::math_opt
