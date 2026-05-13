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

#ifndef OR_TOOLS_MATH_OPT_SOLVER_TESTS_QC_TESTS_H_
#define OR_TOOLS_MATH_OPT_SOLVER_TESTS_QC_TESTS_H_

#include <ostream>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

struct QcTestParameters {
  explicit QcTestParameters(SolverType solver_type, SolveParameters parameters,
                            bool supports_qc,
                            bool supports_incremental_add_and_deletes,
                            bool supports_incremental_variable_deletions,
                            bool use_integer_variables);

  // The tested solver.
  SolverType solver_type;

  SolveParameters parameters;

  // True if the solver supports quadratic constraints.
  bool supports_qc;

  // True if the solver supports incremental updates that add and/or delete
  // quadratic constraints.
  bool supports_incremental_add_and_deletes;

  // True if the solver supports updates that delete variables involved in
  // quadratic constraints.
  bool supports_incremental_variable_deletions;

  // True if the solver supports integer variables.
  bool use_integer_variables;
};

std::ostream& operator<<(std::ostream& out, const QcTestParameters& params);

// A suite of unit tests for (convex) quadratic constraints.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>SimpleQcTest, SimpleQcTest,
//       testing::Values(QcTestParameters(SolverType::k<Solver>,
//                                        parameters, supports_qc,
//                                        supports_incremental_add_and_deletes,
//                                        supports_incremental_variable_deletions,
//                                        use_integer_variables)));
class SimpleQcTest : public testing::TestWithParam<QcTestParameters> {
 protected:
  absl::StatusOr<SolveResult> SimpleSolve(const Model& model) {
    return Solve(model, GetParam().solver_type,
                 {.parameters = GetParam().parameters});
  }
};

// A suite of unit tests focused on incrementalism with quadratic constraints.
// Note that a solver that does not support quadratic constraints should still
// use this fixture to ensure that it is not silently ignoring one.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>IncrementalQcTest, IncrementalQcTest,
//       testing::Values(QcTestParameters(SolverType::k<Solver>,
//                                        parameters, supports_qc,
//                                        supports_incremental_add_and_deletes,
//                                        supports_incremental_variable_deletions,
//                                        use_integer_variables)));
class IncrementalQcTest : public testing::TestWithParam<QcTestParameters> {};

// A suite of unit tests focused on testing dual solutions from QC solvers.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>QcDualsTest, QcDualsTest,
//       testing::Values(QcTestParameters(SolverType::k<Solver>,
//                                        parameters, supports_qc,
//                                        supports_incremental_add_and_deletes,
//                                        supports_incremental_variable_deletions,
//                                        use_integer_variables)));
class QcDualsTest : public testing::TestWithParam<QcTestParameters> {
 protected:
  absl::StatusOr<SolveResult> SimpleSolve(const Model& model) {
    return Solve(model, GetParam().solver_type,
                 {.parameters = GetParam().parameters});
  }
};
}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVER_TESTS_QC_TESTS_H_
