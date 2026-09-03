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

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/types.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/callback_tests.h"
#include "ortools/math_opt/solver_tests/generic_tests.h"
#include "ortools/math_opt/solver_tests/invalid_input_tests.h"
#include "ortools/math_opt/solver_tests/min_cost_flow_tests.h"
#include "ortools/math_opt/solver_tests/test_models.h"
#include "ortools/math_opt/testing/param_name.h"

namespace operations_research::math_opt {
namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

// Tolerance is 1e-6 as defined in min_cost_flow_solver.cc.
constexpr double kMinCostFlowIntegralityTolerance = 1e-6;

// Large floating point numbers for testing corner cases of converting to
// int64_t for cost values and flow quantities.
const double kLargeValidNumber = std::pow(2.0, 62.0);
const double kLargeInvalidNumber = std::pow(2.0, 63.0);

static_assert(std::is_same_v<SimpleMinCostFlow::CostValue, int64_t>,
              "CostValue must be int64_t for these tests");
static_assert(std::is_same_v<SimpleMinCostFlow::FlowQuantity, int64_t>,
              "FlowQuantity must be int64_t for these tests");

// Generic tests.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SimpleLogicalConstraintTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IncrementalLogicalConstraintTest);

INSTANTIATE_TEST_SUITE_P(
    MinCostFlowGenericTest, GenericTest,
    testing::Values(GenericTestParameters(SolverType::kMinCostFlow,
                                          /*support_interrupter=*/false,
                                          TestModelClass::kMinCostFlow,
                                          /*expected_log=*/"",
                                          /*solve_parameters=*/{})));

// MinCostFlow model tests.
INSTANTIATE_TEST_SUITE_P(
    MinCostFlowSolverMinCostFlowTest, MinCostFlowTest,
    testing::Values(MinCostFlowTestParams{
        .name = "min_cost_flow",
        .solver_type = math_opt::SolverType::kMinCostFlow,
        .lp_not_flow_error_substring =
            "model structure is not supported by MinCostFlow solver",
        .mip_not_flow_error_substring = "does not support integer variables",
        .floating_point_cost_error_substring = "not close enough to an integer",
        .floating_point_capacity_error_substring =
            "not close enough to an integer",
        .certifies_nontrivial_infeasibility = false,
        .returns_dual_solution = false,
    }),
    ParamName{});

// The MinCostFlow solver does not support time limits.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TimeLimitTest);
// This test should not be repeated for each solver, see b/172553545.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(InvalidInputTest);

INSTANTIATE_TEST_SUITE_P(MinCostFlowMessageCallbackTest, MessageCallbackTest,
                         testing::Values(MessageCallbackTestParams(
                             SolverType::kMinCostFlow,
                             /*support_message_callback=*/false,
                             /*support_interrupter=*/false,
                             /*integer_variables=*/false,
                             /*ending_substring=*/"")));

INSTANTIATE_TEST_SUITE_P(
    MinCostFlowCallbackTest, CallbackTest,
    testing::Values(CallbackTestParams(SolverType::kMinCostFlow,
                                       TestModelClass::kMinCostFlow,
                                       /*add_lazy_constraints=*/false,
                                       /*add_cuts=*/false,
                                       /*supported_events=*/{},
                                       /*all_solutions=*/std::nullopt,
                                       /*reaches_cut_callback=*/std::nullopt)));

std::vector<InvalidParameterTestParams> MinCostFlowBadParamTestCases() {
  std::vector<InvalidParameterTestParams> result;
  for (const auto& [param, err] :
       std::vector<std::pair<SolveParameters, std::string>>({
           {{.time_limit = absl::Seconds(1)}, "time_limit"},
           {{.iteration_limit = 1}, "iteration_limit"},
           {{.node_limit = 1}, "node_limit"},
           {{.cutoff_limit = 1.0}, "cutoff_limit"},
           {{.objective_limit = 1.0}, "objective_limit"},
           {{.best_bound_limit = 1.0}, "best_bound_limit"},
           {{.solution_limit = 1}, "solution_limit"},
           {{.threads = 1}, "threads"},
           {{.random_seed = 1}, "random_seed"},
           {{.absolute_gap_tolerance = 1e-5}, "absolute_gap_tolerance"},
           {{.relative_gap_tolerance = 1e-5}, "relative_gap_tolerance"},
           {{.solution_pool_size = 1}, "solution_pool_size"},
           {{.lp_algorithm = LPAlgorithm::kPrimalSimplex}, "lp_algorithm"},
           {{.presolve = Emphasis::kOff}, "presolve"},
           {{.cuts = Emphasis::kOff}, "cuts"},
           {{.heuristics = Emphasis::kOff}, "heuristics"},
           {{.scaling = Emphasis::kOff}, "scaling"},
       })) {
    result.push_back(InvalidParameterTestParams(
        SolverType::kMinCostFlow, TestModelClass::kMinCostFlow, param,
        {absl::StrCat("MinCostFlow solver does not support '", err,
                      "' parameter")}));
  }
  return result;
}

INSTANTIATE_TEST_SUITE_P(MinCostFlowSolverTest, InvalidParameterTest,
                         testing::ValuesIn(MinCostFlowBadParamTestCases()));

// Solver-specific tests.
TEST(MinCostFlowSolverTest, InvalidIntegerVariable) {
  Model model;
  const Variable x = model.AddIntegerVariable(0.0, 10.0);
  model.AddLinearConstraint(x == 10.0);
  model.AddLinearConstraint(-x == -10.0);
  model.Minimize(x);
  EXPECT_THAT(
      Solve(model, SolverType::kMinCostFlow),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("MinCostFlow does not support integer variables")));
}

