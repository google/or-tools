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

#include "ortools/math_opt/solver_tests/min_cost_flow_tests.h"

#include <array>
#include <cmath>
#include <limits>
#include <ostream>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/port/proto_utils.h"

namespace operations_research::math_opt {

constexpr double kInf = std::numeric_limits<double>::infinity();

std::ostream& operator<<(std::ostream& out,
                         const MinCostFlowTestParams& params) {
  out << "name: " << params.name << " solver_type: " << params.solver_type
      << " default_parameters: "
      << ProtobufShortDebugString(params.default_parameters.Proto());

  if (params.lp_not_flow_error_substring.has_value()) {
    out << " lp_not_flow_error_substring: '"
        << absl::CEscape(*params.lp_not_flow_error_substring) << "'";
  }

  if (params.mip_not_flow_error_substring.has_value()) {
    out << " mip_not_flow_error_substring: '"
        << absl::CEscape(*params.mip_not_flow_error_substring) << "'";
  }

  if (params.floating_point_cost_error_substring.has_value()) {
    out << " floating_point_cost_error_substring: '"
        << absl::CEscape(*params.floating_point_cost_error_substring) << "'";
  }

  if (params.floating_point_capacity_error_substring.has_value()) {
    out << " floating_point_capacity_error_substring: '"
        << absl::CEscape(*params.floating_point_capacity_error_substring)
        << "'";
  }
  out << " certifies_nontrivial_infeasibility "
      << params.certifies_nontrivial_infeasibility
      << " request_dual_rays_params: "
      << ProtobufShortDebugString(params.request_dual_rays_params.Proto())
      << " returns_dual_solution: " << params.returns_dual_solution;
  return out;
}

namespace {

using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

TEST_P(MinCostFlowTest, MinimizeEmptyModel) {
  Model model;
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(IsOptimalWithSolution(0.0, {})));
}

TEST_P(MinCostFlowTest, MaximizeEmptyModel) {
  Model model;
  model.set_maximize();
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(IsOptimalWithSolution(0.0, {})));
}

TEST_P(MinCostFlowTest, MinimizeOffset) {
  Model model;
  model.Minimize(3.0);
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(IsOptimalWithSolution(3.0, {})));
}

TEST_P(MinCostFlowTest, MaximizeOffset) {
  Model model;
  model.Maximize(3.0);
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(IsOptimalWithSolution(3.0, {})));
}

TEST_P(MinCostFlowTest, MinimalOptimal) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  model.AddLinearConstraint(st == 2.0);    // flow out of s
  model.AddLinearConstraint(-st == -2.0);  // flow into t
  model.Minimize(3.0 * st);
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(IsOptimalWithSolution(6.0, {{st, 2.0}})));
}

TEST_P(MinCostFlowTest, MinimalInfeasible) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 1.0);
  model.AddLinearConstraint(st == 2.0);    // flow out of s
  model.AddLinearConstraint(-st == -2.0);  // flow into t
  model.Minimize(3.0 * st);
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(TerminatesWithOneOf(
                  {TerminationReason::kInfeasible,
                   TerminationReason::kInfeasibleOrUnbounded})));
}

TEST_P(MinCostFlowTest, InfiniteCapacity) {
  Model model;
  const Variable st =
      model.AddContinuousVariable(0.0, std::numeric_limits<double>::infinity());
  model.AddLinearConstraint(st == 2.0);    // flow out of s
  model.AddLinearConstraint(-st == -2.0);  // flow into t
  model.Minimize(3.0 * st);
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(IsOptimalWithSolution(6.0, {{st, 2.0}})));
}

TEST_P(MinCostFlowTest, HasOffset) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  model.AddLinearConstraint(st == 2.0);    // flow out of s
  model.AddLinearConstraint(-st == -2.0);  // flow into t
  model.Minimize(3.0 * st + 11.0);
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(IsOptimalWithSolution(17.0, {{st, 2.0}})));
}

