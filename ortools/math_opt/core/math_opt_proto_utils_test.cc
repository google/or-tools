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

#include "ortools/math_opt/core/math_opt_proto_utils.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/core/sparse_collection_matchers.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::EqualsProto;
using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(GetObjectiveBounds, FromTermination) {
  SolveResultProto solve_result;
  solve_result.mutable_termination()
      ->mutable_objective_bounds()
      ->set_dual_bound(1.0);
  solve_result.mutable_termination()
      ->mutable_objective_bounds()
      ->set_primal_bound(2.0);
  solve_result.mutable_solve_stats()->set_best_dual_bound(10.0);
  solve_result.mutable_solve_stats()->set_best_primal_bound(20.0);
  const ObjectiveBoundsProto bounds = GetObjectiveBounds(solve_result);
  EXPECT_THAT(bounds,
              EqualsProto(solve_result.termination().objective_bounds()));
}

TEST(GetObjectiveBounds, FromSolveStats) {
  SolveResultProto solve_result;
  solve_result.mutable_solve_stats()->set_best_dual_bound(10.0);
  solve_result.mutable_solve_stats()->set_best_primal_bound(20.0);
  const ObjectiveBoundsProto bounds = GetObjectiveBounds(solve_result);
  EXPECT_EQ(bounds.dual_bound(), 10.0);
  EXPECT_EQ(bounds.primal_bound(), 20.0);
}

TEST(GetProblemStatus, FromTermination) {
  SolveResultProto solve_result;
  solve_result.mutable_termination()
      ->mutable_problem_status()
      ->set_primal_status(FEASIBILITY_STATUS_INFEASIBLE);
  solve_result.mutable_termination()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_INFEASIBLE);
  solve_result.mutable_solve_stats()
      ->mutable_problem_status()
      ->set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
  solve_result.mutable_solve_stats()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_FEASIBLE);
  const ProblemStatusProto problem_status = GetProblemStatus(solve_result);
  EXPECT_THAT(problem_status,
              EqualsProto(solve_result.termination().problem_status()));
}

TEST(GetProblemStatus, FromSolveStats) {
  SolveResultProto solve_result;
  solve_result.mutable_solve_stats()
      ->mutable_problem_status()
      ->set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
  solve_result.mutable_solve_stats()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_FEASIBLE);
  const ProblemStatusProto problem_status = GetProblemStatus(solve_result);
  EXPECT_THAT(problem_status,
              EqualsProto(solve_result.solve_stats().problem_status()));
}

TEST(MakeTrivialBounds, Max) {
  const ObjectiveBoundsProto bounds = MakeTrivialBounds(/*is_maximize=*/true);
  EXPECT_EQ(bounds.primal_bound(), -kInf);
  EXPECT_EQ(bounds.dual_bound(), +kInf);
}

TEST(MakeTrivialBounds, Min) {
  const ObjectiveBoundsProto bounds = MakeTrivialBounds(/*is_maximize=*/false);
  EXPECT_EQ(bounds.primal_bound(), +kInf);
  EXPECT_EQ(bounds.dual_bound(), -kInf);
}

TEST(OptimalTerminationProto, OptimalTerminationProto) {
  const TerminationProto termination =
      OptimalTerminationProto(1.0, 2.0, "detail");
  EXPECT_EQ(termination.reason(), TERMINATION_REASON_OPTIMAL);
  EXPECT_EQ(termination.limit(), LIMIT_UNSPECIFIED);
  EXPECT_EQ(termination.objective_bounds().primal_bound(), 1.0);
  EXPECT_EQ(termination.objective_bounds().dual_bound(), 2.0);
  EXPECT_EQ(termination.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination.detail(), "detail");
}

