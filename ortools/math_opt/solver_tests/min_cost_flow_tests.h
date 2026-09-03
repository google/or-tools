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

#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_MIN_COST_FLOW_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_MIN_COST_FLOW_TESTS_H_

#include <optional>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

struct MinCostFlowTestParams {
  std::string name ABSL_REQUIRE_EXPLICIT_INIT;

  SolverType solver_type ABSL_REQUIRE_EXPLICIT_INIT;

  // Okay to leave empty, these parameters will be applied to every test except
  // the tests where we are looking for a dual ray.
  SolveParameters default_parameters;

  // When set, we get an InvalidArgumentError containing this string if the flow
  // problem is an LP instead of a flow problem, otherwise, we expect to solve
  // it as a generic LP.
  std::optional<std::string> lp_not_flow_error_substring
      ABSL_REQUIRE_EXPLICIT_INIT;

  // When set, we get an InvalidArgumentError if the flow problem is a MIP
  // instead of a flow problem, otherwise, we expect to solve it as a MIP.
  std::optional<std::string> mip_not_flow_error_substring
      ABSL_REQUIRE_EXPLICIT_INIT;

  // When set, we get InvalidArgumentError containing this string if the flow
  // problem has floating point costs, otherwise, we expect to solve it.
  std::optional<std::string> floating_point_cost_error_substring
      ABSL_REQUIRE_EXPLICIT_INIT;

  // When set, we get InvalidArgumentError containing this string if the flow
  // problem has floating point capacities, otherwise, we expect to solve it.
  std::optional<std::string> floating_point_capacity_error_substring
      ABSL_REQUIRE_EXPLICIT_INIT;

  // For the case where problem is infeasible not because of imbalanced flow,
  // but because the arc capacity is insufficient. If true, we expect a dual
  // ray, otherwise, we simply expect an infeasible termination reason.
  bool certifies_nontrivial_infeasibility ABSL_REQUIRE_EXPLICIT_INIT;
  // Used only on problems where we know the problem is infeasible and we want
  // to see if we can get a dual ray. Leave blank if dual rays are not
  // supported.
  math_opt::SolveParameters request_dual_rays_params;

  // If true, on every successful solve, we also expect a dual feasible
  // solution.
  bool returns_dual_solution ABSL_REQUIRE_EXPLICIT_INIT;
};

std::ostream& operator<<(std::ostream& out,
                         const MinCostFlowTestParams& params);

class MinCostFlowTest : public testing::TestWithParam<MinCostFlowTestParams> {};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_MIN_COST_FLOW_TESTS_H_