TEST_P(MinCostFlowTest, ObjectiveDirection) {
  for (const bool is_maximize : {false, true}) {
    SCOPED_TRACE(absl::StrCat("is_maximize: ", is_maximize));
    Model model;
    const Variable st1 = model.AddContinuousVariable(0.0, kInf);
    const Variable st2 = model.AddContinuousVariable(0.0, kInf);
    model.AddLinearConstraint(st1 + st2 == 2.0);    // flow out of s
    model.AddLinearConstraint(-st1 - st2 == -2.0);  // flow into t
    model.set_is_maximize(is_maximize);
    model.set_objective_coefficient(st1, 3.0);
    model.set_objective_coefficient(st2, 4.0);

    double expected_obj;
    VariableMap<double> expected_solution;
    if (is_maximize) {
      expected_obj = 8.0;
      expected_solution = {{st1, 0.0}, {st2, 2.0}};
    } else {
      expected_obj = 6.0;
      expected_solution = {{st1, 2.0}, {st2, 0.0}};
    }
    EXPECT_THAT(
        Solve(model, GetParam().solver_type,
              {.parameters = GetParam().default_parameters}),
        IsOkAndHolds(IsOptimalWithSolution(expected_obj, expected_solution)));
  }
}

TEST_P(MinCostFlowTest, Unbalanced) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, kInf);
  model.AddLinearConstraint(st == 2.0);    // flow out of s
  model.AddLinearConstraint(-st == -3.0);  // flow into t
  model.Minimize(st);
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(TerminatesWithOneOf(
                  {TerminationReason::kInfeasible,
                   TerminationReason::kInfeasibleOrUnbounded})));
}

TEST_P(MinCostFlowTest, LpNotFlowMissingDest) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  model.AddLinearConstraint(st == 0.0);  // flow out of s
  model.Minimize(3.0 * st);
  if (GetParam().lp_not_flow_error_substring.has_value()) {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(*GetParam().lp_not_flow_error_substring)));
  } else {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                IsOkAndHolds(IsOptimalWithSolution(0.0, {{st, 0.0}})));
  }
}

TEST_P(MinCostFlowTest, LpNotFlowMissingSource) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  model.AddLinearConstraint(-st == 0.0);  // flow into t
  model.Minimize(3.0 * st);
  if (GetParam().lp_not_flow_error_substring.has_value()) {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(*GetParam().lp_not_flow_error_substring)));
  } else {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                IsOkAndHolds(IsOptimalWithSolution(0.0, {{st, 0.0}})));
  }
}

TEST_P(MinCostFlowTest, LpNotFlowMultipleSources) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  model.AddLinearConstraint(st == 0.0);   // flow out of s
  model.AddLinearConstraint(-st == 0.0);  // flow into t
  model.AddLinearConstraint(st == 0.0);   // extra source
  model.Minimize(3.0 * st);
  if (GetParam().lp_not_flow_error_substring.has_value()) {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(*GetParam().lp_not_flow_error_substring)));
  } else {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                IsOkAndHolds(IsOptimalWithSolution(0.0, {{st, 0.0}})));
  }
}

TEST_P(MinCostFlowTest, LpNotFlowMultipleDestinations) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  model.AddLinearConstraint(st == 0.0);   // flow out of s
  model.AddLinearConstraint(-st == 0.0);  // flow into t
  model.AddLinearConstraint(-st == 0.0);  // extra dest
  model.Minimize(3.0 * st);
  if (GetParam().lp_not_flow_error_substring.has_value()) {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(*GetParam().lp_not_flow_error_substring)));
  } else {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                IsOkAndHolds(IsOptimalWithSolution(0.0, {{st, 0.0}})));
  }
}

TEST_P(MinCostFlowTest, LpNotFlowAbsCoefficientsNotOne) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  model.AddLinearConstraint(2.0 * st == 0.0);   // flow out of s
  model.AddLinearConstraint(-3.0 * st == 0.0);  // flow into t
  model.Minimize(3.0 * st);
  if (GetParam().lp_not_flow_error_substring.has_value()) {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(*GetParam().lp_not_flow_error_substring)));
  } else {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                IsOkAndHolds(IsOptimalWithSolution(0.0, {{st, 0.0}})));
  }
}