TEST(InfeasibleTerminationProto, DualFeasible) {
  const TerminationProto termination_max = InfeasibleTerminationProto(
      /*is_maximize=*/true, FEASIBILITY_STATUS_FEASIBLE, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_INFEASIBLE);
  EXPECT_EQ(termination_max.limit(), LIMIT_UNSPECIFIED);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = InfeasibleTerminationProto(
      /*is_maximize=*/false, FEASIBILITY_STATUS_FEASIBLE, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_INFEASIBLE);
  EXPECT_EQ(termination_min.limit(), LIMIT_UNSPECIFIED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), kInf);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_min.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(InfeasibleTerminationProto, DualInfeasibleAndUndetermined) {
  const TerminationProto termination_max = InfeasibleTerminationProto(
      /*is_maximize=*/true, FEASIBILITY_STATUS_INFEASIBLE, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_INFEASIBLE);
  EXPECT_EQ(termination_max.limit(), LIMIT_UNSPECIFIED);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = InfeasibleTerminationProto(
      /*is_maximize=*/false, FEASIBILITY_STATUS_UNDETERMINED, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_INFEASIBLE);
  EXPECT_EQ(termination_min.limit(), LIMIT_UNSPECIFIED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), kInf);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(termination_min.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(InfeasibleOrUnboundedTerminationProto, DualInfeasible) {
  const TerminationProto termination_max =
      InfeasibleOrUnboundedTerminationProto(
          /*is_maximize=*/true, FEASIBILITY_STATUS_INFEASIBLE, "detail_max");
  EXPECT_EQ(termination_max.reason(),
            TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED);
  EXPECT_EQ(termination_max.limit(), LIMIT_UNSPECIFIED);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min =
      InfeasibleOrUnboundedTerminationProto(
          /*is_maximize=*/false, FEASIBILITY_STATUS_INFEASIBLE, "detail_min");
  EXPECT_EQ(termination_min.reason(),
            TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED);
  EXPECT_EQ(termination_min.limit(), LIMIT_UNSPECIFIED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), kInf);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_FALSE(termination_min.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(InfeasibleOrUnboundedTerminationProto, DualUndetermined) {
  const TerminationProto termination_max =
      InfeasibleOrUnboundedTerminationProto(
          /*is_maximize=*/true, FEASIBILITY_STATUS_UNDETERMINED, "detail_max");
  EXPECT_EQ(termination_max.reason(),
            TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED);
  EXPECT_EQ(termination_max.limit(), LIMIT_UNSPECIFIED);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_TRUE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min =
      InfeasibleOrUnboundedTerminationProto(
          /*is_maximize=*/false, FEASIBILITY_STATUS_UNDETERMINED, "detail_min");
  EXPECT_EQ(termination_min.reason(),
            TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED);
  EXPECT_EQ(termination_min.limit(), LIMIT_UNSPECIFIED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), kInf);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_TRUE(termination_min.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(UnboundedTerminationProto, Max) {
  const TerminationProto termination = UnboundedTerminationProto(
      /*is_maximize=*/true, "detail");
  EXPECT_EQ(termination.reason(), TERMINATION_REASON_UNBOUNDED);
  EXPECT_EQ(termination.limit(), LIMIT_UNSPECIFIED);
  EXPECT_EQ(termination.objective_bounds().primal_bound(), kInf);
  EXPECT_EQ(termination.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination.problem_status().dual_status(),
            FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_FALSE(termination.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination.detail(), "detail");
}

TEST(UnboundedTerminationProto, Min) {
  const TerminationProto termination = UnboundedTerminationProto(
      /*is_maximize=*/false, "detail");
  EXPECT_EQ(termination.reason(), TERMINATION_REASON_UNBOUNDED);
  EXPECT_EQ(termination.limit(), LIMIT_UNSPECIFIED);
  EXPECT_EQ(termination.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination.problem_status().dual_status(),
            FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_FALSE(termination.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination.detail(), "detail");
}

TEST(NoSolutionFoundTerminationProto, NoDualObjective) {
  const TerminationProto termination_max = NoSolutionFoundTerminationProto(
      /*is_maximize=*/true, LIMIT_TIME,
      /*optional_dual_objective=*/std::nullopt, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_max.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = NoSolutionFoundTerminationProto(
      /*is_maximize=*/false, LIMIT_INTERRUPTED,
      /*optional_dual_objective=*/std::nullopt, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_min.limit(), LIMIT_INTERRUPTED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), kInf);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(NoSolutionFoundTerminationProto, FiniteDualObjective) {
  const TerminationProto termination_max = NoSolutionFoundTerminationProto(
      /*is_maximize=*/true, LIMIT_TIME,
      /*optional_dual_objective=*/20.0, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_max.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), 20.0);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = NoSolutionFoundTerminationProto(
      /*is_maximize=*/false, LIMIT_INTERRUPTED,
      /*optional_dual_objective=*/10.0, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_min.limit(), LIMIT_INTERRUPTED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), kInf);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), 10.0);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(NoSolutionFoundTerminationProto, InfiniteDualObjective) {
  const TerminationProto termination_max = NoSolutionFoundTerminationProto(
      /*is_maximize=*/true, LIMIT_TIME,
      /*optional_dual_objective=*/kInf, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_max.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = NoSolutionFoundTerminationProto(
      /*is_maximize=*/false, LIMIT_INTERRUPTED,
      /*optional_dual_objective=*/-kInf, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_min.limit(), LIMIT_INTERRUPTED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), kInf);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(FeasibleTerminationProto, NoDualObjective) {
  const TerminationProto termination_max = FeasibleTerminationProto(
      /*is_maximize=*/true, LIMIT_TIME,
      /*finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/std::nullopt, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_max.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), 10.0);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = FeasibleTerminationProto(
      /*is_maximize=*/false, LIMIT_INTERRUPTED,
      /*finite_primal_objective=*/20.0,
      /*optional_dual_objective=*/std::nullopt, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_min.limit(), LIMIT_INTERRUPTED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), 20.0);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(FeasibleTerminationProto, FiniteDualObjective) {
  const TerminationProto termination_max = FeasibleTerminationProto(
      /*is_maximize=*/true, LIMIT_TIME,
      /*finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/30.0, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_max.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), 10.0);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), 30.0);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = FeasibleTerminationProto(
      /*is_maximize=*/false, LIMIT_INTERRUPTED,
      /*finite_primal_objective=*/20.0,
      /*optional_dual_objective=*/10.0, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_min.limit(), LIMIT_INTERRUPTED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), 20.0);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), 10.0);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(FeasibleTerminationProto, InfiniteDualObjective) {
  const TerminationProto termination_max = FeasibleTerminationProto(
      /*is_maximize=*/true, LIMIT_TIME,
      /*finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/kInf, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_max.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), 10.0);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = FeasibleTerminationProto(
      /*is_maximize=*/false, LIMIT_INTERRUPTED,
      /*finite_primal_objective=*/20.0,
      /*optional_dual_objective=*/-kInf, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_min.limit(), LIMIT_INTERRUPTED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), 20.0);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(LimitTerminationProto, NoPrimalOrDualObjective) {
  const TerminationProto termination_max = LimitTerminationProto(
      /*is_maximize=*/true, LIMIT_TIME,
      /*optional_finite_primal_objective=*/std::nullopt,
      /*optional_dual_objective=*/std::nullopt, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_max.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = LimitTerminationProto(
      /*is_maximize=*/false, LIMIT_INTERRUPTED,
      /*optional_finite_primal_objective=*/std::nullopt,
      /*optional_dual_objective=*/std::nullopt, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_min.limit(), LIMIT_INTERRUPTED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), kInf);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(LimitTerminationProto, NoDualObjective) {
  const TerminationProto termination_max = LimitTerminationProto(
      /*is_maximize=*/true, LIMIT_TIME,
      /*optional_finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/std::nullopt, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_max.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), 10.0);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = LimitTerminationProto(
      /*is_maximize=*/false, LIMIT_INTERRUPTED,
      /*optional_finite_primal_objective=*/20.0,
      /*optional_dual_objective=*/std::nullopt, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_min.limit(), LIMIT_INTERRUPTED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), 20.0);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(LimitTerminationProto, FiniteDualObjective) {
  const TerminationProto termination_max = LimitTerminationProto(
      /*is_maximize=*/true, LIMIT_TIME,
      /*optional_finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/30.0, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_max.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), 10.0);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), 30.0);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = LimitTerminationProto(
      /*is_maximize=*/false, LIMIT_INTERRUPTED,
      /*optional_finite_primal_objective=*/20.0,
      /*optional_dual_objective=*/10.0, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_min.limit(), LIMIT_INTERRUPTED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), 20.0);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), 10.0);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(LimitTerminationProto, InfiniteDualObjective) {
  const TerminationProto termination_max = LimitTerminationProto(
      /*is_maximize=*/true, LIMIT_TIME,
      /*optional_finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/kInf, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_max.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), 10.0);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = LimitTerminationProto(
      /*is_maximize=*/false, LIMIT_INTERRUPTED,
      /*optional_finite_primal_objective=*/20.0,
      /*optional_dual_objective=*/-kInf, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_min.limit(), LIMIT_INTERRUPTED);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), 20.0);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(LimitTerminationProtoNoOptional, InfinitePrimalAndDual) {
  const TerminationProto termination_no_dual_sol = LimitTerminationProto(
      LIMIT_TIME,
      /*primal_objective=*/-kInf,
      /*dual_objective=*/kInf, /*claim_dual_feasible_solution_exists=*/false,
      "detail_no_dual");
  EXPECT_EQ(termination_no_dual_sol.reason(),
            TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_no_dual_sol.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_no_dual_sol.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_no_dual_sol.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_no_dual_sol.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_no_dual_sol.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(
      termination_no_dual_sol.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_no_dual_sol.detail(), "detail_no_dual");

  const TerminationProto termination_dual_sol = LimitTerminationProto(
      LIMIT_TIME,
      /*primal_objective=*/-kInf,
      /*dual_objective=*/kInf, /*claim_dual_feasible_solution_exists=*/true,
      "detail_dual");
  EXPECT_EQ(termination_dual_sol.reason(),
            TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_dual_sol.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_dual_sol.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_dual_sol.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_dual_sol.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_dual_sol.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(
      termination_dual_sol.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_dual_sol.detail(), "detail_dual");
}

TEST(LimitTerminationProtoNoOptional, InfinitePrimalFiniteDual) {
  const TerminationProto termination_no_dual_sol = LimitTerminationProto(
      LIMIT_TIME,
      /*primal_objective=*/-kInf,
      /*dual_objective=*/10.0, /*claim_dual_feasible_solution_exists=*/false,
      "detail_no_dual");
  EXPECT_EQ(termination_no_dual_sol.reason(),
            TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_no_dual_sol.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_no_dual_sol.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_no_dual_sol.objective_bounds().dual_bound(), 10.0);
  EXPECT_EQ(termination_no_dual_sol.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_no_dual_sol.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(
      termination_no_dual_sol.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_no_dual_sol.detail(), "detail_no_dual");

  const TerminationProto termination_dual_sol = LimitTerminationProto(
      LIMIT_TIME,
      /*primal_objective=*/-kInf,
      /*dual_objective=*/10.0, /*claim_dual_feasible_solution_exists=*/true,
      "detail_dual");
  EXPECT_EQ(termination_dual_sol.reason(),
            TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_dual_sol.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_dual_sol.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_dual_sol.objective_bounds().dual_bound(), 10.0);
  EXPECT_EQ(termination_dual_sol.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_dual_sol.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(
      termination_dual_sol.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_dual_sol.detail(), "detail_dual");
}

TEST(LimitTerminationProtoNoOptional, FinitePrimalAndDual) {
  const TerminationProto termination_no_dual_sol = LimitTerminationProto(
      LIMIT_TIME,
      /*primal_objective=*/1.0,
      /*dual_objective=*/10.0, /*claim_dual_feasible_solution_exists=*/false,
      "detail_no_dual");
  EXPECT_EQ(termination_no_dual_sol.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_no_dual_sol.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_no_dual_sol.objective_bounds().primal_bound(), 1.0);
  EXPECT_EQ(termination_no_dual_sol.objective_bounds().dual_bound(), 10.0);
  EXPECT_EQ(termination_no_dual_sol.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_no_dual_sol.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(
      termination_no_dual_sol.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_no_dual_sol.detail(), "detail_no_dual");

  const TerminationProto termination_dual_sol = LimitTerminationProto(
      LIMIT_TIME,
      /*primal_objective=*/1.0,
      /*dual_objective=*/10.0, /*claim_dual_feasible_solution_exists=*/true,
      "detail_dual");
  EXPECT_EQ(termination_dual_sol.reason(), TERMINATION_REASON_FEASIBLE);
  EXPECT_EQ(termination_dual_sol.limit(), LIMIT_TIME);
  EXPECT_EQ(termination_dual_sol.objective_bounds().primal_bound(), 1.0);
  EXPECT_EQ(termination_dual_sol.objective_bounds().dual_bound(), 10.0);
  EXPECT_EQ(termination_dual_sol.problem_status().primal_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(termination_dual_sol.problem_status().dual_status(),
            FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_FALSE(
      termination_dual_sol.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_dual_sol.detail(), "detail_dual");
}

TEST(LimitCutoffTerminationProto, LimitCutoffTerminationProto) {
  const TerminationProto termination_max = CutoffTerminationProto(
      /*is_maximize=*/true, "detail_max");
  EXPECT_EQ(termination_max.reason(), TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_max.limit(), LIMIT_CUTOFF);
  EXPECT_EQ(termination_max.objective_bounds().primal_bound(), -kInf);
  EXPECT_EQ(termination_max.objective_bounds().dual_bound(), kInf);
  EXPECT_EQ(termination_max.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_max.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_max.detail(), "detail_max");

  const TerminationProto termination_min = CutoffTerminationProto(
      /*is_maximize=*/false, "detail_min");
  EXPECT_EQ(termination_min.reason(), TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(termination_min.limit(), LIMIT_CUTOFF);
  EXPECT_EQ(termination_min.objective_bounds().primal_bound(), +kInf);
  EXPECT_EQ(termination_min.objective_bounds().dual_bound(), -kInf);
  EXPECT_EQ(termination_min.problem_status().primal_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(termination_min.problem_status().dual_status(),
            FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_FALSE(termination_max.problem_status().primal_or_dual_infeasible());
  EXPECT_EQ(termination_min.detail(), "detail_min");
}

TEST(FirstVariableIdTest, Empty) {
  EXPECT_EQ(FirstVariableId({}), std::nullopt);
}

TEST(FirstVariableIdTest, NonEmpty) {
  VariablesProto variables;
  variables.add_ids(3);
  variables.add_ids(5);
  variables.add_ids(7);
  EXPECT_THAT(FirstVariableId(variables), Optional(3));
}

TEST(FirstLinearConstraintIdTest, Empty) {
  EXPECT_EQ(FirstLinearConstraintId({}), std::nullopt);
}

TEST(FirstLinearConstraintIdTest, NonEmpty) {
  LinearConstraintsProto linear_constraints;
  linear_constraints.add_ids(3);
  linear_constraints.add_ids(5);
  linear_constraints.add_ids(7);
  EXPECT_THAT(FirstLinearConstraintId(linear_constraints), Optional(3));
}

TEST(RemoveSparseDoubleVectorZerosTest, Empty) {
  SparseDoubleVectorProto v;
  RemoveSparseDoubleVectorZeros(v);
  EXPECT_THAT(v, SparseVectorMatcher(Pairs<double>{}));
}

TEST(RemoveSparseDoubleVectorZerosTest, NoZeros) {
  SparseDoubleVectorProto v = MakeSparseDoubleVector({{3, 2.5}, {4, 4.0}});
  RemoveSparseDoubleVectorZeros(v);
  EXPECT_THAT(v, SparseVectorMatcher(Pairs<double>{{3, 2.5}, {4, 4.0}}));
}

TEST(RemoveSparseDoubleVectorZerosTest, NaN) {
  SparseDoubleVectorProto v = MakeSparseDoubleVector({{3, std::nan("")}});
  RemoveSparseDoubleVectorZeros(v);
  ASSERT_EQ(v.values_size(), 1);
  EXPECT_TRUE(std::isnan(v.values(0)));
}

TEST(RemoveSparseDoubleVectorZerosTest, SomeZeros) {
  SparseDoubleVectorProto v = MakeSparseDoubleVector(
      {{1, 0.0}, {3, 2.5}, {5, 0.0}, {6, 4.0}, {9, 0.0}});
  RemoveSparseDoubleVectorZeros(v);
  EXPECT_THAT(v, SparseVectorMatcher(Pairs<double>{{3, 2.5}, {6, 4.0}}));
}

TEST(RemoveSparseDoubleVectorZerosTest, AllZeros) {
  SparseDoubleVectorProto v = MakeSparseDoubleVector({{3, 0.0}, {4, 0.0}});
  RemoveSparseDoubleVectorZeros(v);
  EXPECT_THAT(v, SparseVectorMatcher(Pairs<double>{}));
}

TEST(RemoveSparseDoubleVectorZerosDeathTest, Invalid) {
  SparseDoubleVectorProto v = MakeSparseDoubleVector({{3, 0.0}, {4, 0.0}});
  v.mutable_ids()->RemoveLast();
  EXPECT_DEATH(RemoveSparseDoubleVectorZeros(v), "ids");
}

// Returns the result of calling AcceptsAndUpdate() with the input parameters on
// a temporary SparseVectorFilterPredicate.
//
// This is useful in tests that don't want/need to check the sequential aspect
// of AcceptsAndUpdate().
template <typename Value>
bool CallPredicateAcceptsAndUpdateOnce(const SparseVectorFilterProto& filter,
                                       const int64_t id, const Value& value) {
  return SparseVectorFilterPredicate(filter).AcceptsAndUpdate(id, value);
}

TEST(SparseVectorFilterPredicateTest, DoubleZeros) {
  SparseVectorFilterProto filter;
  filter.set_skip_zero_values(true);
  // These should be ignored since we haven't set `filter_by_ids`.
  filter.mutable_filtered_ids()->Add(2);
  filter.mutable_filtered_ids()->Add(3);

  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 1, 0.0));
  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 2, 0.0));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 1, 3.0));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 2, 3.0));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 3, kInf));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 3, -kInf));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 3, std::nan("")));
}

