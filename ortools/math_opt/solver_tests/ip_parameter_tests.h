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

#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_IP_PARAMETER_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_IP_PARAMETER_TESTS_H_

#include <iosfwd>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace math_opt {

// Unless otherwise noted, each field indicates if setting an analogous field in
// SolveParameters is supported.
//
// Note that "supported" may be context dependent, i.e. a parameter might be
// supported for LP but not MIP with the same solver.
//
// Implementation note: keep parameters in the order they appear in
// SolveParameters.
struct ParameterSupport {
  bool supports_iteration_limit = false;
  bool supports_node_limit = false;
  bool supports_cutoff = false;
  bool supports_objective_limit = false;
  bool supports_bound_limit = false;

  // Indicates if setting solution_limit with value 1 is supported.
  bool supports_solution_limit_one = false;

  // Supports setting threads = 1 (all but HiGHS support this).
  bool supports_one_thread = false;

  // Supports setting threads to an arbitrary value.
  bool supports_n_threads = false;

  bool supports_random_seed = false;
  bool supports_absolute_gap_tolerance = false;
  bool supports_lp_algorithm_simplex = false;
  bool supports_lp_algorithm_barrier = false;
  bool supports_presolve = false;
  bool supports_cuts = false;
  bool supports_heuristics = false;
  bool supports_scaling = false;
};

std::ostream& operator<<(std::ostream& ostr,
                         const ParameterSupport& param_support);

// Indicates what data will be present in a SolveResult.
//
// Like ParameterSupport above, what data is "supported" by a solver may be
// context dependent, i.e. a statistic might be supported for LP but not MIP
// with the same solver.
struct SolveResultSupport {
  // When the solve terminates from reaching a limit, if the specific limit
  // reached is reported in Termination.
  //
  // This is very coarse, if we have solvers that report some limits but not
  // others we may want to make this more granular for better testing.
  bool termination_limit = false;
  // If SolveStats reports iteration stats for LP/IPM/FOM.
  bool iteration_stats = false;
  // If SolveStats reports the node count.
  bool node_count = false;
};

std::ostream& operator<<(std::ostream& ostr,
                         const SolveResultSupport& solve_result_support);

struct IpParameterTestParameters {
  // Used as a suffix for the gUnit test name in parametric tests, use with the
  // ParamName functor.
  std::string name;

  // The tested solver.
  SolverType solver_type;

  ParameterSupport parameter_support;
  bool hint_supported = false;
  SolveResultSupport solve_result_support;

  // Contains a regexp found in the solver logs when presolve is enabled and
  // the problem is completely solved in presolve, AND that is not found in
  // the solver logs when presolve is disabled for the same model.
  //
  // It uses testing::ContainsRegex().
  std::string presolved_regexp;

  // Parameters to try and get the solver to stop early without completely
  // solving the problem.
  SolveParameters stop_before_optimal;
};

std::ostream& operator<<(std::ostream& ostr,
                         const IpParameterTestParameters& params);

// A suite of unit tests to show that an IP solver handles parameters correctly.
//
// To use these tests, in file <solver>_test.cc write:
//   INSTANTIATE_TEST_SUITE_P(<Solver>IpParameterTest, IpParameterTest,
//     testing::Values(IpParameterTestParameters(
//       SolverType::k<Solver>,
//       /*supports_iteration_stats=*/true,
//       /*presolved_string=*/"presolved problem has 0 variables")));
class IpParameterTest
    : public ::testing::TestWithParam<IpParameterTestParameters> {
 public:
  SolverType TestedSolver() const { return GetParam().solver_type; }
};

struct LargeInstanceTestParams {
  // Used as a suffix for the gUnit test name in parametric tests, use with the
  // ParamName functor.
  std::string name;

  // The tested solver.
  SolverType solver_type;

  // Note: the test will further customize these.
  SolveParameters base_parameters;

  ParameterSupport parameter_support;

  // When we stop from hitting a limit, if the solver returns which limit was
  // the cause in the Termination object. This parameter is very coarse, make
  // it more specific if we have solvers that can return the limit some of the
  // time.
  bool allow_limit_undetermined = false;
};

std::ostream& operator<<(std::ostream& out,
                         const LargeInstanceTestParams& params);

// Tests MIP parameters on the MIPLIB instance 23588, which has optimal solution
// 8090 and LP relaxation of 7649.87. This instance was selected because every
// supported solver can solve it quickly (a few seconds), but no solver can
// solve it in one node (so we can test node limit) or too quickly (so we can
// test time limit).
//
// The cut test uses beavma instead of 23588 (this made the test less brittle,
// see cl/581963920 for details).
class LargeInstanceIpParameterTest
    : public ::testing::TestWithParam<LargeInstanceTestParams> {
 protected:
  absl::StatusOr<std::unique_ptr<Model>> Load23588();
  absl::StatusOr<std::unique_ptr<Model>> LoadBeavma();

  static constexpr double kOptimalObjective = 8090.0;
  // Computed with the command:
  //  blaze-bin/ortools/math_opt/tools/mathopt_solve \
  //   --input_file \
  //   operations_research_data/MIP_MIPLIB/miplib2017/23588.mps.gz \
  //   --solver_type glop --solver_logs --lp_relaxation
  static constexpr double kLpRelaxationObjective = 7649.87;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_IP_PARAMETER_TESTS_H_
