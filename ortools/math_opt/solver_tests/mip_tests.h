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

#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_MIP_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_MIP_TESTS_H_

#include <memory>
#include <ostream>

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/base_solver_test.h"

namespace operations_research {
namespace math_opt {

struct SimpleMipTestParameters {
  explicit SimpleMipTestParameters(SolverType solver_type,
                                   bool report_unboundness_correctly = true);

  // The tested solver.
  SolverType solver_type;

  // True if the solver reports unbound MIPs as UNBOUNDED or DUAL_INFEASIBLE. If
  // false, the solver is expected to return OTHER_ERROR.
  //
  // TODO(b/202159173): remove this when we start using the direct CP-SAT API
  // and thus will be able to get proper details.
  bool report_unboundness_correctly;
};

std::ostream& operator<<(std::ostream& out,
                         const SimpleMipTestParameters& params);

// A suite of unit tests to validate that mandatory behavior for MIP solvers.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>SimpleMipTest, SimpleMipTest,
//       testing::Values(SimpleMipTestParameters(SolverType::k<Solver>)));
class SimpleMipTest : public ::testing::TestWithParam<SimpleMipTestParameters> {
};

class IncrementalMipTest : public BaseSolverTest {
 protected:
  IncrementalMipTest();

  Model model_;
  const Variable x_;
  const Variable y_;
  const LinearConstraint c_;
  std::unique_ptr<IncrementalSolver> solver_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_MIP_TESTS_H_
