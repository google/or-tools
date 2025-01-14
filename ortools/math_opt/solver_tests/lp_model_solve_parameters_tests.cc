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

#include "ortools/math_opt/solver_tests/lp_model_solve_parameters_tests.h"

#include <limits>
#include <ostream>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/base_solver_test.h"
#include "ortools/math_opt/solver_tests/test_models.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {

std::ostream& operator<<(std::ostream& out,
                         const LpModelSolveParametersTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << ", exact_zeros: " << params.exact_zeros
      << ", supports_duals: " << params.supports_duals
      << ", supports_primal_only_warm_starts: "
      << params.supports_primal_only_warm_starts
      << ", parameters: " << ProtobufShortDebugString(params.parameters.Proto())
      << " }";
  return out;
}

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// A basic feasible linear program used in filtering tests below.
//
// It has an optimal solution that is unique and has the property that for
// primal values, dual values and reduced costs, it has one zero value and one
// non-zero value. This enables testing for filtering zeros and testing
// filtering with keys.
//
// The model is:
//
//   min  2 * x1 + x2
//   s.t. x1 + x2 >= 1     (constraint y1)
//        x1 + 4 * x2 >= 2 (constraint y2)
//        x1 >= 0, x2 >= 0 (non-negative variables)
//
// The solution is:
//   x1 = 0, x2 = 1
//
// The use of non-negative variables without upper-bounds and one-sided
// constraints makes it simpler to write the dual problem:
//
//   max  y1 + 2 * y2
//   s.t. y1 + y2 + r1 = 2
//        y1 + 4 * y2 + r2 = 1
//        y1 >= 0, y2 >= 0
//        r1 >= 0, r2 >= 0
//
// The solution of the dual is:
//   y1 = 1, y2 = 0
//   r1 = 1, r2 = 0
struct FeasibleLP {
  FeasibleLP()
      : x1(model.AddContinuousVariable(0, kInf, "x1")),
        x2(model.AddContinuousVariable(0, kInf, "x2")),
        y1(model.AddLinearConstraint(x1 + x2 >= 1, "y1")),
        y2(model.AddLinearConstraint(x1 + 4 * x2 >= 2, "y2")) {
    model.Minimize(2 * x1 + x2);
  }

  Model model;
  const Variable x1;
  const Variable x2;
  const LinearConstraint y1;
  const LinearConstraint y2;
};

// An unbounded linear program used in filtering tests below.
//
// The model is:
//
//   max  2 * x1 - x2
//   s.t. x1 + x2 >= 1     (constraint y1)
//        x2 <= 0          (constraint y2)
//        x1 >= 0, x2 >= 0 (non-negative variables)
//
// It will have a primal ray with a non-zero value for x1 and a zero value for
// x2.
//
// The primal ray will be proportional to:
//   R := (x1 = 1, x2 = 0)
//
// A feasible point is:
//   P := (x1 = 1, x2 = 0)
//
// For all t >=0, P + t * R will be feasible.
struct UnboundedLP {
  UnboundedLP()
      : x1(model.AddContinuousVariable(0, kInf, "x1")),
        x2(model.AddContinuousVariable(0, kInf, "x2")),
        y1(model.AddLinearConstraint(x1 + x2 >= 1, "y1")),
        y2(model.AddLinearConstraint(x2 <= 0, "y2")) {
    model.Maximize(2 * x1 - x2);
  }

  Model model;
  const Variable x1;
  const Variable x2;
  const LinearConstraint y1;
  const LinearConstraint y2;
};

