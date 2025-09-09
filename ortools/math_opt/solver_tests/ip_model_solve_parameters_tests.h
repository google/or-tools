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

#ifndef OR_TOOLS_MATH_OPT_SOLVER_TESTS_IP_MODEL_SOLVE_PARAMETERS_TESTS_H_
#define OR_TOOLS_MATH_OPT_SOLVER_TESTS_IP_MODEL_SOLVE_PARAMETERS_TESTS_H_

#include <iosfwd>
#include <optional>
#include <ostream>
#include <string>

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/base_solver_test.h"

namespace operations_research {
namespace math_opt {

// Parameterized test suite that validates that an implementation of
// SolverInterface::Solve() for an Integer Programming solver takes into account
// correctly the input ModelSolveParametersProto.
//
// Usage:
//
//   INSTANTIATE_TEST_SUITE_P(<Solver>IpModelSolveParametersTest,
//                            IpModelSolveParametersTest,
//                            testing::Values(SolverType::k<Solver>));
//
class IpModelSolveParametersTest : public BaseSolverTest {};

// Parameters for the MipSolutionHintTest suite below.
struct SolutionHintTestParams {
  SolutionHintTestParams(SolverType solver_type,
                         std::optional<SolveParameters> single_hint_params,
                         std::optional<SolveParameters> two_hint_params,
                         std::string hint_accepted_message_regex)
      : solver_type(solver_type),
        single_hint_params(single_hint_params),
        two_hint_params(two_hint_params),
        hint_accepted_message_regex(hint_accepted_message_regex) {}

  // The tested solver.
  SolverType solver_type;

  // Should be non-null if the solver supports a single hint. Furthermore, it
  // must ensure that the solve terminates with the hinted solution, rather than
  // the optimal solution. Some values (e.g. enable_output) may be overridden.
  std::optional<SolveParameters> single_hint_params;

  // Should be non-null if the solver supports a two hints. Furthermore, it
  // must ensure that the solve terminates with the two hinted solutions (and
  // returns both solutions), rather than the optimal solution. Some values
  // (e.g. enable_output) may be overridden.
  std::optional<SolveParameters> two_hint_params;

  // A testing::ContainsRegex-compatible regex for the expected hint-acceptance
  // message. see http://go/gunitadvanced#regular-expression-syntax)
  std::string hint_accepted_message_regex;

  friend std::ostream& operator<<(std::ostream& out,
                                  const SolutionHintTestParams& params);
};

// A suite of unit tests to show that an MIP solver handles solution hints
// correctly.
//
// To use these tests, in file <solver>_test.cc write:
//   INSTANTIATE_TEST_SUITE_P(<Solver>MipSolutionHintTest, MipSolutionHintTest,
//     testing::Values(solution_hint_test_params));
class MipSolutionHintTest
    : public ::testing::TestWithParam<SolutionHintTestParams> {
 protected:
  SolverType TestedSolver() const { return GetParam().solver_type; }
  const std::optional<SolveParameters>& SingleHintParams() const {
    return GetParam().single_hint_params;
  }
  const std::optional<SolveParameters>& TwoHintParams() const {
    return GetParam().two_hint_params;
  }
  const std::string& HintAcceptedMessageRegex() const {
    return GetParam().hint_accepted_message_regex;
  }
};

// Parameters for the BranchPrioritiesTest suite below.
struct BranchPrioritiesTestParams {
  BranchPrioritiesTestParams(SolverType solver_type,
                             SolveParameters solve_params)
      : solver_type(solver_type), solve_params(solve_params) {}

  // The tested solver.
  SolverType solver_type;

  // Should ensure the solver behaves as close as possible to a pure
  // branch-and-bound solver (e.g. turn presolve, heuristics and cuts off).
  // Major deviations from this could cause the test to fail.
  SolveParameters solve_params;

  friend std::ostream& operator<<(std::ostream& out,
                                  const BranchPrioritiesTestParams& params);
};

// A suite of unit tests to show that an MIP solver handles branching priorities
// correctly.
//
// To use these tests, in file <solver>_test.cc write:
//   INSTANTIATE_TEST_SUITE_P(<Solver>BranchPrioritiesTest,
//   BranchPrioritiesTest,
//     testing::Values(branching_priorities_test_params));
class BranchPrioritiesTest
    : public ::testing::TestWithParam<BranchPrioritiesTestParams> {
 protected:
  SolverType TestedSolver() const { return GetParam().solver_type; }
  const SolveParameters& SolveParams() const { return GetParam().solve_params; }
};

// Parameters for the LazyConstraintsTest suite below.
struct LazyConstraintsTestParams {
  LazyConstraintsTestParams(SolverType solver_type,
                            SolveParameters solve_params)
      : solver_type(solver_type), nerfed_solve_params(solve_params) {}

  // The tested solver.
  SolverType solver_type;

  // Should ensure the solver behaves as close as possible to a pure
  // branch-and-bound solver (e.g., turn presolve, heuristics and cuts off).
  // Major deviations from this could cause the test to fail.
  SolveParameters nerfed_solve_params;

  friend std::ostream& operator<<(std::ostream& out,
                                  const LazyConstraintsTestParams& params);
};

// A suite of unit tests to show that an MIP solver handles lazy constraints
// correctly.
//
// To use these tests, in file <solver>_test.cc write:
//   INSTANTIATE_TEST_SUITE_P(<Solver>LazyConstraintsTest,
//   LazyConstraintsTest,
//     testing::Values(lazy_constraints_test_params));
class LazyConstraintsTest
    : public ::testing::TestWithParam<LazyConstraintsTestParams> {
 protected:
  SolverType TestedSolver() const { return GetParam().solver_type; }
  const SolveParameters& NerfedSolveParams() const {
    return GetParam().nerfed_solve_params;
  }
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVER_TESTS_IP_MODEL_SOLVE_PARAMETERS_TESTS_H_
