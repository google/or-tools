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

#include "ortools/math_opt/validators/result_validator.h"

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <tuple>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/enum_sets.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

PrimalSolutionProto VerySimplePrimalSolutionProto(
    SolutionStatusProto status, const double objective_value = 0.0) {
  PrimalSolutionProto solution;
  solution.mutable_variable_values()->add_ids(0);
  solution.mutable_variable_values()->add_values(1.0);
  solution.set_feasibility_status(status);
  solution.set_objective_value(objective_value);
  return solution;
}

DualSolutionProto VerySimpleDualSolutionProto(
    SolutionStatusProto status, const double objective_value = 0.0) {
  DualSolutionProto solution;
  solution.mutable_reduced_costs()->add_ids(0);
  solution.mutable_reduced_costs()->add_values(1.0);
  solution.set_feasibility_status(status);
  solution.set_objective_value(objective_value);
  return solution;
}

ModelSummary MakeModelSummary(
    bool is_maximize, std::initializer_list<int64_t> variables,
    std::initializer_list<int64_t> linear_constraints) {
  ModelSummary ret;
  ret.maximize = is_maximize;
  for (const auto v : variables) {
    CHECK_OK(ret.variables.Insert(v, ""));
  }
  for (const auto c : linear_constraints) {
    CHECK_OK(ret.linear_constraints.Insert(c, ""));
  }
  return ret;
}

ModelSummary VerySimpleModelSummary(const bool is_maximize) {
  return MakeModelSummary(is_maximize, {0}, {});
}

SparseDoubleVectorProto MakeSparseDoubleVector(
    std::initializer_list<std::pair<int64_t, double>> pairs) {
  SparseDoubleVectorProto ret;
  for (const auto [id, value] : pairs) {
    ret.add_ids(id);
    ret.add_values(value);
  }
  return ret;
}

PrimalSolutionProto MakePrimalFeasibleSolutionProto(
    std::initializer_list<std::pair<int64_t, double>> variable_values,
    const double objective_value) {
  PrimalSolutionProto solution;
  *solution.mutable_variable_values() = MakeSparseDoubleVector(variable_values);
  solution.set_objective_value(objective_value);
  solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  return solution;
}

PrimalRayProto MakePrimalRayProto(
    std::initializer_list<std::pair<int64_t, double>> variable_values) {
  PrimalRayProto ray;
  *ray.mutable_variable_values() = MakeSparseDoubleVector(variable_values);
  return ray;
}

DualRayProto MakeDualRayProto(
    std::initializer_list<std::pair<int64_t, double>> dual_values,
    std::initializer_list<std::pair<int64_t, double>> reduced_costs) {
  DualRayProto ray;
  *ray.mutable_dual_values() = MakeSparseDoubleVector(dual_values);
  *ray.mutable_reduced_costs() = MakeSparseDoubleVector(reduced_costs);
  return ray;
}

SolveResultProto MakeOptimalTerminationNoSolutionResult(
    double objective_value = 0.0) {
  SolveResultProto result;
  *result.mutable_termination() =
      OptimalTerminationProto(/*finite_primal_objective=*/objective_value,
                              /*dual_objective=*/objective_value);
  return result;
}

SolveResultProto MakeFeasibleTerminationNoSolutionResult(
    bool is_maximize, double objective_value = 0.0) {
  SolveResultProto result;
  *result.mutable_termination() = FeasibleTerminationProto(
      is_maximize, LIMIT_TIME, /*finite_primal_objective=*/objective_value);
  return result;
}

SolveResultProto MakeInfeasibleTerminationResult(
    bool is_maximize, FeasibilityStatusProto dual_feasibility_status =
                          FEASIBILITY_STATUS_UNDETERMINED) {
  SolveResultProto result;
  *result.mutable_termination() =
      InfeasibleTerminationProto(is_maximize, dual_feasibility_status);
  return result;
}