// An infeasible linear program used in filtering tests below.
//
// The model is:
//
//   min  x1 - x2
//   s.t. x1 <= -2  (constraint y1)
//        x2 <= 3   (constraint y2)
//        x1 >= 0   (non-negative variable)
//
// Its dual is:
//
//   max  -2 * y1 + 3 * y2
//   s.t. y1 + r1 = 1
//        y2 + r2 = -1
//        y1 <= 0
//        y2 <= 0
//        r1 >= 0
//        r2 = 0
//
// The dual is indeed unbounded. It will have a dual ray with a non-zero value
// for y1 and a zero value for y2. It also has non-zero reduced cost for x1 and
// a zero one for x2.
//
// The dual ray will be proportional to:
//   R := (y1 = -1, y2 = 0, r1 = 1, r2 = 0)
//
// A feasible point of the dual is:
//   P := (y1 = 0, y2 = -1, r1 = 1, r2 = 0)
//
// For all t >= 0, P + t * R will be feasible.
struct InfeasibleLP {
  InfeasibleLP()
      : x1(model.AddContinuousVariable(0, kInf, "x1")),
        x2(model.AddContinuousVariable(-kInf, kInf, "x2")),
        y1(model.AddLinearConstraint(x1 <= -2, "y1")),
        y2(model.AddLinearConstraint(x2 <= 3, "y2")) {
    model.Minimize(x1 - x2);
  }

  Model model;
  const Variable x1;
  const Variable x2;
  const LinearConstraint y1;
  const LinearConstraint y2;
};

TEST_P(LpModelSolveParametersTest, SolutionFilterSkipZerosPrimalVars) {
  if (!GetParam().exact_zeros) {
    GTEST_SKIP()
        << "Solver " << GetParam().solver_type
        << " does not reliably return exact zeros; this test is disabled.";
  }
  FeasibleLP lp;

  const SolveArguments args = {
      .parameters = GetParam().parameters,
      .model_parameters = {
          .variable_values_filter = {.skip_zero_values = true}}};
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(lp.model, GetParam().solver_type, args));
  ASSERT_THAT(result, IsOptimal(1.0));
  EXPECT_THAT(result.variable_values(), IsNear({{lp.x2, 1.0}}));
  if (GetParam().supports_duals) {
    const DualSolution expected_dual = {
        .dual_values = {{lp.y1, 1.0}, {lp.y2, 0.0}},
        .reduced_costs = {{lp.x1, 1.0}, {lp.x2, 0.0}},
        .objective_value = 1.0,
        .feasibility_status = SolutionStatus::kFeasible};

    EXPECT_THAT(result, HasDualSolution(expected_dual));
  }
}

TEST_P(LpModelSolveParametersTest, SolutionFilterSkipZerosReducedCosts) {
  if (!GetParam().exact_zeros) {
    GTEST_SKIP()
        << "Solver " << GetParam().solver_type
        << " does not reliably return exact zeros; this test is disabled.";
  }
  if (!GetParam().supports_duals) {
    GTEST_SKIP() << "Solver " << GetParam().solver_type
                 << " can't produce dual solutions; this test is disabled.";
  }
  FeasibleLP lp;
  const SolveArguments args = {
      .parameters = GetParam().parameters,
      .model_parameters = {.reduced_costs_filter = {.skip_zero_values = true}}};
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(lp.model, GetParam().solver_type, args));
  ASSERT_THAT(result, IsOptimal(1.0));
  EXPECT_THAT(result.variable_values(), IsNear({{lp.x1, 0.0}, {lp.x2, 1.0}}));
  const DualSolution expected_dual = {
      .dual_values = {{lp.y1, 1.0}, {lp.y2, 0.0}},
      .reduced_costs = {{lp.x1, 1.0}},
      .objective_value = 1.0,
      .feasibility_status = SolutionStatus::kFeasible};
  EXPECT_THAT(result, HasDualSolution(expected_dual));
}

TEST_P(LpModelSolveParametersTest, SolutionFilterSkipZerosDualVars) {
  if (!GetParam().exact_zeros) {
    GTEST_SKIP()
        << "Solver " << GetParam().solver_type
        << " does not reliably return exact zeros; this test is disabled.";
  }
  if (!GetParam().supports_duals) {
    GTEST_SKIP() << "Solver " << GetParam().solver_type
                 << " can't produce dual solutions; this test is disabled.";
  }
  FeasibleLP lp;

  const SolveArguments args = {
      .parameters = GetParam().parameters,
      .model_parameters = {.dual_values_filter = {.skip_zero_values = true}}};
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(lp.model, GetParam().solver_type, args));
  ASSERT_THAT(result, IsOptimal(1.0));
  EXPECT_THAT(result.variable_values(), IsNear({{lp.x1, 0.0}, {lp.x2, 1.0}}));
  const DualSolution expected_dual = {
      .dual_values = {{lp.y1, 1.0}},
      .reduced_costs = {{lp.x1, 1.0}, {lp.x2, 0.0}},
      .objective_value = 1.0,
      .feasibility_status = SolutionStatus::kFeasible};
  EXPECT_THAT(result, HasDualSolution(expected_dual));
}