TEST(MinCostFlowSolverTest, InvalidVariableLowerBound) {
  Model model;
  const Variable x = model.AddContinuousVariable(1.0, 10.0);
  model.AddLinearConstraint(x == 10.0);
  model.AddLinearConstraint(-x == -10.0);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("variable 0 lower bound is not 0")));
}

TEST(MinCostFlowSolverTest, InvalidInequalityConstraint) {
  Model model;
  const Variable x = model.AddContinuousVariable(1.0, 10.0);
  model.AddLinearConstraint(9.0 <= x <= 10.0);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("constraint 0 is not an equality")));
}

TEST(MinCostFlowSolverTest, VariableMissingNegativeOneCoefficient) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(x == 10.0);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("variable 0 does not have a -1 coefficient")));
}

TEST(MinCostFlowSolverTest, VariableMissingPositiveOneCoefficient) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(-x == -10.0);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("variable 0 does not have a +1 coefficient")));
}

TEST(MinCostFlowSolverTest, MultipleNegativeOneCoefficients) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(-x == -10.0);
  model.AddLinearConstraint(-x == -20.0);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("variable 0 has multiple -1 coefficients")));
}

TEST(MinCostFlowSolverTest, MultiplePositiveOneCoefficients) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(x == 10.0);
  model.AddLinearConstraint(x == 20.0);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("variable 0 has multiple +1 coefficients")));
}

TEST(MinCostFlowSolverTest, ValidLargeCostValue) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0);
  model.AddLinearConstraint(x == 1.0);
  model.AddLinearConstraint(-x == -1.0);
  model.Minimize(kLargeValidNumber * x);

  // The cost value is accepted as valid in MinCostFlowSolver::New and passed
  // down to SimpleMinCostFlow which returns "BAD_COST_RANGE" which is expected.
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              IsOkAndHolds(AllOf(TerminatesWith(TerminationReason::kOtherError),
                                 Field("termination", &SolveResult::termination,
                                       Field("detail", &Termination::detail,
                                             HasSubstr("BAD_COST_RANGE"))))));
}

TEST(MinCostFlowSolverTest, CostValueTooLarge) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(x == 10.0);
  model.AddLinearConstraint(-x == -10.0);
  model.Minimize(kLargeInvalidNumber * x);
  EXPECT_THAT(
      Solve(model, SolverType::kMinCostFlow),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("is too large to be represented as a cost value")));
}