SolveResultProto MakeUnboundedTerminationResult(bool is_maximize) {
  SolveResultProto result;
  *result.mutable_termination() = UnboundedTerminationProto(is_maximize);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// ValidateSolutions
////////////////////////////////////////////////////////////////////////////////

TEST(ValidateSolutionsTest, ValidateSolutionCalled) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult();
  *result.add_solutions()->mutable_primal_solution() =
      MakePrimalFeasibleSolutionProto({}, std::nan(""));
  EXPECT_THAT(
      ValidateSolutions(result.solutions(), {}, ModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("NaN"), HasSubstr("objective_value"),
                     HasSubstr("primal_solution"), HasSubstr("solutions[0]"))));
}

TEST(ValidateSolutionsTest, PrimalSolutionsFeasibleAfterUndetermined) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult();
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_UNDETERMINED);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);

  EXPECT_THAT(ValidateSolutions(result.solutions(), {},
                                VerySimpleModelSummary(/*is_maximize=*/true)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("primal solution ordering not satisfied")));
}

TEST(ValidateSolutionsTest, PrimalSolutionsFeasibleAfterInfeasible) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult();
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);
  EXPECT_THAT(ValidateSolutions(result.solutions(), {},
                                VerySimpleModelSummary(/*is_maximize=*/true)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("primal solution ordering not satisfied")));
}

TEST(ValidateSolutionsTest, PrimalSolutionsOrderOk) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult();
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_UNDETERMINED);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_UNDETERMINED);
  EXPECT_OK(ValidateSolutions(result.solutions(), {},
                              VerySimpleModelSummary(/*is_maximize=*/true)));
}

TEST(ValidateSolutionsTest, DualSolutionsOrderOk) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult();
  SolutionProto& solution1 = *result.add_solutions();
  *solution1.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);
  *solution1.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE);
  SolutionProto& solution2 = *result.add_solutions();
  *solution2.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);
  *solution2.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  SolutionProto& solution3 = *result.add_solutions();
  *solution3.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  *solution3.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE);
  SolutionProto& solution4 = *result.add_solutions();
  *solution4.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  *solution4.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  EXPECT_OK(ValidateSolutions(result.solutions(), {},
                              VerySimpleModelSummary(/*is_maximize=*/true)));
}

TEST(ValidateSolutionsTest, WrongDualOrderWithinFeasiblePrimals) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult();
  SolutionProto& solution1 = *result.add_solutions();
  *solution1.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);
  *solution1.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  SolutionProto& solution2 = *result.add_solutions();
  *solution2.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);
  *solution2.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE);
  SolutionProto& solution3 = *result.add_solutions();
  *solution3.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  *solution3.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE);
  SolutionProto& solution4 = *result.add_solutions();
  *solution4.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  *solution4.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  EXPECT_THAT(ValidateSolutions(result.solutions(), {},
                                VerySimpleModelSummary(/*is_maximize=*/true)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("dual solution ordering not satisfied")));
}

TEST(ValidateSolutionsTest, WrongDualOrderWithinInfeasiblePrimals) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult();
  SolutionProto& solution1 = *result.add_solutions();
  *solution1.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);
  *solution1.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE);
  SolutionProto& solution2 = *result.add_solutions();
  *solution2.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);
  *solution2.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  SolutionProto& solution3 = *result.add_solutions();
  *solution3.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  *solution3.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  SolutionProto& solution4 = *result.add_solutions();
  *solution4.mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_INFEASIBLE);
  *solution4.mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE);
  EXPECT_THAT(ValidateSolutions(result.solutions(), {},
                                VerySimpleModelSummary(/*is_maximize=*/true)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("dual solution ordering not satisfied")));
}

////////////////////////////////////////////////////////////////////////////////
// Parameterized Test Fixtures - Solution statuses.
////////////////////////////////////////////////////////////////////////////////

class AllSolStatusParamTest
    : public testing::TestWithParam<SolutionStatusProto> {};
INSTANTIATE_TEST_SUITE_P(AllSolStatusParamTests, AllSolStatusParamTest,
                         testing::ValuesIn(AllSolutionStatuses()),
                         testing::PrintToStringParamName());

////////////////////////////////////////////////////////////////////////////////
// Parameterized Test Fixtures - Feasibility statuses.
////////////////////////////////////////////////////////////////////////////////

