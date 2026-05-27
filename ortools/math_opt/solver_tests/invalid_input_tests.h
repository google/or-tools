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

#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_INVALID_INPUT_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_INVALID_INPUT_TESTS_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/solver_tests/base_solver_test.h"
#include "ortools/math_opt/solver_tests/test_models.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research {
namespace math_opt {

struct InvalidInputTestParameters {
  SolverType solver_type;
  TestModelClass model_class;

  InvalidInputTestParameters(const SolverType solver_type,
                             const TestModelClass model_class)
      : solver_type(solver_type), model_class(model_class) {}

  // Returns true if model_class uses integer variables (i.e., is `kIp`).
  bool uses_integer_variables() const {
    return model_class == TestModelClass::kIp;
  }

  friend std::ostream& operator<<(std::ostream& out,
                                  const InvalidInputTestParameters& params);
};

// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>InvalidInputTest, InvalidInputTest,
//       testing::Values(InvalidInputTestParameters(
//         SolverType::k<Solver>, TestModelClass::kIp)));
// TODO(b/172553545): this test should not be repeated for each solver since it
//   tests that the Solver class validates the model before calling the
//   interface.
class InvalidInputTest
    : public ::testing::TestWithParam<InvalidInputTestParameters> {
 protected:
  SolverType TestedSolver() const { return GetParam().solver_type; }
};

struct InvalidParameterTestParams {
  InvalidParameterTestParams(
      SolverType solver_type, TestModelClass model_class,
      SolveParameters solve_parameters,
      std::vector<std::string> expected_error_substrings);

  SolverType solver_type;
  TestModelClass model_class;
  SolveParameters solve_parameters;
  std::vector<std::string> expected_error_substrings;

  friend std::ostream& operator<<(std::ostream& out,
                                  const InvalidParameterTestParams& params);
};

class InvalidParameterTest
    : public ::testing::TestWithParam<InvalidParameterTestParams> {
 protected:
  InvalidParameterTest();

  absl::StatusOr<SolveResult> SimpleSolve(
      const SolveParameters& parameters = GetParam().solve_parameters) {
    return Solve(*model_, GetParam().solver_type, {.parameters = parameters});
  }

  const std::unique_ptr<const Model> model_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_INVALID_INPUT_TESTS_H_
