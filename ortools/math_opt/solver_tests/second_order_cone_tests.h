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

#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_SECOND_ORDER_CONE_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_SECOND_ORDER_CONE_TESTS_H_

#include <ostream>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

struct SecondOrderConeTestParameters {
  explicit SecondOrderConeTestParameters(
      SolverType solver_type, SolveParameters parameters,
      bool supports_soc_constraints, bool supports_incremental_add_and_deletes);

  // The tested solver.
  SolverType solver_type;

  SolveParameters parameters;

  // True if the solver supports second-order cone constraints.
  bool supports_soc_constraints;

  // True if the solver supports incremental updates that add and/or delete
  // second-order cone constraints.
  bool supports_incremental_add_and_deletes;
};

std::ostream& operator<<(std::ostream& out,
                         const SecondOrderConeTestParameters& params);

// A suite of unit tests for second-order cone constraints.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>SimpleSecondOrderConeTest, SimpleSecondOrderConeTest,
//       testing::Values(SecondOrderConeTestParameters(
//                           SolverType::k<Solver>, parameters,
//                           /*supports_soc_constraints=*/false,
//                           /*supports_incremental_add_and_deletes=*/false)));
class SimpleSecondOrderConeTest
    : public testing::TestWithParam<SecondOrderConeTestParameters> {
 protected:
  absl::StatusOr<SolveResult> SimpleSolve(const Model& model) {
    return Solve(model, GetParam().solver_type,
                 {.parameters = GetParam().parameters});
  }
};

// A suite of unit tests focused on incrementalism with second-order cone
// constraints. Note that a solver that does not support second-order cone
// constraints should still use this fixture to ensure that it is not silently
// ignoring one.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>IncrementalSecondOrderConeTest, IncrementalSecondOrderConeTest,
//       testing::Values(SecondOrderConeTestParameters(
//                           SolverType::k<Solver>, parameters,
//                           /*supports_soc_constraints=*/false,
//                           /*supports_incremental_add_and_deletes=*/false)));
class IncrementalSecondOrderConeTest
    : public testing::TestWithParam<SecondOrderConeTestParameters> {};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_SECOND_ORDER_CONE_TESTS_H_