// This test is shared by for all three filters since each filter use a
// different set of keys.
TEST_P(LpModelSolveParametersTest, SolutionFilterByKey) {
  FeasibleLP lp;

  const SolveArguments args = {
      .parameters = GetParam().parameters,
      .model_parameters = {
          .variable_values_filter = MakeKeepKeysFilter({lp.x1}),
          .dual_values_filter = MakeKeepKeysFilter({lp.y2}),
          .reduced_costs_filter = MakeKeepKeysFilter({lp.x2})}};
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(lp.model, GetParam().solver_type, args));
  ASSERT_THAT(result, IsOptimal(1.0));
  EXPECT_THAT(result.variable_values(), IsNear({{lp.x1, 0.0}}));
  if (GetParam().supports_duals) {
    const DualSolution expected_dual = {
        .dual_values = {{lp.y2, 0.0}},
        .reduced_costs = {{lp.x2, 0.0}},
        .objective_value = 1.0,
        .feasibility_status = SolutionStatus::kFeasible};
    EXPECT_THAT(result, HasDualSolution(expected_dual));
  }
}

TEST_P(LpModelSolveParametersTest, SolutionFilterSkipZerosPrimalRay) {
  if (!GetParam().exact_zeros) {
    GTEST_SKIP()
        << "Solver " << GetParam().solver_type
        << " does not reliably return exact zeros; this test is disabled.";
  }
  UnboundedLP lp;

  SolveArguments args = {.parameters = GetParam().parameters,
                         .model_parameters = {.variable_values_filter = {
                                                  .skip_zero_values = true}}};

  if (!ActivatePrimalRay(GetParam().solver_type, args.parameters)) {
    GTEST_SKIP() << "Solver " << GetParam().solver_type
                 << " can't produce primal rays; this test is disabled.";
  }

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(lp.model, GetParam().solver_type, args));
  ASSERT_THAT(result,
              TerminatesWithOneOf({TerminationReason::kUnbounded,
                                   TerminationReason::kInfeasibleOrUnbounded}));
  EXPECT_THAT(result, HasPrimalRay({{lp.x1, 1.0}}));
}

TEST_P(LpModelSolveParametersTest, SolutionFilterByKeyPrimalRay) {
  UnboundedLP lp;

  SolveArguments args = {.parameters = GetParam().parameters,
                         .model_parameters = {.variable_values_filter =
                                                  MakeKeepKeysFilter({lp.x2})}};

  if (!ActivatePrimalRay(GetParam().solver_type, args.parameters)) {
    GTEST_SKIP() << "Solver " << GetParam().solver_type
                 << " can't produce primal rays; this test is disabled.";
  }

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(lp.model, GetParam().solver_type, args));
  ASSERT_THAT(result,
              TerminatesWithOneOf({TerminationReason::kUnbounded,
                                   TerminationReason::kInfeasibleOrUnbounded}));
  EXPECT_THAT(result, HasPrimalRay({{lp.x2, 0.0}}));
}