TEST(SparseVectorFilterPredicateTest, DoubleZerosAndIds) {
  SparseVectorFilterProto filter;
  filter.set_skip_zero_values(true);
  filter.set_filter_by_ids(true);
  filter.mutable_filtered_ids()->Add(2);
  filter.mutable_filtered_ids()->Add(3);

  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 1, 0.0));
  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 2, 0.0));
  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 1, 3.0));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 2, 3.0));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 3, kInf));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 3, -kInf));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 3, std::nan("")));
}

TEST(SparseVectorFilterPredicateTest, DoubleIds) {
  SparseVectorFilterProto filter;
  filter.set_filter_by_ids(true);
  filter.mutable_filtered_ids()->Add(2);
  filter.mutable_filtered_ids()->Add(3);

  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 1, 0.0));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 2, 0.0));
  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 1, 3.0));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 2, 3.0));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 3, kInf));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 3, -kInf));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 3, std::nan("")));
}

TEST(SparseVectorFilterPredicateTest, BoolZeros) {
  SparseVectorFilterProto filter;
  filter.set_skip_zero_values(true);
  // These should be ignored since we haven't set `filter_by_ids`.
  filter.mutable_filtered_ids()->Add(2);
  filter.mutable_filtered_ids()->Add(3);

  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 1, false));
  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 2, false));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 1, true));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 2, true));
}

