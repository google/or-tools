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

#ifndef ORTOOLS_MATH_OPT_CPP_EXECUTOR_SOLVE_EXECUTOR_GUROBI_TESTS_H_
#define ORTOOLS_MATH_OPT_CPP_EXECUTOR_SOLVE_EXECUTOR_GUROBI_TESTS_H_

#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/executor/executor_init_args.h"
#include "ortools/math_opt/cpp/executor/solve_executor.h"

namespace operations_research::math_opt {

struct SolveExecutorGurobiTestParameters {
  std::string name;
  std::function<std::unique_ptr<SolveExecutor>()> executor_provider;
  std::optional<GurobiInitializerProto::ISVKey> isv_key;

  ExecutorSolverInitArguments init_args() const;
  std::function<absl::Status()> setup = []() { return absl::OkStatus(); };
};

std::ostream& operator<<(std::ostream& ostr,
                         const SolveExecutorGurobiTestParameters& params);

class SolveExecutorGurobiTest
    : public testing::TestWithParam<SolveExecutorGurobiTestParameters> {
 public:
  std::unique_ptr<SolveExecutor> MakeExecutor() {
    return GetParam().executor_provider();
  }
  void SetUp() override { ASSERT_OK(GetParam().setup()); }
};

// TODO(b/347046083): delete these functions once we can use UOSS with ITS.
void RunSolveSuccess(const SolveExecutorGurobiTestParameters& params);
void RunIISSuccess(const SolveExecutorGurobiTestParameters& params);

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CPP_EXECUTOR_SOLVE_EXECUTOR_GUROBI_TESTS_H_