class AllFeasibilityStatusParamTest
    : public testing::TestWithParam<FeasibilityStatusProto> {};
INSTANTIATE_TEST_SUITE_P(AllFeasibilityStatusParamTests,
                         AllFeasibilityStatusParamTest,
                         testing::ValuesIn(AllFeasibilityStatuses()),
                         testing::PrintToStringParamName());

////////////////////////////////////////////////////////////////////////////////
// Parameterized Test Fixtures - Consistent statuses.
////////////////////////////////////////////////////////////////////////////////

class AllPairsOfSolAndFeasStatusTest
    : public testing::TestWithParam<
          std::tuple<SolutionStatusProto, FeasibilityStatusProto>> {};
INSTANTIATE_TEST_SUITE_P(
    AllPairsOfSolAndFeasStatusTests, AllPairsOfSolAndFeasStatusTest,
    testing::Combine(testing::ValuesIn(AllSolutionStatuses()),
                     testing::ValuesIn(AllFeasibilityStatuses())),
    [](const testing::TestParamInfo<AllPairsOfSolAndFeasStatusTest::ParamType>&
           info) {
      return absl::StrCat(SolutionStatusProto_Name(std::get<0>(info.param)),
                          FeasibilityStatusProto_Name(std::get<1>(info.param)));
    });

////////////////////////////////////////////////////////////////////////////////
// CheckHasPrimalSolution
////////////////////////////////////////////////////////////////////////////////

TEST(CheckHasPrimalSolutionTest, OkForFeasibleSolution) {
  SolveResultProto result;
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE);
  EXPECT_OK(CheckHasPrimalSolution(result));
}

TEST(CheckHasPrimalSolutionTest, PrimalSolutionMissing) {
  SolveResultProto result;
  // No solutions.
  EXPECT_THAT(
      CheckHasPrimalSolution(result),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("primal feasible solution expected, but not found")));
  // Solutions, but no primal solutions.
  result.add_solutions();
  EXPECT_THAT(
      CheckHasPrimalSolution(result),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("primal feasible solution expected, but not found")));
}

TEST_P(AllSolStatusParamTest, CheckHasPrimalSolutionSolNotFeasible) {
  const SolutionStatusProto primal_solution_status = GetParam();
  if (primal_solution_status == SOLUTION_STATUS_FEASIBLE) GTEST_SKIP();

  SolveResultProto result;
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(primal_solution_status);
  EXPECT_THAT(
      CheckHasPrimalSolution(result),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("primal feasible solution expected, but not found")));
}

////////////////////////////////////////////////////////////////////////////////
// CheckPrimalSolutionAndTerminationConsistency
////////////////////////////////////////////////////////////////////////////////

TEST_P(AllPairsOfSolAndFeasStatusTest,
       CheckPrimalSolutionAndTerminationConsistencyOk) {
  const auto [primal_solution_status, primal_feasibility_status] = GetParam();
  if (primal_solution_status == SOLUTION_STATUS_FEASIBLE &&
      primal_feasibility_status != FEASIBILITY_STATUS_FEASIBLE) {
    GTEST_SKIP();
  }
  SolveResultProto result;
  result.mutable_termination()->mutable_problem_status()->set_primal_status(
      primal_feasibility_status);
  result.add_solutions()->mutable_primal_solution()->set_feasibility_status(
      primal_solution_status);
  EXPECT_OK(CheckPrimalSolutionAndTerminationConsistency(
      result.termination(), result.solutions(), /*maximize=*/false));
}

TEST(CheckPrimalSolutionAndTerminationConsistency, BoundsOkNotUnboundedMax) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult(10.0);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);

  EXPECT_OK(CheckPrimalSolutionAndTerminationConsistency(
      result.termination(), result.solutions(), /*maximize=*/true));
}

TEST(CheckPrimalSolutionAndTerminationConsistency, BoundsOkNotUnboundedMin) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult(10.0);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);

  EXPECT_OK(CheckPrimalSolutionAndTerminationConsistency(
      result.termination(), result.solutions(), /*maximize=*/false));
}