TEST(SparseVectorFilterPredicateTest, BoolZerosAndIds) {
  SparseVectorFilterProto filter;
  filter.set_skip_zero_values(true);
  filter.set_filter_by_ids(true);
  filter.mutable_filtered_ids()->Add(2);
  filter.mutable_filtered_ids()->Add(3);

  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 1, false));
  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 2, false));
  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 1, true));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 2, true));
}

TEST(SparseVectorFilterPredicateTest, BoolIds) {
  SparseVectorFilterProto filter;
  filter.set_filter_by_ids(true);
  filter.mutable_filtered_ids()->Add(2);
  filter.mutable_filtered_ids()->Add(3);

  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 1, false));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 2, false));
  EXPECT_FALSE(CallPredicateAcceptsAndUpdateOnce(filter, 1, true));
  EXPECT_TRUE(CallPredicateAcceptsAndUpdateOnce(filter, 2, true));
}

TEST(SparseVectorFilterPredicateTest, FilteredIds) {
  SparseVectorFilterProto filter;
  filter.set_filter_by_ids(true);
  filter.mutable_filtered_ids()->Add(2);
  filter.mutable_filtered_ids()->Add(3);
  filter.mutable_filtered_ids()->Add(5);

  SparseVectorFilterPredicate predicate(filter);
  EXPECT_FALSE(predicate.AcceptsAndUpdate(1, 2.0));
  EXPECT_TRUE(predicate.AcceptsAndUpdate(3, 1.0));
  EXPECT_FALSE(predicate.AcceptsAndUpdate(4, 3.0));
  EXPECT_TRUE(predicate.AcceptsAndUpdate(5, -2.0));
  EXPECT_FALSE(predicate.AcceptsAndUpdate(8, -3.0));
}

TEST(SparseVectorFilterPredicateTest, FilteredIdsAndZeros) {
  SparseVectorFilterProto filter;
  filter.set_skip_zero_values(true);
  filter.set_filter_by_ids(true);
  filter.mutable_filtered_ids()->Add(2);
  filter.mutable_filtered_ids()->Add(3);
  filter.mutable_filtered_ids()->Add(5);

  SparseVectorFilterPredicate predicate(filter);
  EXPECT_FALSE(predicate.AcceptsAndUpdate(1, 2.0));
  EXPECT_FALSE(predicate.AcceptsAndUpdate(3, 0.0));
  EXPECT_FALSE(predicate.AcceptsAndUpdate(4, 3.0));
  EXPECT_TRUE(predicate.AcceptsAndUpdate(5, -2.0));
  EXPECT_FALSE(predicate.AcceptsAndUpdate(8, -3.0));
  EXPECT_FALSE(predicate.AcceptsAndUpdate(10, 0.0));
}

