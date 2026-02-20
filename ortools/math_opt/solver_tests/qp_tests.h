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

#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_QP_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_QP_TESTS_H_

#include <iosfwd>
#include <ostream>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

enum class QpSupportType { kNoQpSupport, kDiagonalQpOnly, kConvexQp };

struct QpTestParameters {
  QpTestParameters(SolverType solver_type, SolveParameters parameters,
                   QpSupportType qp_support,
                   bool supports_incrementalism_not_modifying_qp,
                   bool supports_qp_incrementalism, bool use_integer_variables);

  // The tested solver.
  SolverType solver_type;

  SolveParameters parameters;

  QpSupportType qp_support;

  // True if the solver supports updates that do not modify existing quadratic
  // objectives (adding quadratic objectives to LPs are OK).
  bool supports_incrementalism_not_modifying_qp;

  // True if the solver supports arbitrary updates that change (add, delete, or
  // update) quadratic objective coefficients.
  bool supports_qp_incrementalism;

  // True if the solver supports integer variables.
  bool use_integer_variables;
};

std::ostream& operator<<(std::ostream& out, const QpTestParameters& params);

// A suite of unit tests for quadratic objectives. Note that a solver that does
// not support quadratic objectives should still use this fixture to ensure that
// it is not silently ignoring one.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>SimpleQpTest, SimpleQpTest,
//       testing::Values(QpTestParameters(SolverType::k<Solver>, parameters,
//                                        qp_support,
//                                        supports_incrementalism_not_modifying_qp,
//                                        supports_qp_incrementalism,
//                                        use_integer_variables)));
class SimpleQpTest : public testing::TestWithParam<QpTestParameters> {
 protected:
  SolverType TestedSolver() const { return GetParam().solver_type; }
  absl::StatusOr<SolveResult> SimpleSolve(const Model& model) {
    return Solve(model, TestedSolver(), {.parameters = GetParam().parameters});
  }
};

// A suite of unit tests focused on incrementalism with quadratic objectives.
// Note that a solver that does not support quadratic objectives should still
// use this fixture to ensure that it is not silently ignoring one.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>IncrementalQpTest, IncrementalQpTest,
//       testing::Values(QpTestParameters(SolverType::k<Solver>, parameters,
//                                        qp_support,
//                                        supports_incrementalism_not_modifying_qp,
//                                        supports_qp_incrementalism,
//                                        use_integer_variables)));
class IncrementalQpTest : public testing::TestWithParam<QpTestParameters> {
 protected:
  SolverType TestedSolver() const { return GetParam().solver_type; }
};

// A suite of unit tests focused on testing dual solutions from QP solvers.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>QpDualsTest, QpDualsTest,
//       testing::Values(QpTestParameters(SolverType::k<Solver>, parameters,
//                                        qp_support,
//                                        supports_incrementalism_not_modifying_qp,
//                                        supports_qp_incrementalism,
//                                        use_integer_variables)));
class QpDualsTest : public testing::TestWithParam<QpTestParameters> {
 protected:
  SolverType TestedSolver() const { return GetParam().solver_type; }
  absl::StatusOr<SolveResult> SimpleSolve(const Model& model) {
    return Solve(model, TestedSolver(), {.parameters = GetParam().parameters});
  }
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_QP_TESTS_H_