TEST_P(MinCostFlowTest, LpNotFlowConstraintAtleast) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  model.AddLinearConstraint(st >= 0.0);  // flow out of s
  model.AddLinearConstraint(st == 0.0);  // flow into t
  model.Minimize(3.0 * st);
  if (GetParam().lp_not_flow_error_substring.has_value()) {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(*GetParam().lp_not_flow_error_substring)));
  } else {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                IsOkAndHolds(IsOptimalWithSolution(0.0, {{st, 0.0}})));
  }
}

TEST_P(MinCostFlowTest, LpNotFlowConstraintAtMost) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  model.AddLinearConstraint(st == 0.0);  // flow out of s
  model.AddLinearConstraint(st <= 0.0);  // flow into t
  model.Minimize(3.0 * st);
  if (GetParam().lp_not_flow_error_substring.has_value()) {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(*GetParam().lp_not_flow_error_substring)));
  } else {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                IsOkAndHolds(IsOptimalWithSolution(0.0, {{st, 0.0}})));
  }
}

TEST_P(MinCostFlowTest, LpNotFlowConstraintRange) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  model.AddLinearConstraint(st == 0.0);          // flow out of s
  model.AddLinearConstraint(-4.0 <= st <= 0.0);  // flow into t
  model.Minimize(3.0 * st);
  if (GetParam().lp_not_flow_error_substring.has_value()) {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(*GetParam().lp_not_flow_error_substring)));
  } else {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                IsOkAndHolds(IsOptimalWithSolution(0.0, {{st, 0.0}})));
  }
}

TEST_P(MinCostFlowTest, LpNotFlowVariableLbNotZero) {
  Model model;
  const Variable st = model.AddContinuousVariable(2.0, 5.0);
  model.AddLinearConstraint(st == 2.0);  // flow out of s
  model.AddLinearConstraint(st == 2.0);  // flow into t
  model.Minimize(3.0 * st);
  if (GetParam().lp_not_flow_error_substring.has_value()) {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(*GetParam().lp_not_flow_error_substring)));
  } else {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                IsOkAndHolds(IsOptimalWithSolution(6.0, {{st, 2.0}})));
  }
}

TEST_P(MinCostFlowTest, MipNotFlow) {
  Model model;
  const Variable st = model.AddIntegerVariable(0.0, 5.0);
  model.AddLinearConstraint(st == 2.0);    // flow out of s
  model.AddLinearConstraint(-st == -2.0);  // flow into t
  model.Minimize(3.0 * st);
  if (GetParam().mip_not_flow_error_substring.has_value()) {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(*GetParam().mip_not_flow_error_substring)));
  } else {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                IsOkAndHolds(IsOptimalWithSolution(6.0, {{st, 2.0}})));
  }
}

TEST_P(MinCostFlowTest, FloatingPointCost) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  model.AddLinearConstraint(st == 2.0);    // flow out of s
  model.AddLinearConstraint(-st == -2.0);  // flow into t
  model.Minimize(3.2 * st);
  if (!GetParam().floating_point_cost_error_substring.has_value()) {
    EXPECT_THAT(Solve(model, GetParam().solver_type,
                      {.parameters = GetParam().default_parameters}),
                IsOkAndHolds(IsOptimalWithSolution(6.4, {{st, 2.0}})));
  } else {
    EXPECT_THAT(
        Solve(model, GetParam().solver_type,
              {.parameters = GetParam().default_parameters}),
        StatusIs(absl::StatusCode::kInvalidArgument,
                 HasSubstr(*GetParam().floating_point_cost_error_substring)));
  }
}

TEST_P(MinCostFlowTest, FloatingPointCapacity) {
  Model model;
  const Variable st1 = model.AddContinuousVariable(0.0, 0.5);
  const Variable st2 = model.AddContinuousVariable(0.0, 1.5);
  model.AddLinearConstraint(st1 + st2 == 2.0);    // flow out of s
  model.AddLinearConstraint(-st1 - st2 == -2.0);  // flow into t
  model.Minimize(3 * st1 + 3 * st2);
  if (!GetParam().floating_point_capacity_error_substring.has_value()) {
    EXPECT_THAT(
        Solve(model, GetParam().solver_type,
              {.parameters = GetParam().default_parameters}),
        IsOkAndHolds(IsOptimalWithSolution(6.0, {{st1, 0.5}, {st2, 1.5}})));
  } else {
    EXPECT_THAT(
        Solve(model, GetParam().solver_type,
              {.parameters = GetParam().default_parameters}),
        StatusIs(
            absl::StatusCode::kInvalidArgument,
            HasSubstr(*GetParam().floating_point_capacity_error_substring)));
  }
}