TEST(SparseVectorFilterPredicateDeathTest, UnsortedFilteredIds) {
  if (!DEBUG_MODE) {
    GTEST_SKIP()
        << "The assertion that the filtered ids are sorted only exists "
           "in non-optimized builds. We are executing this test in an "
           "optimized build so we skip it.";
  }

  SparseVectorFilterProto filter;
  filter.set_filter_by_ids(true);
  filter.mutable_filtered_ids()->Add(3);
  filter.mutable_filtered_ids()->Add(2);
  filter.mutable_filtered_ids()->Add(5);

  EXPECT_DEATH(SparseVectorFilterPredicate{filter}, "strictly increasing");
}

TEST(SparseVectorFilterPredicateDeathTest, DuplicatedFilteredIds) {
  if (!DEBUG_MODE) {
    GTEST_SKIP()
        << "The assertion that the filtered ids are sorted only exists "
           "in non-optimized builds. We are executing this test in an "
           "optimized build so we skip it.";
  }

  SparseVectorFilterProto filter;
  filter.set_filter_by_ids(true);
  filter.mutable_filtered_ids()->Add(2);
  filter.mutable_filtered_ids()->Add(5);
  filter.mutable_filtered_ids()->Add(5);
  filter.mutable_filtered_ids()->Add(6);

  EXPECT_DEATH(SparseVectorFilterPredicate{filter}, "strictly increasing");
}

TEST(SparseVectorFilterPredicateDeathTest, UnsortedInputIds) {
  if (!DEBUG_MODE) {
    GTEST_SKIP()
        << "The assertion that the input ids are sorted only exists in "
           "non-optimized builds. We are executing this test in an "
           "optimized build so we skip it.";
  }

  SparseVectorFilterProto filter;

  SparseVectorFilterPredicate predicate(filter);
  EXPECT_TRUE(predicate.AcceptsAndUpdate(2, 0.0));
  EXPECT_DEATH(predicate.AcceptsAndUpdate(1, 0.0), "strictly increasing");
}

TEST(SparseVectorFilterPredicateDeathTest, DuplicatedInputIds) {
  if (!DEBUG_MODE) {
    GTEST_SKIP()
        << "The assertion that the input ids are sorted only exists in "
           "non-optimized builds. We are executing this test in an "
           "optimized build so we skip it.";
  }

  SparseVectorFilterProto filter;

  SparseVectorFilterPredicate predicate(filter);
  EXPECT_TRUE(predicate.AcceptsAndUpdate(2, 0.0));
  EXPECT_DEATH(predicate.AcceptsAndUpdate(2, 0.0), "strictly increasing");
}

TEST(FilterSparseVectorTest, SimpleFilter) {
  SparseVectorFilterProto filter;
  filter.set_filter_by_ids(true);
  filter.mutable_filtered_ids()->Add(2);
  filter.mutable_filtered_ids()->Add(7);

  SparseDoubleVectorProto vec;
  vec.add_ids(0);
  vec.add_values(10.0);
  vec.add_ids(2);
  vec.add_values(11.0);
  vec.add_ids(5);
  vec.add_values(12.0);
  vec.add_ids(7);
  vec.add_values(13.0);
  vec.add_ids(9);
  vec.add_values(14.0);

  SparseDoubleVectorProto expected;
  expected.add_ids(2);
  expected.add_values(11.0);
  expected.add_ids(7);
  expected.add_values(13.0);

  EXPECT_THAT(FilterSparseVector(vec, filter), EqualsProto(expected));
}

TEST(ApplyAllFiltersTest, MissingSolutions) {
  SolutionProto solution;
  solution.mutable_basis()->set_basic_dual_feasibility(
      SOLUTION_STATUS_FEASIBLE);

  ModelSolveParametersProto model_params;
  model_params.mutable_variable_values_filter()->set_skip_zero_values(true);
  model_params.mutable_dual_values_filter()->set_skip_zero_values(true);
  model_params.mutable_reduced_costs_filter()->set_skip_zero_values(true);

  const SolutionProto expected = solution;

  ApplyAllFilters(model_params, solution);

  EXPECT_THAT(solution, EqualsProto(expected));
}

TEST(ApplyAllFiltersTest, MissingFilters) {
  SolutionProto solution;
  SparseDoubleVectorProto& var_values =
      *solution.mutable_primal_solution()->mutable_variable_values();
  var_values.add_ids(2);
  var_values.add_values(0.0);
  SparseDoubleVectorProto& reduced_costs =
      *solution.mutable_dual_solution()->mutable_reduced_costs();
  reduced_costs.add_ids(2);
  reduced_costs.add_values(1.0);
  SparseDoubleVectorProto& dual_values =
      *solution.mutable_dual_solution()->mutable_dual_values();
  dual_values.add_ids(4);
  dual_values.add_values(3.0);

  ModelSolveParametersProto model_params;

  const SolutionProto expected = solution;

  ApplyAllFilters(model_params, solution);

  EXPECT_THAT(solution, EqualsProto(expected));
}

TEST(ApplyAllFiltersTest, UseFilters) {
  SolutionProto solution;
  {
    SparseDoubleVectorProto& var_values =
        *solution.mutable_primal_solution()->mutable_variable_values();
    var_values.add_ids(2);
    var_values.add_values(0.0);
    var_values.add_ids(4);
    var_values.add_values(1.0);
    SparseDoubleVectorProto& reduced_costs =
        *solution.mutable_dual_solution()->mutable_reduced_costs();
    reduced_costs.add_ids(2);
    reduced_costs.add_values(1.0);
    reduced_costs.add_ids(4);
    reduced_costs.add_values(0.0);
    SparseDoubleVectorProto& dual_values =
        *solution.mutable_dual_solution()->mutable_dual_values();
    dual_values.add_ids(5);
    dual_values.add_values(0.0);
    dual_values.add_ids(7);
    dual_values.add_values(4.0);
  }

  ModelSolveParametersProto model_params;
  model_params.mutable_variable_values_filter()->set_skip_zero_values(true);
  model_params.mutable_dual_values_filter()->set_skip_zero_values(true);
  model_params.mutable_reduced_costs_filter()->set_skip_zero_values(true);

  SolutionProto expected;
  {
    SparseDoubleVectorProto& var_values =
        *expected.mutable_primal_solution()->mutable_variable_values();
    var_values.add_ids(4);
    var_values.add_values(1.0);
    SparseDoubleVectorProto& reduced_costs =
        *expected.mutable_dual_solution()->mutable_reduced_costs();
    reduced_costs.add_ids(2);
    reduced_costs.add_values(1.0);
    SparseDoubleVectorProto& dual_values =
        *expected.mutable_dual_solution()->mutable_dual_values();
    dual_values.add_ids(7);
    dual_values.add_values(4.0);
  }

  ApplyAllFilters(model_params, solution);

  EXPECT_THAT(solution, EqualsProto(expected));
}