TEST_P(LpModelSolveParametersTest, SolutionFilterSkipZerosDualRayDuals) {
  if (!GetParam().exact_zeros) {
    GTEST_SKIP()
        << "Solver " << GetParam().solver_type
        << " does not reliably return exact zeros; this test is disabled.";
  }
  InfeasibleLP lp;

  SolveArguments args = {
      .parameters = GetParam().parameters,
      .model_parameters = {.dual_values_filter = {.skip_zero_values = true}}};

  if (!ActivateDualRay(GetParam().solver_type, args.parameters)) {
    GTEST_SKIP() << "Solver " << GetParam().solver_type
                 << " can't produce dual rays; this test is disabled.";
  }

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(lp.model, GetParam().solver_type, args));
  ASSERT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
  const DualRay expected = {.dual_values = {{lp.y1, -1.0}},
                            .reduced_costs = {{lp.x1, 1.0}, {lp.x2, 0.0}}};
  EXPECT_THAT(result, HasDualRay(expected));
}

TEST_P(LpModelSolveParametersTest, SolutionFilterSkipZerosDualRayReducedCosts) {
  if (!GetParam().exact_zeros) {
    GTEST_SKIP()
        << "Solver " << GetParam().solver_type
        << " does not reliably return exact zeros; this test is disabled.";
  }
  InfeasibleLP lp;

  SolveArguments args = {
      .parameters = GetParam().parameters,
      .model_parameters = {.reduced_costs_filter = {.skip_zero_values = true}}};

  if (!ActivateDualRay(GetParam().solver_type, args.parameters)) {
    GTEST_SKIP() << "Solver " << GetParam().solver_type
                 << " can't produce dual rays; this test is disabled.";
  }

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(lp.model, GetParam().solver_type, args));
  ASSERT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
  const DualRay expected = {.dual_values = {{lp.y1, -1.0}, {lp.y2, 0.0}},
                            .reduced_costs = {{lp.x1, 1.0}}};
  EXPECT_THAT(result, HasDualRay(expected));
}

TEST_P(LpModelSolveParametersTest, SolutionFilterByKeysDualRay) {
  InfeasibleLP lp;

  SolveArguments args = {
      .parameters = GetParam().parameters,
      .model_parameters = {
          .dual_values_filter = MakeKeepKeysFilter({lp.y2}),
          .reduced_costs_filter = MakeKeepKeysFilter({lp.x2})}};

  if (!ActivateDualRay(GetParam().solver_type, args.parameters)) {
    GTEST_SKIP() << "Solver " << GetParam().solver_type
                 << " can't produce dual rays; this test is disabled.";
  }

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(lp.model, GetParam().solver_type, args));
  ASSERT_THAT(result, TerminatesWith(TerminationReason::kInfeasible));
  const DualRay expected = {.dual_values = {{lp.y2, 0.0}},
                            .reduced_costs = {{lp.x2, 0.0}}};
  EXPECT_THAT(result, HasDualRay(expected));
}

TEST_P(LpModelSolveParametersTest, PrimalWarmStart) {
  constexpr int n = 10;
  const auto model = IndependentSetCompleteGraph(/*integer=*/false, /*n=*/n);
  int baseline_num_iters = 0;
  {
    ASSERT_OK_AND_ASSIGN(const auto result,
                         Solve(*model, GetParam().solver_type,
                               {.parameters = GetParam().parameters}));
    ASSERT_THAT(result, IsOptimal(n / 2.0));
    baseline_num_iters = result.solve_stats.simplex_iterations +
                         result.solve_stats.barrier_iterations +
                         result.solve_stats.first_order_iterations;
  }

  // We seed the optimal primal solution as a warm start.
  ModelSolveParameters::SolutionHint warm_start;
  for (const Variable var : model->Variables()) {
    warm_start.variable_values[var] = 1.0 / 2.0;
  }
  ASSERT_OK_AND_ASSIGN(
      const auto result,
      Solve(*model, GetParam().solver_type,
            {.parameters = GetParam().parameters,
             .model_parameters = {.solution_hints = {warm_start}}}));
  EXPECT_THAT(result, IsOptimal(n / 2.0));
  const int actual_num_iters = result.solve_stats.simplex_iterations +
                               result.solve_stats.barrier_iterations +
                               result.solve_stats.first_order_iterations;
  if (!GetParam().supports_primal_only_warm_starts) {
    EXPECT_EQ(actual_num_iters, baseline_num_iters);
    return;
  }
  EXPECT_LT(actual_num_iters, baseline_num_iters);
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