TEST(MinCostFlowSolverTest, ValidSmallCostValue) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0);
  model.AddLinearConstraint(x == 1.0);
  model.AddLinearConstraint(-x == -1.0);
  model.Minimize(-kLargeValidNumber * x);

  // The cost value is accepted as valid in MinCostFlowSolver::New and passed
  // down to SimpleMinCostFlow which returns "BAD_COST_RANGE" which is expected.
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              IsOkAndHolds(AllOf(TerminatesWith(TerminationReason::kOtherError),
                                 Field("termination", &SolveResult::termination,
                                       Field("detail", &Termination::detail,
                                             HasSubstr("BAD_COST_RANGE"))))));
}

TEST(MinCostFlowSolverTest, CostValueTooSmall) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(x == 10.0);
  model.AddLinearConstraint(-x == -10.0);
  model.Minimize(-kLargeInvalidNumber * x);
  EXPECT_THAT(
      Solve(model, SolverType::kMinCostFlow),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("is too small to be represented as a cost value")));
}

TEST(MinCostFlowSolverTest, ValidLargeFlowQuantity) {
  Model model;
  const Variable x =
      model.AddContinuousVariable(0.0, std::numeric_limits<double>::infinity());
  model.AddLinearConstraint(x == kLargeValidNumber);
  model.AddLinearConstraint(-x == -kLargeValidNumber);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              IsOkAndHolds(IsOptimalWithSolution(kLargeValidNumber,
                                                 {{x, kLargeValidNumber}})));
}

TEST(MinCostFlowSolverTest, FlowQuantityTooLarge) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(x == kLargeInvalidNumber);
  model.Minimize(x);
  EXPECT_THAT(
      Solve(model, SolverType::kMinCostFlow),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("is too large to be represented as a flow quantity")));
}

TEST(MinCostFlowSolverTest, FlowQuantityTooSmall) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(x == -kLargeInvalidNumber);
  model.Minimize(x);
  EXPECT_THAT(
      Solve(model, SolverType::kMinCostFlow),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("is too small to be represented as a flow quantity")));
}

TEST(MinCostFlowSolverTest, InvalidFractionalConstraintLowerBound) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(0.5 <= x <= 2.0);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid constraint 0 lower-bound")));
}

TEST(MinCostFlowSolverTest, InvalidFractionalConstraintUpperBound) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(1.0 <= x <= 1.5);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid constraint 0 upper-bound")));
}

TEST(MinCostFlowSolverTest, InvalidFractionalMatrixCoefficient) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(0.5 * x == 1.0);
  model.Minimize(x);
  EXPECT_THAT(
      Solve(model, SolverType::kMinCostFlow),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("invalid coefficient for constraint 0 and variable 0")));
}

TEST(MinCostFlowSolverTest, InvalidFractionalObjectiveCoefficient) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(x == 10.0);
  model.AddLinearConstraint(-x == -10.0);
  model.Minimize(0.5 * x);
  EXPECT_THAT(
      Solve(model, SolverType::kMinCostFlow),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("invalid objective coefficient for variable 0")));
}

TEST(MinCostFlowSolverTest, InvalidFractionalVariableLowerBound) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.5, 10.0);
  model.AddLinearConstraint(x == 10.0);
  model.AddLinearConstraint(-x == -10.0);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid lower bound for variable 0")));
}

TEST(MinCostFlowSolverTest, InvalidFractionalVariableUpperBound) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.5);
  model.AddLinearConstraint(x == 10.0);
  model.AddLinearConstraint(-x == -10.0);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid upper bound for variable 0")));
}

TEST(MinCostFlowSolverTest, VariableNotInConstraints) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.Minimize(x);
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("does not appear in any constraints")));
}

TEST(MinCostFlowSolverTest, InvalidQuadraticObjective) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(x == 10.0);
  model.Minimize(x * x + 2.0 * x);
  EXPECT_THAT(
      Solve(model, SolverType::kMinCostFlow),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("MinCostFlow does not support quadratic objectives")));
}

TEST(MinCostFlowSolverTest, ComputeInfeasibleSubsystemNotSupported) {
  Model model;
  EXPECT_THAT(
      ComputeInfeasibleSubsystem(model, SolverType::kMinCostFlow),
      StatusIs(absl::StatusCode::kUnimplemented,
               HasSubstr("does not support ComputeInfeasibleSubsystem")));
}