TEST(ModelIsSupportedTest, EmptyModel) {
  EXPECT_OK(ModelIsSupported(ModelProto(), {}, "test"));
}

TEST(ModelIsSupportedTest, IntegerVariable) {
  ModelProto model;
  model.mutable_variables()->add_integers(true);
  EXPECT_OK(ModelIsSupported(
      model, {.integer_variables = SupportType::kSupported}, "test"));
  EXPECT_THAT(
      ModelIsSupported(
          model, {.integer_variables = SupportType::kNotImplemented}, "test"),
      StatusIs(absl::StatusCode::kUnimplemented, HasSubstr("integer")));
  EXPECT_THAT(
      ModelIsSupported(model, {.integer_variables = SupportType::kNotSupported},
                       "test"),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("integer")));
}

TEST(UpdateIsSupportedTest, UpdatedIntegerVariable) {
  ModelUpdateProto update;
  update.mutable_variable_updates()->mutable_integers()->add_values(true);
  EXPECT_TRUE(UpdateIsSupported(
      update, {.integer_variables = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.integer_variables = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.integer_variables = SupportType::kNotSupported}));
}

TEST(UpdateIsSupportedTest, NewIntegerVariable) {
  ModelUpdateProto update;
  update.mutable_new_variables()->add_integers(true);
  EXPECT_TRUE(UpdateIsSupported(
      update, {.integer_variables = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.integer_variables = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.integer_variables = SupportType::kNotSupported}));
}

TEST(ModelIsSupportedTest, MultiObjectives) {
  ModelProto model;
  (*model.mutable_auxiliary_objectives())[0] = {};
  (*model.mutable_auxiliary_objectives())[1] = {};
  EXPECT_OK(ModelIsSupported(
      model, {.multi_objectives = SupportType::kSupported}, "test"));
  EXPECT_THAT(
      ModelIsSupported(
          model, {.multi_objectives = SupportType::kNotImplemented}, "test"),
      StatusIs(absl::StatusCode::kUnimplemented,
               HasSubstr("multiple objectives")));
  EXPECT_THAT(
      ModelIsSupported(model, {.multi_objectives = SupportType::kNotSupported},
                       "test"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("multiple objectives")));
}

TEST(UpdateIsSupportedTest, DeletedMultiObjective) {
  ModelUpdateProto update;
  update.mutable_auxiliary_objectives_updates()->add_deleted_objective_ids(1);
  EXPECT_TRUE(
      UpdateIsSupported(update, {.multi_objectives = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kNotSupported}));
}

TEST(UpdateIsSupportedTest, NewMultiObjective) {
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()
        ->mutable_new_objectives())[1] = {};
  EXPECT_TRUE(
      UpdateIsSupported(update, {.multi_objectives = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kNotSupported}));
}

TEST(UpdateIsSupportedTest, UpdatedMultiObjective) {
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()
        ->mutable_objective_updates())[1] = {};
  EXPECT_TRUE(
      UpdateIsSupported(update, {.multi_objectives = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kNotSupported}));
}

TEST(ModelIsSupportedTest, QuadraticObjective) {
  ModelProto model;
  model.mutable_objective()->mutable_quadratic_coefficients()->add_row_ids(1);
  model.mutable_objective()->mutable_quadratic_coefficients()->add_column_ids(
      1);
  model.mutable_objective()->mutable_quadratic_coefficients()->add_coefficients(
      2.0);
  EXPECT_OK(ModelIsSupported(
      model, {.quadratic_objectives = SupportType::kSupported}, "test"));
  EXPECT_THAT(ModelIsSupported(
                  model, {.quadratic_objectives = SupportType::kNotImplemented},
                  "test"),
              StatusIs(absl::StatusCode::kUnimplemented,
                       HasSubstr("quadratic objective")));
  EXPECT_THAT(
      ModelIsSupported(
          model, {.quadratic_objectives = SupportType::kNotSupported}, "test"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("quadratic objective")));
}

TEST(ModelIsSupportedTest, AuxiliaryQuadraticObjective) {
  ModelProto model;
  {
    ObjectiveProto& objective = (*model.mutable_auxiliary_objectives())[0];
    objective.mutable_quadratic_coefficients()->add_row_ids(1);
    objective.mutable_quadratic_coefficients()->add_column_ids(1);
    objective.mutable_quadratic_coefficients()->add_coefficients(2.0);
  }
  EXPECT_OK(ModelIsSupported(model,
                             {.multi_objectives = SupportType::kSupported,
                              .quadratic_objectives = SupportType::kSupported},
                             "test"));
  EXPECT_THAT(
      ModelIsSupported(model,
                       {.multi_objectives = SupportType::kSupported,
                        .quadratic_objectives = SupportType::kNotImplemented},
                       "test"),
      StatusIs(absl::StatusCode::kUnimplemented,
               HasSubstr("quadratic objective")));
  EXPECT_THAT(
      ModelIsSupported(model,
                       {.multi_objectives = SupportType::kSupported,
                        .quadratic_objectives = SupportType::kNotSupported},
                       "test"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("quadratic objective")));
}

TEST(UpdateIsSupportedTest, QuadraticObjective) {
  ModelUpdateProto update;
  update.mutable_objective_updates()
      ->mutable_quadratic_coefficients()
      ->add_row_ids(1);
  update.mutable_objective_updates()
      ->mutable_quadratic_coefficients()
      ->add_column_ids(1);
  update.mutable_objective_updates()
      ->mutable_quadratic_coefficients()
      ->add_coefficients(2.0);
  EXPECT_TRUE(UpdateIsSupported(
      update, {.quadratic_objectives = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.quadratic_objectives = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.quadratic_objectives = SupportType::kNotSupported}));
}

TEST(UpdateIsSupportedTest, NewAuxiliaryQuadraticObjective) {
  ModelUpdateProto update;
  {
    SparseDoubleMatrixProto& coeffs =
        *(*update.mutable_auxiliary_objectives_updates()
               ->mutable_new_objectives())[0]
             .mutable_quadratic_coefficients();
    coeffs.add_row_ids(1);
    coeffs.add_column_ids(1);
    coeffs.add_coefficients(2.0);
  }
  EXPECT_TRUE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kSupported,
               .quadratic_objectives = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kSupported,
               .quadratic_objectives = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kSupported,
               .quadratic_objectives = SupportType::kNotSupported}));
}