// Also checks primal bound strictly better than primal solution objective.
TEST(CheckPrimalSolutionAndTerminationConsistency, BoundsOkUnboundedMax) {
  SolveResultProto result =
      MakeUnboundedTerminationResult(/*is_maximize=*/true);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);

  EXPECT_OK(CheckPrimalSolutionAndTerminationConsistency(
      result.termination(), result.solutions(), /*maximize=*/true));
}

// Also checks primal bound strictly better than primal solution objective.
TEST(CheckPrimalSolutionAndTerminationConsistency, BoundsOkUnboundedMin) {
  SolveResultProto result =
      MakeUnboundedTerminationResult(/*is_maximize=*/false);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);

  EXPECT_OK(CheckPrimalSolutionAndTerminationConsistency(
      result.termination(), result.solutions(), /*maximize=*/false));
}

TEST(CheckPrimalSolutionAndTerminationConsistency, BoundsNotOkMax) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult(10.0);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE, 20.0);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);

  EXPECT_THAT(CheckPrimalSolutionAndTerminationConsistency(
                  result.termination(), result.solutions(), /*maximize=*/true),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("better"), HasSubstr("primal_bound"))));
}

TEST(CheckPrimalSolutionAndTerminationConsistency, BoundsNotOkMin) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult(20.0);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);
  *result.add_solutions()->mutable_primal_solution() =
      VerySimplePrimalSolutionProto(SOLUTION_STATUS_FEASIBLE, 20.0);

  EXPECT_THAT(CheckPrimalSolutionAndTerminationConsistency(
                  result.termination(), result.solutions(), /*maximize=*/false),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("better"), HasSubstr("primal_bound"))));
}

TEST_P(AllFeasibilityStatusParamTest,
       CheckPrimalSolutionAndTerminationConsistencyNonFeasibleButSolAvailable) {
  const FeasibilityStatusProto feasibility_status = GetParam();
  if (feasibility_status == FEASIBILITY_STATUS_FEASIBLE) {
    GTEST_SKIP();
  }
  SolveResultProto result;
  result.mutable_termination()->mutable_problem_status()->set_primal_status(
      feasibility_status);
  result.add_solutions()->mutable_primal_solution()->set_feasibility_status(
      SOLUTION_STATUS_FEASIBLE);
  EXPECT_THAT(
      CheckPrimalSolutionAndTerminationConsistency(
          result.termination(), result.solutions(), /*maximize=*/false),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(FeasibilityStatusProto_Name(feasibility_status))));
}

////////////////////////////////////////////////////////////////////////////////
// CheckDualSolutionAndStatusConsistency
////////////////////////////////////////////////////////////////////////////////

TEST_P(AllPairsOfSolAndFeasStatusTest,
       CheckDualSolutionAndStatusConsistencyOk) {
  const auto [dual_solution_status, dual_feasibility_status] = GetParam();
  if (dual_solution_status == SOLUTION_STATUS_FEASIBLE &&
      dual_feasibility_status != FEASIBILITY_STATUS_FEASIBLE) {
    GTEST_SKIP();
  }
  SolveResultProto result;
  result.mutable_termination()->mutable_problem_status()->set_dual_status(
      dual_feasibility_status);
  result.add_solutions()->mutable_dual_solution()->set_feasibility_status(
      dual_solution_status);
  EXPECT_OK(CheckDualSolutionAndStatusConsistency(
      result.termination(), result.solutions(), /*maximize=*/false));
}

TEST_P(AllFeasibilityStatusParamTest,
       CheckDualSolutionAndStatusConsistencyNonFeasibleStatButSolAvailable) {
  const FeasibilityStatusProto feasibility_status = GetParam();
  if (feasibility_status == FEASIBILITY_STATUS_FEASIBLE) {
    GTEST_SKIP();
  }
  SolveResultProto result;
  result.mutable_termination()->mutable_problem_status()->set_dual_status(
      feasibility_status);
  result.add_solutions()->mutable_dual_solution()->set_feasibility_status(
      SOLUTION_STATUS_FEASIBLE);
  EXPECT_THAT(
      CheckDualSolutionAndStatusConsistency(
          result.termination(), result.solutions(), /*maximize=*/false),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr(FeasibilityStatusProto_Name(feasibility_status))));
}