TEST(MinCostFlowSolverTest, ConstraintBoundsDifferenceWithinTolerancePasses) {
  Model model;

  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  const Variable y = model.AddContinuousVariable(0.0, 10.0);

  model.AddLinearConstraint(10.0 <= x <=
                            10.0 + kMinCostFlowIntegralityTolerance);
  model.AddLinearConstraint(y - x == 0.0);
  model.AddLinearConstraint(-y == -10.0);

  model.Minimize(2.0 * x + 3.0 * y);

  EXPECT_THAT(
      Solve(model, SolverType::kMinCostFlow),
      IsOkAndHolds(IsOptimalWithSolution(50.0, {{x, 10.0}, {y, 10.0}})));
}

TEST(MinCostFlowSolverTest, ConstraintBoundsDifferenceMoreThanToleranceFails) {
  Model model;

  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  const Variable y = model.AddContinuousVariable(0.0, 10.0);

  model.AddLinearConstraint(10.0 <= x <=
                            10.0 + kMinCostFlowIntegralityTolerance + 1e-12);
  model.AddLinearConstraint(y - x == 0.0);
  model.AddLinearConstraint(-y == -10.0);

  model.Minimize(2.0 * x + 3.0 * y);

  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("not close enough to an integer")));
}

TEST(MinCostFlowSolverTest,
     ConstraintBoundsDifferenceFromIntegerWithinTolerancePasses) {
  Model model;

  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  const Variable y = model.AddContinuousVariable(0.0, 10.0);

  model.AddLinearConstraint(x == 10.0 + kMinCostFlowIntegralityTolerance);
  model.AddLinearConstraint(y - x == 0.0);
  model.AddLinearConstraint(-y == -10.0 - kMinCostFlowIntegralityTolerance);

  model.Minimize(2.0 * x + 3.0 * y);

  EXPECT_THAT(
      Solve(model, SolverType::kMinCostFlow),
      IsOkAndHolds(IsOptimalWithSolution(50.0, {{x, 10.0}, {y, 10.0}})));
}

TEST(MinCostFlowSolverTest,
     ConstraintBoundsDifferenceFromIntegerMoreThanToleranceFails) {
  Model model;

  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  const Variable y = model.AddContinuousVariable(0.0, 10.0);

  model.AddLinearConstraint(x == 10.0);
  model.AddLinearConstraint(y - x == 0.0);
  model.AddLinearConstraint(-y ==
                            -10.0 + kMinCostFlowIntegralityTolerance + 1e-12);

  model.Minimize(2.0 * x + 3.0 * y);

  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("not close enough to an integer")));
}

// Test that an unbalanced problem is infeasible with a specific error message.
TEST(MinCostFlowSolverTest, UnbalancedProblem) {
  Model model;

  const Variable x = model.AddContinuousVariable(0.0, 10.0);
  model.AddLinearConstraint(x == 6.0);
  model.AddLinearConstraint(-x == -5.0);
  model.Minimize(x);

  EXPECT_THAT(
      Solve(model, SolverType::kMinCostFlow),
      IsOkAndHolds(AllOf(TerminatesWith(TerminationReason::kInfeasible),
                         Field("termination", &SolveResult::termination,
                               Field("detail", &Termination::detail,
                                     HasSubstr("problem is unbalanced"))))));
}