TEST(UpdateIsSupportedTest, AuxiliaryQuadraticObjectiveUpdate) {
  ModelUpdateProto update;
  {
    SparseDoubleMatrixProto& coeffs =
        *(*update.mutable_auxiliary_objectives_updates()
               ->mutable_new_objectives())[0]
             .mutable_quadratic_coefficients();
    coeffs.add_row_ids(1);
    coeffs.add_column_ids(1);
    coeffs.add_coefficients(2.0);
  }
  EXPECT_TRUE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kSupported,
               .quadratic_objectives = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kSupported,
               .quadratic_objectives = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.multi_objectives = SupportType::kSupported,
               .quadratic_objectives = SupportType::kNotSupported}));
}

TEST(ModelIsSupportedTest, QuadraticConstraintInModel) {
  ModelProto model;
  (*model.mutable_quadratic_constraints())[1] = {};
  EXPECT_OK(ModelIsSupported(
      model, {.quadratic_constraints = SupportType::kSupported}, "test"));
  EXPECT_THAT(
      ModelIsSupported(model,
                       {.quadratic_constraints = SupportType::kNotImplemented},
                       "test"),
      StatusIs(absl::StatusCode::kUnimplemented,
               HasSubstr("quadratic constraint")));
  EXPECT_THAT(
      ModelIsSupported(
          model, {.quadratic_constraints = SupportType::kNotSupported}, "test"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("quadratic constraint")));
}

TEST(UpdateIsSupportedTest, NewQuadraticConstraint) {
  ModelUpdateProto update;
  (*update.mutable_quadratic_constraint_updates()
        ->mutable_new_constraints())[1] = {};
  EXPECT_TRUE(UpdateIsSupported(
      update, {.quadratic_constraints = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.quadratic_constraints = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.quadratic_constraints = SupportType::kNotSupported}));
}

TEST(UpdateIsSupportedTest, DeletedQuadraticConstraint) {
  ModelUpdateProto update;
  update.mutable_quadratic_constraint_updates()->add_deleted_constraint_ids(1);
  EXPECT_TRUE(UpdateIsSupported(
      update, {.quadratic_constraints = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.quadratic_constraints = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.quadratic_constraints = SupportType::kNotSupported}));
}

TEST(ModelIsSupportedTest, SecondOrderConeConstraintInModel) {
  ModelProto model;
  (*model.mutable_second_order_cone_constraints())[1] = {};
  EXPECT_OK(ModelIsSupported(
      model, {.second_order_cone_constraints = SupportType::kSupported},
      "test"));
  EXPECT_THAT(ModelIsSupported(model,
                               {.second_order_cone_constraints =
                                    SupportType::kNotImplemented},
                               "test"),
              StatusIs(absl::StatusCode::kUnimplemented,
                       HasSubstr("second-order cone constraint")));
  EXPECT_THAT(
      ModelIsSupported(
          model, {.second_order_cone_constraints = SupportType::kNotSupported},
          "test"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("second-order cone constraint")));
}

TEST(UpdateIsSupportedTest, NewSecondOrderConeConstraint) {
  ModelUpdateProto update;
  (*update.mutable_second_order_cone_constraint_updates()
        ->mutable_new_constraints())[1] = {};
  EXPECT_TRUE(UpdateIsSupported(
      update, {.second_order_cone_constraints = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.second_order_cone_constraints = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.second_order_cone_constraints = SupportType::kNotSupported}));
}

TEST(UpdateIsSupportedTest, DeletedSecondOrderConeConstraint) {
  ModelUpdateProto update;
  update.mutable_second_order_cone_constraint_updates()
      ->add_deleted_constraint_ids(1);
  EXPECT_TRUE(UpdateIsSupported(
      update, {.second_order_cone_constraints = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.second_order_cone_constraints = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.second_order_cone_constraints = SupportType::kNotSupported}));
}

TEST(ModelIsSupportedTest, Sos1Constraint) {
  ModelProto model;
  (*model.mutable_sos1_constraints())[1] = {};
  EXPECT_OK(ModelIsSupported(
      model, {.sos1_constraints = SupportType::kSupported}, "test"));
  EXPECT_THAT(
      ModelIsSupported(
          model, {.sos1_constraints = SupportType::kNotImplemented}, "test"),
      StatusIs(absl::StatusCode::kUnimplemented, HasSubstr("sos1 constraint")));
  EXPECT_THAT(
      ModelIsSupported(model, {.sos1_constraints = SupportType::kNotSupported},
                       "test"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("sos1 constraint")));
}

TEST(UpdateIsSupportedTest, NewSos1Constraint) {
  ModelUpdateProto update;
  (*update.mutable_sos1_constraint_updates()->mutable_new_constraints())[1] =
      {};
  EXPECT_TRUE(
      UpdateIsSupported(update, {.sos1_constraints = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.sos1_constraints = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.sos1_constraints = SupportType::kNotSupported}));
}

TEST(UpdateIsSupportedTest, DeletedSos1Constraint) {
  ModelUpdateProto update;
  update.mutable_sos1_constraint_updates()->add_deleted_constraint_ids(1);
  EXPECT_TRUE(
      UpdateIsSupported(update, {.sos1_constraints = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.sos1_constraints = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.sos1_constraints = SupportType::kNotSupported}));
}

TEST(ModelIsSupportedTest, Sos2Constraint) {
  ModelProto model;
  (*model.mutable_sos2_constraints())[1] = {};
  EXPECT_OK(ModelIsSupported(
      model, {.sos2_constraints = SupportType::kSupported}, "test"));
  EXPECT_THAT(
      ModelIsSupported(
          model, {.sos2_constraints = SupportType::kNotImplemented}, "test"),
      StatusIs(absl::StatusCode::kUnimplemented, HasSubstr("sos2 constraint")));
  EXPECT_THAT(
      ModelIsSupported(model, {.sos2_constraints = SupportType::kNotSupported},
                       "test"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("sos2 constraint")));
}

TEST(UpdateIsSupportedTest, NewSos2Constraint) {
  ModelUpdateProto update;
  (*update.mutable_sos2_constraint_updates()->mutable_new_constraints())[1] =
      {};
  EXPECT_TRUE(
      UpdateIsSupported(update, {.sos2_constraints = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.sos2_constraints = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.sos2_constraints = SupportType::kNotSupported}));
}