TEST(CheckDualSolutionAndTerminationConsistency, BoundsOkNotDualUnboundedMax) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult(10.0);
  *result.add_solutions()->mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);

  EXPECT_OK(CheckDualSolutionAndStatusConsistency(
      result.termination(), result.solutions(), /*maximize=*/true));
}

TEST(CheckDualSolutionAndTerminationConsistency, BoundsOkNotDualUnboundedMin) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult(10.0);
  *result.add_solutions()->mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);

  EXPECT_OK(CheckDualSolutionAndStatusConsistency(
      result.termination(), result.solutions(), /*maximize=*/false));
}

// Also checks dual bound strictly better than dual solution objective.
TEST(CheckDualSolutionAndTerminationConsistency, BoundsOkDualUnboundedMax) {
  SolveResultProto result = MakeInfeasibleTerminationResult(
      /*is_maximize=*/true,
      /*dual_feasibility_status=*/FEASIBILITY_STATUS_FEASIBLE);
  *result.add_solutions()->mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);

  EXPECT_OK(CheckDualSolutionAndStatusConsistency(
      result.termination(), result.solutions(), /*maximize=*/true));
}

// Also checks dual bound strictly better than dual solution objective.
TEST(CheckDualSolutionAndTerminationConsistency, BoundsOkDualUnboundedMin) {
  SolveResultProto result = MakeInfeasibleTerminationResult(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FEASIBILITY_STATUS_FEASIBLE);
  *result.add_solutions()->mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);

  EXPECT_OK(CheckDualSolutionAndStatusConsistency(
      result.termination(), result.solutions(), /*maximize=*/false));
}

TEST(CheckDualSolutionAndTerminationConsistency, BoundsNotOkMax) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult(20.0);
  *result.add_solutions()->mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);
  *result.add_solutions()->mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE, 20.0);

  EXPECT_THAT(CheckDualSolutionAndStatusConsistency(
                  result.termination(), result.solutions(), /*maximize=*/true),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("better"), HasSubstr("dual_bound"))));
}

TEST(CheckDualSolutionAndTerminationConsistency, BoundsNotOkMin) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult(10.0);
  *result.add_solutions()->mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE, 20.0);
  *result.add_solutions()->mutable_dual_solution() =
      VerySimpleDualSolutionProto(SOLUTION_STATUS_FEASIBLE, 10.0);

  EXPECT_THAT(CheckDualSolutionAndStatusConsistency(
                  result.termination(), result.solutions(), /*maximize=*/false),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("better"), HasSubstr("dual_bound"))));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateResult
////////////////////////////////////////////////////////////////////////////////

TEST(ValidateResultTest, ValidateTerminationCalled) {
  SolveResultProto result =
      MakeInfeasibleTerminationResult(/*is_maximize=*/true);
  result.mutable_termination()->set_reason(TERMINATION_REASON_UNSPECIFIED);
  ModelSummary model_summary;
  model_summary.maximize = true;
  EXPECT_THAT(ValidateResult(result, {}, model_summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("termination reason must be specified")));
}

TEST(ValidateResultTest, ValidateSolveStatsCalled) {
  SolveResultProto result =
      MakeInfeasibleTerminationResult(/*is_maximize=*/true);
  result.mutable_solve_stats()->set_simplex_iterations(-1);
  ModelSummary model_summary;
  model_summary.maximize = true;
  EXPECT_THAT(ValidateResult(result, {}, model_summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("simplex_iterations must be non-negative")));
}

TEST(ValidateResultTest, ValidateSolutionsCalled) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult();
  *result.add_solutions()->mutable_primal_solution() =
      MakePrimalFeasibleSolutionProto({}, std::nan(""));
  EXPECT_THAT(
      ValidateResult(result, {}, ModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("NaN"), HasSubstr("objective_value"),
                     HasSubstr("primal_solution"), HasSubstr("solutions[0]"))));
}

TEST(ValidateResultTest, CheckHasPrimalSolutionCalledForOptimal) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult();
  EXPECT_THAT(ValidateResult(result, {}, ModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("primal feasible solution expected"),
                             HasSubstr("TERMINATION_REASON_OPTIMAL"))));
}

