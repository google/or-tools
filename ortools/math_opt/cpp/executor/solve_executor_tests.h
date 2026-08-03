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

#ifndef ORTOOLS_MATH_OPT_CPP_EXECUTOR_SOLVE_EXECUTOR_TESTS_H_
#define ORTOOLS_MATH_OPT_CPP_EXECUTOR_SOLVE_EXECUTOR_TESTS_H_

#include <functional>
#include <memory>
#include <ostream>
#include <string>

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/executor/solve_executor.h"

namespace operations_research::math_opt {

// What features are supported/unsupported for a SolveExecutor.
struct SupportedFeatures {
  // When interruption is not supported, we just ignore interruptions and
  // Solve() calls run to their conclusion.
  bool interruption = false;
  // When a solve callback is provided but not supported the solve returns an
  // error immediately.
  bool solve_callback = false;
  // Indicates that incremental solves are reusing state from previous solve,
  // rather than just solving from scratch.
  bool actual_incrementalism = false;
};

// Configures tests for an implementation of SolveExecutor.
//
// Implementation note: use `features` to control behavior that is conditional
// in tests. Use GTEST_SKIP only when there is a deficiency in an implementation
// that is worth warning the test runner about (it is noisy in logs). Use the
// name to skip tests only for bugs we can fix, otherwise just add a new
// feature.
struct SolveExecutorTestParameters {
  // Display information in test logs.
  std::string name;
  std::function<absl::StatusOr<std::unique_ptr<SolveExecutor>>()>
      executor_provider;
  // The functionality supported by SolveExecutor returned from
  // executor_provider.
  SupportedFeatures features;
};

std::ostream& operator<<(std::ostream& ostr,
                         const SolveExecutorTestParameters& params);

class SolveExecutorTest
    : public testing::TestWithParam<SolveExecutorTestParameters> {
 public:
  absl::StatusOr<std::unique_ptr<SolveExecutor>> MakeExecutor() {
    return GetParam().executor_provider();
  }
  const SupportedFeatures& features() const { return GetParam().features; }
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CPP_EXECUTOR_SOLVE_EXECUTOR_TESTS_H_