TEST(UpdateIsSupportedTest, DeletedSos2Constraint) {
  ModelUpdateProto update;
  update.mutable_sos2_constraint_updates()->add_deleted_constraint_ids(1);
  EXPECT_TRUE(
      UpdateIsSupported(update, {.sos2_constraints = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.sos2_constraints = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.sos2_constraints = SupportType::kNotSupported}));
}

TEST(ModelIsSupportedTest, IndicatorConstraint) {
  ModelProto model;
  (*model.mutable_indicator_constraints())[1] = {};
  EXPECT_OK(ModelIsSupported(
      model, {.indicator_constraints = SupportType::kSupported}, "test"));
  EXPECT_THAT(
      ModelIsSupported(model,
                       {.indicator_constraints = SupportType::kNotImplemented},
                       "test"),
      StatusIs(absl::StatusCode::kUnimplemented,
               HasSubstr("indicator constraint")));
  EXPECT_THAT(
      ModelIsSupported(
          model, {.indicator_constraints = SupportType::kNotSupported}, "test"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("indicator constraint")));
}

TEST(UpdateIsSupportedTest, NewIndicatorConstraint) {
  ModelUpdateProto update;
  (*update.mutable_indicator_constraint_updates()
        ->mutable_new_constraints())[1] = {};
  EXPECT_TRUE(UpdateIsSupported(
      update, {.indicator_constraints = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.indicator_constraints = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.indicator_constraints = SupportType::kNotSupported}));
}

TEST(UpdateIsSupportedTest, DeletedIndicatorConstraint) {
  ModelUpdateProto update;
  update.mutable_indicator_constraint_updates()->add_deleted_constraint_ids(1);
  EXPECT_TRUE(UpdateIsSupported(
      update, {.indicator_constraints = SupportType::kSupported}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.indicator_constraints = SupportType::kNotImplemented}));
  EXPECT_FALSE(UpdateIsSupported(
      update, {.indicator_constraints = SupportType::kNotSupported}));
}

TEST(ModelSolveParametersAreSupportedTest, PrimaryObjectiveParameters) {
  ModelSolveParametersProto model_parameters;
  model_parameters.mutable_primary_objective_parameters();
  EXPECT_OK(ModelSolveParametersAreSupported(
      model_parameters, {.multi_objectives = SupportType::kSupported}, "test"));
  EXPECT_THAT(ModelSolveParametersAreSupported(
                  model_parameters,
                  {.multi_objectives = SupportType::kNotImplemented}, "test"),
              StatusIs(absl::StatusCode::kUnimplemented,
                       HasSubstr("primary_objective_parameters")));
  EXPECT_THAT(ModelSolveParametersAreSupported(
                  model_parameters,
                  {.multi_objectives = SupportType::kNotSupported}, "test"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("primary_objective_parameters")));
}

TEST(ModelSolveParametersAreSupportedTest, AuxiliaryObjectiveParameters) {
  ModelSolveParametersProto model_parameters;
  (*model_parameters.mutable_auxiliary_objective_parameters())[2];
  EXPECT_OK(ModelSolveParametersAreSupported(
      model_parameters, {.multi_objectives = SupportType::kSupported}, "test"));
  EXPECT_THAT(ModelSolveParametersAreSupported(
                  model_parameters,
                  {.multi_objectives = SupportType::kNotImplemented}, "test"),
              StatusIs(absl::StatusCode::kUnimplemented,
                       HasSubstr("auxiliary_objective_parameters")));
  EXPECT_THAT(ModelSolveParametersAreSupported(
                  model_parameters,
                  {.multi_objectives = SupportType::kNotSupported}, "test"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("auxiliary_objective_parameters")));
}

TEST(UpgradeSolveResultProtoForStatsMigrationTest, HasOldMessages) {
  SolveResultProto actual;
  actual.mutable_solve_stats()->set_best_primal_bound(-kInf);
  actual.mutable_solve_stats()->set_best_dual_bound(kInf);
  actual.mutable_solve_stats()->mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_INFEASIBLE);
  actual.mutable_solve_stats()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_UNDETERMINED);

  SolveResultProto expected = actual;
  expected.mutable_termination()->mutable_objective_bounds()->set_primal_bound(
      -kInf);
  expected.mutable_termination()->mutable_objective_bounds()->set_dual_bound(
      kInf);
  expected.mutable_termination()->mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_INFEASIBLE);
  expected.mutable_termination()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_UNDETERMINED);

  UpgradeSolveResultProtoForStatsMigration(actual);
  EXPECT_THAT(actual, EquivToProto(expected));
}

TEST(UpgradeSolveResultProtoForStatsMigrationTest, HasNewMessages) {
  SolveResultProto actual;
  actual.mutable_termination()->mutable_objective_bounds()->set_primal_bound(
      -kInf);
  actual.mutable_termination()->mutable_objective_bounds()->set_dual_bound(
      kInf);
  actual.mutable_termination()->mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_INFEASIBLE);
  actual.mutable_termination()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_UNDETERMINED);

  SolveResultProto expected = actual;
  expected.mutable_solve_stats()->set_best_primal_bound(-kInf);
  expected.mutable_solve_stats()->set_best_dual_bound(kInf);
  expected.mutable_solve_stats()->mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_INFEASIBLE);
  expected.mutable_solve_stats()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_UNDETERMINED);

  UpgradeSolveResultProtoForStatsMigration(actual);
  EXPECT_THAT(actual, EquivToProto(expected));
}

TEST(UpgradeSolveResultProtoForStatsMigrationTest,
     HasBothOldAndNewMessagesNewWins) {
  SolveResultProto actual;
  actual.mutable_solve_stats()->set_best_primal_bound(-kInf);
  actual.mutable_solve_stats()->set_best_dual_bound(kInf);
  actual.mutable_solve_stats()->mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_UNDETERMINED);
  actual.mutable_solve_stats()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_UNDETERMINED);

  actual.mutable_termination()->mutable_objective_bounds()->set_primal_bound(
      20);
  actual.mutable_termination()->mutable_objective_bounds()->set_dual_bound(10);
  actual.mutable_termination()->mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_FEASIBLE);
  actual.mutable_termination()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_FEASIBLE);

  SolveResultProto expected = actual;
  expected.mutable_solve_stats()->set_best_primal_bound(20);
  expected.mutable_solve_stats()->set_best_dual_bound(10);
  *expected.mutable_solve_stats()->mutable_problem_status() =
      expected.termination().problem_status();

  UpgradeSolveResultProtoForStatsMigration(actual);
  EXPECT_THAT(actual, EquivToProto(expected));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
