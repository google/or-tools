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

#ifndef OR_TOOLS_MATH_OPT_SOLVER_TESTS_CALLBACK_TESTS_H_
#define OR_TOOLS_MATH_OPT_SOLVER_TESTS_CALLBACK_TESTS_H_

#include <functional>
#include <iosfwd>
#include <optional>
#include <ostream>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/solver_tests/base_solver_test.h"

namespace operations_research {
namespace math_opt {

// Parameters for the MessageCallbackTest suite below.
struct MessageCallbackTestParams {
  MessageCallbackTestParams(SolverType solver_type,
                            bool support_message_callback,
                            bool support_interrupter, bool integer_variables,
                            std::string ending_substring,
                            SolveParameters solve_parameters = {});

  // The tested solver.
  SolverType solver_type;

  // True if the solver is expected to support message callbacks. False if not,
  // in which case the solver is expected to ignore the callback.
  bool support_message_callback;

  // True if the solver supports SolveInterrupter.
  bool support_interrupter;

  // True if the tests should be performed with integer variables.
  bool integer_variables;

  // A sub-string expected to be found on the last log lines.
  std::string ending_substring;

  // Additional parameters to control the solve.
  SolveParameters solve_parameters;

  friend std::ostream& operator<<(std::ostream& out,
                                  const MessageCallbackTestParams& params);
};

// A suite of unit tests to validates that a solver handles message callbacks
// correctly.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>MessageCallbackTest, MessageCallbackTest,
//       testing::Values(MessageCallbackTestParams(
//         SolverType::k<Solver>,
//         /*use_new_message_callback=*/true,
//         /*support_message_callback=*/true,
//         /*support_interrupter=*/true,
//         "ENDING")));
class MessageCallbackTest
    : public ::testing::TestWithParam<MessageCallbackTestParams> {};

// Parameters for CallbackTest.
struct CallbackTestParams {
  CallbackTestParams(SolverType solver_type, bool integer_variables,
                     bool add_lazy_constraints, bool add_cuts,
                     absl::flat_hash_set<CallbackEvent> supported_events,
                     std::optional<SolveParameters> all_solutions,
                     std::optional<SolveParameters> reaches_cut_callback);

  // The solver to test.
  SolverType solver_type;

  // True if the tests should be performed with integer variables.
  bool integer_variables;

  // If the solver supports adding lazy constraints at the MIP_SOLUTION event.
  bool add_lazy_constraints;

  // If the solver supports adding cuts at the event MIP_NODE.
  bool add_cuts;

  // The events that should be supported by the solver.
  absl::flat_hash_set<CallbackEvent> supported_events;

  // For a small feasibility problem (objective is zero) with <= 10 feasible
  // solutions, ensure the solver finds all solutions.
  std::optional<SolveParameters> all_solutions;

  // Disable as much as possible of presolve, (solver) cuts, and heuristics, so
  // that we can run a custom cut on this problem. Not setting this value will
  // result in the test on adding cuts at event kMipNode not running.
  std::optional<SolveParameters> reaches_cut_callback;

  friend std::ostream& operator<<(std::ostream& out,
                                  const CallbackTestParams& params);
};

// A suite of unit tests to show that a solver handles other callbacks
// correctly.
//
// Note that the tests use callbacks that are not thread-safe and request that
// the underlying solvers run in single threaded mode.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(<Solver>CallbackTest, CallbackTest,
//                            testing::Values(CallbackTestParams(
//                              SolverType::k<Solver>,
//                              /*integer_variables=*/true,
//                              /*supported_events=*/{...}));
// (In `Values` above, you can put any list of `SolverType`s).
class CallbackTest : public ::testing::TestWithParam<CallbackTestParams> {};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVER_TESTS_CALLBACK_TESTS_H_
