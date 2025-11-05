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

// This header groups parameteric tests to validates behaviors common to MIP and
// LP solvers.
#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_GENERIC_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_GENERIC_TESTS_H_

#include <optional>
#include <ostream>
#include <string>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace math_opt {

struct GenericTestParameters {
  GenericTestParameters(SolverType solver_type, bool support_interrupter,
                        bool integer_variables, std::string expected_log,
                        SolveParameters solve_parameters = {});

  // The tested solver.
  SolverType solver_type;

  // True if the solver support SolveInterrupter.
  bool support_interrupter;

  // True if the tests should be performed with integer variables.
  bool integer_variables;

  // A message included in the solver logs when an optimal solution is found.
  std::string expected_log;

  // Additional parameters to control the solve.
  SolveParameters solve_parameters;

  friend std::ostream& operator<<(std::ostream& out,
                                  const GenericTestParameters& params);
};

// A suite of unit tests to validate that mandatory behavior for all (MIP and
// LP) solvers.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>GenericTest, GenericTest,
//       testing::Values(GenericTestParameters(
//         SolverType::k<Solver>,
//         /*support_interrupter=*/true,
//         /*integer_variables=*/true
//       )));
class GenericTest : public ::testing::TestWithParam<GenericTestParameters> {
 protected:
  static absl::StatusOr<SolveResult> SimpleSolve(const Model& model) {
    return Solve(model, GetParam().solver_type,
                 {.parameters = GetParam().solve_parameters});
  }
};

struct TimeLimitTestParameters {
  TimeLimitTestParameters(
      const SolverType solver_type, const bool integer_variables,
      const std::optional<CallbackEvent> supported_event = std::nullopt)
      : solver_type(solver_type),
        integer_variables(integer_variables),
        event(supported_event) {}

  // The tested solver.
  SolverType solver_type;

  // The test problem will be a 0-1 IP if true, otherwise will be an LP.
  bool integer_variables;

  // A supported callback event, or nullopt if no event is supported.
  std::optional<CallbackEvent> event;

  friend std::ostream& operator<<(std::ostream& out,
                                  const TimeLimitTestParameters& params);
};

// A suite of unit tests to show that time limits are handled correctly.
//
// These tests require that the underlying solver supports a callback. The tests
// will create either a small LP or IP, depending on the bool
// integer_variables below.
//
// To use these tests, in file <solver>_test.cc write:
//   INSTANTIATE_TEST_SUITE_P(<Solver>TimeLimitTest, TimeLimitTest,
//     testing::Values(TimeLimitTestParameters(
//       SolverType::k<Solver>, <integer_variables>, CALLBACK_EVENT_<EVENT>)));
class TimeLimitTest : public ::testing::TestWithParam<TimeLimitTestParameters> {
 public:
  SolverType TestedSolver() const { return GetParam().solver_type; }
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_GENERIC_TESTS_H_
