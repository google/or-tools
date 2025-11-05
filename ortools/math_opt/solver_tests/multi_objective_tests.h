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

#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_MULTI_OBJECTIVE_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_MULTI_OBJECTIVE_TESTS_H_

#include <iosfwd>
#include <ostream>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

struct MultiObjectiveTestParameters {
  MultiObjectiveTestParameters(
      SolverType solver_type, SolveParameters parameters,
      bool supports_auxiliary_objectives,
      bool supports_incremental_objective_add_and_delete,
      bool supports_incremental_objective_modification,
      bool supports_integer_variables);

  // The tested solver.
  SolverType solver_type;

  SolveParameters parameters;

  // True if the solver supports auxiliary objectives.
  bool supports_auxiliary_objectives;

  // True if the solver supports incrementally adding and deleting auxiliary
  // objectives.
  bool supports_incremental_objective_add_and_delete;

  // True if the solver supports incremental, in-place modification of
  // objectives in multi-objective models.
  bool supports_incremental_objective_modification;

  // True if the solver supports integer variables.
  bool supports_integer_variables;
};

std::ostream& operator<<(std::ostream& out,
                         const MultiObjectiveTestParameters& params);

// A suite of unit tests for multiple objectives. Note that a solver that does
// not support multiple objectives should still use this fixture to ensure that
// it is not silently ignoring one.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>SimpleMultiObjectiveTest, SimpleMultiObjectiveTest,
//       testing::Values(
//           MultiObjectiveTestParameters(
//               SolverType::k<Solver>, parameters,
//               /*supports_multi_objectives=*/false,
//               /*supports_incremental_objective_add_and_delete=*/false,
//               /*supports_incremental_objective_modification=*/false)));
class SimpleMultiObjectiveTest
    : public testing::TestWithParam<MultiObjectiveTestParameters> {
 protected:
  absl::StatusOr<SolveResult> SimpleSolve(const Model& model) {
    return Solve(model, GetParam().solver_type,
                 {.parameters = GetParam().parameters});
  }
};

// A suite of unit tests focused on incrementalism with multiple objectives.
// Note that a solver that does not support multiple objectives should still use
// this fixture to ensure that it is not silently ignoring one.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>IncrementalMultiObjectiveTest, IncrementalMultiObjectiveTest,
//       testing::Values(
//           MultiObjectiveTestParameters(
//               SolverType::k<Solver>, parameters,
//               /*supports_multi_objectives=*/false,
//               /*supports_incremental_objective_add_and_delete=*/false,
//               /*supports_incremental_objective_modification=*/false)));
class IncrementalMultiObjectiveTest
    : public testing::TestWithParam<MultiObjectiveTestParameters> {};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_MULTI_OBJECTIVE_TESTS_H_