// A valid Min-Cost Flow problem formulated as a MIP with unbounded arcs and
// large supply and cost values.
//
// Nodes A(supply=int32_max), B(supply=-5), C(supply=-(int32_max - 5))
// Arcs:
//   A->B (capacity=int32_max - 1, cost=2e8)
//   B->C (capacity=inf, cost=3e8)
//   A->C (capacity=inf, cost=5e8 + 1)
TEST(MinCostFlowSolverTest, ValidProblemWithLargeNumbers) {
  Model model;

  const double int32_max = kint32max;

  // Create arcs with large and infinite capacities.
  const Variable ab = model.AddContinuousVariable(0.0, int32_max - 1, "ab");
  const Variable bc = model.AddContinuousVariable(
      0.0, std::numeric_limits<double>::infinity(), "bc");
  const Variable ac = model.AddContinuousVariable(
      0.0, std::numeric_limits<double>::infinity(), "ac");

  // Create flow conservation constraints: outflow - inflow = supply
  model.AddLinearConstraint(ab + ac == int32_max, "NodeA");
  model.AddLinearConstraint(bc - ab == -5.0, "NodeB");
  model.AddLinearConstraint(-bc - ac == -(int32_max - 5.0), "NodeC");

  // Objective: Minimize 2e8 * ab + 3e8 * bc + (5e8 + 1) * ac
  model.Minimize(2e8 * ab + 3e8 * bc + (5e8 + 1) * ac);

  // Optimal flow:
  // A must send `int32_max` units of flow. The path A->B->C costs
  // 2e8 + 3e8 = 5e8 per unit, which is slightly cheaper than A->C (cost
  // 5e8 + 1). After saturating A->B (capacity `int32_max - 1`), A sends the
  // remaining 1 unit via A->C.
  // B consumes 5 units (supply -5 = demand 5). B forwards the remaining
  // `int32_max - 1 - 5 = int32_max - 6` units to C via B->C. C receives
  // `int32_max - 6` units from B and 1 unit from A, satisfying its demand of
  // `int32_max - 5`.
  // Costs:
  //   ab: (int32_max - 1) * 2e8
  //   bc: (int32_max - 6) * 3e8
  //   ac:  1 unit * (5e8 + 1) = 5e8 + 1
  // Total cost: 5e8 * int32_max - 1.5e9 + 1
  EXPECT_THAT(Solve(model, SolverType::kMinCostFlow),
              IsOkAndHolds(IsOptimalWithSolution(
                  5e8 * int32_max - 1.5e9 + 1.0,
                  {{ab, int32_max - 1.0}, {bc, int32_max - 6.0}, {ac, 1.0}})));
}

// Test that objective value is consistent for nontrivial models.
struct NontrivialModelParams {
  int num_nodes ABSL_REQUIRE_EXPLICIT_INIT;
  int seed ABSL_REQUIRE_EXPLICIT_INIT;
};

std::ostream& operator<<(std::ostream& out,
                         const NontrivialModelParams& params) {
  out << "{ num_nodes: " << params.num_nodes << ", seed: " << params.seed
      << " }";
  return out;
}

class NontrivialModelTest
    : public ::testing::TestWithParam<NontrivialModelParams> {};

TEST_P(NontrivialModelTest, ObjectiveValueIsConsistent) {
  const NontrivialModelParams& params = GetParam();
  const std::unique_ptr<const Model> model = NontrivialModel(
      TestModelClass::kMinCostFlow, params.num_nodes, params.seed);

  ASSERT_OK_AND_ASSIGN(const SolveResult glop_result,
                       Solve(*model, SolverType::kGlop));
  ASSERT_THAT(glop_result, IsOptimal());

  // We allow a small tolerance to account for floating point inaccuracies in
  // Glop.
  EXPECT_THAT(Solve(*model, SolverType::kMinCostFlow),
              IsOkAndHolds(IsOptimal(glop_result.objective_value(), 1.0e-6)));
}

INSTANTIATE_TEST_SUITE_P(
    MinCostFlowSolverTest, NontrivialModelTest,
    testing::Values(NontrivialModelParams{.num_nodes = 10, .seed = 1},
                    NontrivialModelParams{.num_nodes = 10, .seed = 2},
                    NontrivialModelParams{.num_nodes = 50, .seed = 3},
                    NontrivialModelParams{.num_nodes = 50, .seed = 4},
                    NontrivialModelParams{.num_nodes = 100, .seed = 5},
                    NontrivialModelParams{.num_nodes = 100, .seed = 6},
                    NontrivialModelParams{.num_nodes = 200, .seed = 7},
                    NontrivialModelParams{.num_nodes = 200, .seed = 8}),
    [](const testing::TestParamInfo<NontrivialModelTest::ParamType>& info) {
      return absl::StrCat(info.param.num_nodes, "_", info.param.seed);
    });

}  // namespace
}  // namespace operations_research::math_opt