// Primal:
// min   3st
// s.t    st = 2          (s)
//       -st = -2         (t)
//        st in [0,1]     (r)
//
// Dual:
// max   2s - 2t + r
// s.t.  s - t + r <= 3
//       r <= 0
//
// Has the basic dual rays:
//   * (s, t, r) = (1, 0, -1).
//   * (s, t, r) = (0, -1, -1)
// (Start from (0, 0, 0), add any multiple of these rays and we maintain
// feasibility and increase the objective.)
TEST_P(MinCostFlowTest, CertifiesInfeasibility) {
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 1.0);
  math_opt::LinearConstraint s_balance =
      model.AddLinearConstraint(st == 2.0);  // flow out of s
  math_opt::LinearConstraint t_balance =
      model.AddLinearConstraint(-st == -2.0);  // flow into t
  model.Minimize(3.0 * st);
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, GetParam().solver_type,
            {.parameters = GetParam().request_dual_rays_params}));
  EXPECT_THAT(result,
              TerminatesWithOneOf({TerminationReason::kInfeasible,
                                   TerminationReason::kInfeasibleOrUnbounded}));
  if (GetParam().certifies_nontrivial_infeasibility) {
    // We need to check that (s, t, r) is a convex combination of (1, 0, -1) and
    // (0, -1, -1). Note that solvers which do not return basic solutions (e.g.
    // PDLP can and will give us a convex combination.
    //
    // Strategy:
    //   1. check that r < -esp, t <= 0, s >= 0
    //   2. check that  |s| + |t| ~= |r|
    //
    // NOTE: it would be better if we had a test where dual ray was unique (up
    // to scaling).
    ASSERT_TRUE(result.has_dual_ray());
    const DualRay& ray = result.dual_rays[0];
    ASSERT_EQ(ray.dual_values.size(), 2);
    ASSERT_TRUE(ray.dual_values.contains(s_balance));
    ASSERT_TRUE(ray.dual_values.contains(t_balance));
    ASSERT_EQ(ray.reduced_costs.size(), 1);
    ASSERT_TRUE(ray.reduced_costs.contains(st));
    // Check that t < 0, no obvious minimum separation...
    ASSERT_LE(ray.reduced_costs.at(st), -1e-1);
    // Check that s ~=> 0.
    ASSERT_GE(ray.dual_values.at(s_balance), -1.0e-6);
    // Check that t ~<= 0.
    ASSERT_LE(ray.dual_values.at(t_balance), 1.0e-6);
    const double lhs = std::abs(ray.dual_values.at(s_balance)) +
                       std::abs(ray.dual_values.at(t_balance));
    const double rhs = std::abs(ray.reduced_costs.at(st));
    // check that lhs ~= rhs, up to a relative error. Note that we know that rhs
    // is nonzero already.
    EXPECT_NEAR(lhs / rhs, 1.0, 1e-4);
  }
}