TEST(ValidateResultTest, CheckHasPrimalSolutionCalledForFeasible) {
  SolveResultProto result = MakeFeasibleTerminationNoSolutionResult(
      /*is_maximize=*/true);
  ModelSummary model_summary;
  model_summary.maximize = true;
  EXPECT_THAT(ValidateResult(result, {}, model_summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("primal feasible solution expected"),
                             HasSubstr("TERMINATION_REASON_FEASIBLE"))));
}

TEST(ValidateResultTest,
     CheckCheckPrimalSolutionAndTerminationConsistencyCalled) {
  SolveResultProto result =
      MakeInfeasibleTerminationResult(/*is_maximize=*/true);
  result.add_solutions()->mutable_primal_solution()->set_feasibility_status(
      SOLUTION_STATUS_FEASIBLE);
  ModelSummary model_summary;
  model_summary.maximize = true;
  EXPECT_THAT(ValidateResult(result, {}, model_summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("FEASIBILITY_STATUS_INFEASIBLE")));
}

TEST(ValidateResultTest, CheckCheckDualSolutionAndStatusConsistencyCalled) {
  SolveResultProto result = MakeInfeasibleTerminationResult(
      /*is_maximize=*/true,
      /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE);
  result.add_solutions()->mutable_dual_solution()->set_feasibility_status(
      SOLUTION_STATUS_FEASIBLE);
  ModelSummary model_summary;
  model_summary.maximize = true;
  EXPECT_THAT(ValidateResult(result, {}, model_summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("FEASIBILITY_STATUS_INFEASIBLE")));
}

TEST(ValidateResultTest, PrimalRayStatusConsistencyNotOk) {
  SolveResultProto result =
      MakeOptimalTerminationNoSolutionResult(/*objective_value=*/0.0);
  *result.add_solutions()->mutable_primal_solution() =
      MakePrimalFeasibleSolutionProto({{0, 0.0}}, 0.0);
  *result.add_primal_rays() = MakePrimalRayProto({{0, 0.0}});
  EXPECT_THAT(
      ValidateResult(result, {},
                     MakeModelSummary(/*is_maximize=*/true, {0}, {})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("dual_status"), HasSubstr("primal ray"))));
}

TEST(ValidateResultTest, ValidatePrimalRayCalled) {
  // We only test error propagation here, the specific test cases are tested
  // in solution_validator_test.cc.
  SolveResultProto result =
      MakeUnboundedTerminationResult(/*is_maximize=*/true);

  *result.add_primal_rays() = MakePrimalRayProto({{0, std::nan("")}});
  EXPECT_THAT(ValidateResult(result, {},
                             MakeModelSummary(/*is_maximize=*/true, {0}, {})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("NaN"), HasSubstr("variable_values"),
                             HasSubstr("primal_rays[0]"))));
}

TEST(ValidateResultTest, DualRayStatusConsistencyNotOk) {
  SolveResultProto result = MakeOptimalTerminationNoSolutionResult();
  *result.add_solutions()->mutable_primal_solution() =
      MakePrimalFeasibleSolutionProto({{0, 0.0}}, 0.0);
  *result.add_dual_rays() = MakeDualRayProto({{0, 0}}, {});
  EXPECT_THAT(
      ValidateResult(result, {},
                     MakeModelSummary(/*is_maximize=*/true, {0}, {})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("primal_status"), HasSubstr("dual ray"))));
}

TEST(ValidateResultTest, ValidateDualRayCalled) {
  // We only test error propagation here, the specific test cases are tested
  // in solution_validator_test.cc.
  SolveResultProto result = MakeInfeasibleTerminationResult(
      /*is_maximize=*/true,
      /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE);
  *result.add_dual_rays() = MakeDualRayProto({{0, std::nan("")}}, {});
  EXPECT_THAT(ValidateResult(result, {},
                             MakeModelSummary(/*is_maximize=*/true, {}, {0})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("NaN"), HasSubstr("dual_values"),
                             HasSubstr("dual_rays[0]"))));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
