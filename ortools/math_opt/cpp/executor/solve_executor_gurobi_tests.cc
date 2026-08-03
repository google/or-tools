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

#include "ortools/math_opt/cpp/executor/solve_executor_gurobi_tests.h"

#include <memory>
#include <ostream>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/gurobi/gurobi_testing.h"
#include "ortools/math_opt/cpp/executor/executor_init_args.h"
#include "ortools/math_opt/cpp/executor/solve_executor.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/util/testing_utils.h"

namespace operations_research::math_opt {

namespace {

using ::testing::status::IsOkAndHolds;

}  // namespace

std::ostream& operator<<(std::ostream& ostr,
                         const SolveExecutorGurobiTestParameters& params) {
  ostr << params.name;
  return ostr;
}

math_opt::ExecutorSolverInitArguments
SolveExecutorGurobiTestParameters::init_args() const {
  math_opt::ExecutorSolverInitArguments result;
  if (isv_key.has_value()) {
    result.streamable.gurobi = math_opt::StreamableGurobiInitArguments{
        .isv_key = math_opt::GurobiISVKey::FromProto(*isv_key),
    };
  }
  return result;
}

void RunSolveSuccess(const SolveExecutorGurobiTestParameters& params) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  model.Maximize(x);
  std::unique_ptr<SolveExecutor> executor = params.executor_provider();
  EXPECT_THAT(
      executor->Solve(model, SolverType::kGurobi, {}, params.init_args()),
      IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 1.0}})));
}

void RunIISSuccess(const SolveExecutorGurobiTestParameters& params) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const Variable z = model.AddBinaryVariable("z");
  model.AddLinearConstraint(x + y <= 1);
  model.AddLinearConstraint(y + z <= 1);
  model.AddLinearConstraint(x + z <= 1);
  model.AddLinearConstraint(x + y + z >= 2);
  model.Maximize(x);
  std::unique_ptr<SolveExecutor> executor = params.executor_provider();
  EXPECT_THAT(executor->ComputeInfeasibleSubsystem(model, SolverType::kGurobi,
                                                   {}, params.init_args()),
              IsOkAndHolds(IsInfeasible()));
}

namespace {

TEST_P(SolveExecutorGurobiTest, SolveSuccess) {
  if (kAnyXsanEnabled) {
    GTEST_SKIP() << kGurobiFailsWithXSAN;
  }
  RunSolveSuccess(GetParam());
}

TEST_P(SolveExecutorGurobiTest, IISSuccess) {
  if (kAnyXsanEnabled) {
    GTEST_SKIP() << kGurobiFailsWithXSAN;
  }
  RunIISSuccess(GetParam());
}

}  // namespace
}  // namespace operations_research::math_opt