// Primal:
// min   3st
// s.t    st = 2          (s)
//       -st = -2         (t)
//        st in [0,5]     (r)
//
// Dual:
// max   2s - 2t + 5r
// s.t.  s - t + r <= 3
//       r <= 0
//
// Optimal objective value: 6.0
// Primal solution: st = 2
// The problem has one dual basic optimal solution:
//   (s, t, r) = (0, -3, 0)
// And a ray along which all solution are optimal:
//   ray = (1, 1, 0)
// I.e., for all a >= 0, we have that
//   (s, t, r) = (a, a-3, 0)
// is optimal.
TEST_P(MinCostFlowTest, DualSolution) {
  if (!GetParam().returns_dual_solution) {
    return;
  }
  Model model;
  const Variable st = model.AddContinuousVariable(0.0, 5.0);
  math_opt::LinearConstraint s_balance =
      model.AddLinearConstraint(st == 2.0);  // flow out of s
  math_opt::LinearConstraint t_balance =
      model.AddLinearConstraint(-st == -2.0);  // flow into t
  model.Minimize(3.0 * st);
  VariableMap<double> expected_reduced_costs = {{st, 0.0}};
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type,
                             {.parameters = GetParam().default_parameters}));
  ASSERT_THAT(result, IsOptimal(6.0));
  ASSERT_TRUE(result.has_dual_feasible_solution());
  EXPECT_THAT(result.reduced_costs(), IsNear(expected_reduced_costs));
  std::vector<math_opt::LinearConstraint> dual_keys;
  for (const auto& [dual_key, value] : result.dual_values()) {
    dual_keys.push_back(dual_key);
  }
  ASSERT_THAT(dual_keys, UnorderedElementsAre(s_balance, t_balance));
  double s_val = result.dual_values().at(s_balance);
  double t_val = result.dual_values().at(t_balance);
  LOG(INFO) << "s_dual: " << s_val << ", t_dual: " << t_val;
  // Fix uniqueness, subtract off s_val from both
  t_val -= s_val;
  EXPECT_NEAR(t_val, -3.0, 1.0e-6);
}

// A valid Min-Cost Flow problem formulated as an LP.
//
// Nodes: A (supply=20), B (supply=-5), C (supply=-15)
// Arcs:
//   A->B (capacity=15, cost=2)
//   B->C (capacity=10, cost=3)
//   A->C (capacity=20, cost=10)
TEST_P(MinCostFlowTest, ValidMinCostFlow) {
  Model model;

  // Create arcs as variables.
  const Variable ab = model.AddContinuousVariable(0.0, 15.0, "ab");
  const Variable bc = model.AddContinuousVariable(0.0, 10.0, "bc");
  const Variable ac = model.AddContinuousVariable(0.0, 20.0, "ac");

  // Create flow conservation constraints: outflow - inflow = supply
  model.AddLinearConstraint(ab + ac == 20.0, "NodeA");
  model.AddLinearConstraint(bc - ab == -5.0, "NodeB");
  model.AddLinearConstraint(-bc - ac == -15.0, "NodeC");

  // Objective: Minimize 2*ab + 3*bc + 10*ac
  model.Minimize(2.0 * ab + 3.0 * bc + 10.0 * ac);

  // Optimal flow:
  // A must send 20 units of flow.
  // The path A->B->C costs 2+3=5 per unit, which is cheaper than A->C (cost
  // 10). A can send 15 units via A->B (saturating A->B capacity). B consumes 5
  // units (supply -5 = demand 5). B forwards the remaining 10 units to C via
  // B->C, saturating B->C capacity.
  // A must send 5 more units. It must send them via A->C. C receives 10 units
  // from B and 5 units from A, satisfying its demand of 15.
  // Costs:
  //   ab: 15 units *  2 = 30
  //   bc: 10 units *  3 = 30
  //   ac:  5 units * 10 = 50
  // Total cost: 110
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(IsOptimalWithSolution(
                  110.0, {{ab, 15.0}, {bc, 10.0}, {ac, 5.0}}, 1e-4)));
}

// A valid Max-Cost Flow problem formulated as an LP.
//
// Nodes: A (supply=20), B (supply=-5), C (supply=-15)
// Arcs:
//   A->B (capacity=15, cost=2)
//   B->C (capacity=10, cost=3)
//   A->C (capacity=20, cost=10)
TEST_P(MinCostFlowTest, ValidMaxCostFlow) {
  Model model;

  const Variable ab = model.AddContinuousVariable(0.0, 15.0, "ab");
  const Variable bc = model.AddContinuousVariable(0.0, 10.0, "bc");
  const Variable ac = model.AddContinuousVariable(0.0, 20.0, "ac");

  model.AddLinearConstraint(ab + ac == 20.0, "NodeA");
  model.AddLinearConstraint(bc - ab == -5.0, "NodeB");
  model.AddLinearConstraint(-bc - ac == -15.0, "NodeC");

  // Objective: Maximize 2*ab + 3*bc + 10*ac
  model.Maximize(2.0 * ab + 3.0 * bc + 10.0 * ac);

  // Optimal flow:
  // A must send 20 units of flow. The path A->C has the highest cost (or
  // profit) of 10 per unit, so we send 15 units via A->C, saturating C's demand
  // of 15.
  // A must send 5 more units. It sends them via A->B. B consumes all 5 units
  // (supply -5 = demand 5). We don't send any flow via B->C.
  // Costs:
  //   ab:  5 units *  2 =  10
  //   bc:  0 units *  3 =   0
  //   ac: 15 units * 10 = 150
  // Total cost: 160
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(IsOptimalWithSolution(
                  160.0, {{ab, 5.0}, {bc, 0.0}, {ac, 15.0}}, 1e-3)));
}

