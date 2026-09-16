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

#include "ortools/math_opt/solver_tests/test_models.h"

#include <memory>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/testing/stream.h"

namespace operations_research::math_opt {

using ::testing::status::IsOkAndHolds;

TEST(TestModelClassTest, ToString) {
  EXPECT_EQ(ToString(TestModelClass::kIp), "ip");
  EXPECT_EQ(StreamToString(TestModelClass::kIp), "ip");
  EXPECT_EQ(ToString(TestModelClass::kLp), "lp");
  EXPECT_EQ(StreamToString(TestModelClass::kLp), "lp");
  EXPECT_EQ(ToString(TestModelClass::kMinCostFlow), "min_cost_flow");
  EXPECT_EQ(StreamToString(TestModelClass::kMinCostFlow), "min_cost_flow");
}

TEST(MinimalModelForTestModelClassTest, LpCanSolve) {
  std::unique_ptr<Model> model =
      MinimalModelForTestModelClass(TestModelClass::kLp);
  EXPECT_THAT(
      Solve(*model, SolverType::kGlop),
      IsOkAndHolds(IsOptimal(kMinimalModelForTestModelClassOptimalObjective)));
}

TEST(MinimalModelTest, IpCanSolve) {
  std::unique_ptr<Model> model =
      MinimalModelForTestModelClass(TestModelClass::kIp);
  EXPECT_THAT(
      Solve(*model, SolverType::kCpSat),
      IsOkAndHolds(IsOptimal(kMinimalModelForTestModelClassOptimalObjective)));
}

TEST(MinimalModelTest, MinCostFlowCanSolve) {
  std::unique_ptr<Model> model =
      MinimalModelForTestModelClass(TestModelClass::kMinCostFlow);
  // TODO: b/495435136 - use min cost flow solver here instead.
  EXPECT_THAT(
      Solve(*model, SolverType::kGlop),
      IsOkAndHolds(IsOptimal(kMinimalModelForTestModelClassOptimalObjective)));
}

TEST(NontrivialModelTest, LpCanSolve) {
  std::unique_ptr<Model> model = NontrivialModel(TestModelClass::kLp, 5);
  EXPECT_THAT(Solve(*model, SolverType::kGlop), IsOkAndHolds(IsOptimal()));
}

TEST(NontrivialModelTest, IpCanSolve) {
  std::unique_ptr<Model> model = NontrivialModel(TestModelClass::kIp, 5);
  EXPECT_THAT(Solve(*model, SolverType::kCpSat), IsOkAndHolds(IsOptimal()));
}

TEST(NontrivialModelTest, MinCostFlowCanSolve) {
  std::unique_ptr<Model> model =
      NontrivialModel(TestModelClass::kMinCostFlow, 5);
  // TODO: b/495435136 - use min cost flow solver here instead.
  EXPECT_THAT(Solve(*model, SolverType::kGlop), IsOkAndHolds(IsOptimal()));
}

TEST(SmallModelTest, Integer) {
  const std::unique_ptr<const Model> model = SmallModel(/*integer=*/true);
  EXPECT_THAT(Solve(*model, SolverType::kGscip), IsOkAndHolds(IsOptimal(9.0)));
}

TEST(SmallModelTest, Continuous) {
  const std::unique_ptr<const Model> model = SmallModel(/*integer=*/false);
  EXPECT_THAT(Solve(*model, SolverType::kGlop), IsOkAndHolds(IsOptimal(12.0)));
}

TEST(DenseIndependentSetTest, Integer) {
  const std::unique_ptr<const Model> model =
      DenseIndependentSet(/*integer=*/true);
  EXPECT_THAT(Solve(*model, SolverType::kGscip), IsOkAndHolds(IsOptimal(7.0)));
}

TEST(DenseIndependentSetTest, Continuous) {
  const std::unique_ptr<const Model> model =
      DenseIndependentSet(/*integer=*/false);
  EXPECT_THAT(Solve(*model, SolverType::kGlop),
              IsOkAndHolds(IsOptimal(10.0 * (5 + 4 + 3) / 2.0)));
}

TEST(DenseIndependentSetHint5Test, HintIsFeasibleWithObjective5) {
  const std::unique_ptr<Model> model = DenseIndependentSet(/*integer=*/true, 5);
  ModelSolveParameters model_params;
  const auto hint = DenseIndependentSetHint5(*model);
  model_params.solution_hints.push_back(hint);
  for (const auto& [var, value] : hint.variable_values) {
    model->set_lower_bound(var, value);
    model->set_upper_bound(var, value);
  }
  EXPECT_THAT(Solve(*model, SolverType::kGscip), IsOkAndHolds(IsOptimal(5.0)));
}

TEST(IndependentSetCompleteGraphTest, Integer) {
  const std::unique_ptr<const Model> model =
      IndependentSetCompleteGraph(/*integer=*/true);
  EXPECT_THAT(Solve(*model, SolverType::kGscip), IsOkAndHolds(IsOptimal(1.0)));
}

TEST(IndependentSetCompleteGraphTest, Continuous) {
  const std::unique_ptr<const Model> model =
      IndependentSetCompleteGraph(/*integer=*/false);
  EXPECT_THAT(Solve(*model, SolverType::kGlop), IsOkAndHolds(IsOptimal(5.0)));
}

}  // namespace operations_research::math_opt