// This test is taken from
// ortools/graph/min_cost_flow_test.cc
TEST_P(MinCostFlowTest, FeasibleProblem) {
  Model model;

  constexpr std::array kNodeSupply = {20.0,  10.0,  25.0, -11.0,
                                      -13.0, -17.0, -14.0};
  constexpr std::array kSrc = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2};
  constexpr std::array kDst = {3, 4, 5, 6, 3, 4, 5, 6, 3, 4, 5, 6};
  constexpr std::array kCost = {0.0, 0.0, 1.0, 1.0, 1.0, 1.0,
                                0.0, 1.0, 0.0, 1.0, 0.0, 0.0};
  constexpr std::array kCapacity = {100.0, 100.0, 100.0, 100.0, 100.0, 100.0,
                                    100.0, 100.0, 100.0, 100.0, 100.0, 100.0};
  const double kExpectedFlowCost = 0.0;
  constexpr std::array kExpectedFlow = {7.0,  13.0, 0.0, 0.0, 0.0, 0.0,
                                        10.0, 0.0,  4.0, 0.0, 7.0, 14.0};

  std::vector<LinearConstraint> nodes;
  for (int i = 0; i < kNodeSupply.size(); ++i) {
    nodes.push_back(model.AddLinearConstraint(kNodeSupply[i], kNodeSupply[i]));
  }

  std::vector<Variable> vars;
  for (int arc = 0; arc < kSrc.size(); ++arc) {
    const Variable v = model.AddContinuousVariable(0.0, kCapacity[arc]);
    vars.push_back(v);
    model.set_coefficient(nodes[kSrc[arc]], v, 1.0);
    model.set_coefficient(nodes[kDst[arc]], v, -1.0);
  }
  model.Minimize(InnerProduct(vars, kCost));

  VariableMap<double> expected_values;
  for (int i = 0; i < kExpectedFlow.size(); ++i) {
    expected_values[vars[i]] = kExpectedFlow[i];
  }

  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(IsOptimalWithSolution(kExpectedFlowCost,
                                                 expected_values, 1e-3)));
}

// This test is taken from
// ortools/graph/min_cost_flow_test.cc
TEST_P(MinCostFlowTest, InfeasibleProblem) {
  Model model;

  constexpr std::array kNodeSupply = {20.0,  10.0,  25.0, -11.0,
                                      -13.0, -17.0, -14.0};
  constexpr std::array kSrc = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2};
  constexpr std::array kDst = {3, 4, 5, 6, 3, 4, 5, 6, 3, 4, 5, 6};
  constexpr std::array kCost = {0.0, 0.0, 1.0, 1.0, 1.0, 1.0,
                                0.0, 1.0, 0.0, 1.0, 0.0, 0.0};

  std::vector<LinearConstraint> nodes;
  for (int i = 0; i < kNodeSupply.size(); ++i) {
    nodes.push_back(model.AddLinearConstraint(kNodeSupply[i], kNodeSupply[i]));
  }

  std::vector<Variable> vars;
  for (int arc = 0; arc < kSrc.size(); ++arc) {
    const Variable v = model.AddContinuousVariable(0.0, 1.0);
    vars.push_back(v);
    model.set_coefficient(nodes[kSrc[arc]], v, 1.0);
    model.set_coefficient(nodes[kDst[arc]], v, -1.0);
  }
  model.Minimize(InnerProduct(vars, kCost));

  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = GetParam().default_parameters}),
              IsOkAndHolds(TerminatesWithOneOf(
                  {TerminationReason::kInfeasible,
                   TerminationReason::kInfeasibleOrUnbounded})));
}

}  // namespace
}  // namespace operations_research::math_opt
