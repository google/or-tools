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

#include "ortools/math_opt/cpp/solve_result.h"

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/macros/os_support.h"
#include "ortools/base/protoutil.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/cpp/enums_testing.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/objective.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/testing/stream.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/util/fp_roundtrip_conv_testing.h"

namespace operations_research {
namespace math_opt {
namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

INSTANTIATE_TYPED_TEST_SUITE_P(TerminationReason, EnumTest, TerminationReason);
INSTANTIATE_TYPED_TEST_SUITE_P(Limit, EnumTest, Limit);
INSTANTIATE_TYPED_TEST_SUITE_P(FeasibilityStatus, EnumTest, FeasibilityStatus);

TEST(ObjectiveBounds, StringTest) {
  const ObjectiveBounds objective_bounds = {.primal_bound = 1.0,
                                            .dual_bound = 2.0};
  std::string expected = "{primal_bound: 1, dual_bound: 2}";
  EXPECT_EQ(objective_bounds.ToString(), expected);
  EXPECT_EQ(StreamToString(objective_bounds), expected);
}

TEST(ObjectiveBounds, FloatingPointRoundTripPrimalBound) {
  const ObjectiveBounds objective_bounds = {
      .primal_bound = kRoundTripTestNumber, .dual_bound = kInf};
  EXPECT_THAT(
      objective_bounds.ToString(),
      HasSubstr(absl::StrCat("primal_bound: ", kRoundTripTestNumberStr)));
}

TEST(ObjectiveBounds, FloatingPointRoundTripDualBound) {
  const ObjectiveBounds objective_bounds = {.primal_bound = -kInf,
                                            .dual_bound = kRoundTripTestNumber};
  EXPECT_THAT(objective_bounds.ToString(),
              HasSubstr(absl::StrCat("dual_bound: ", kRoundTripTestNumberStr)));
}

TEST(ObjectiveBounds, ProtoRoundTripTest) {
  const ObjectiveBounds objective_bounds = {.primal_bound = 10,
                                            .dual_bound = 20};
  const ObjectiveBounds actual =
      ObjectiveBounds::FromProto(objective_bounds.Proto());
  EXPECT_EQ(actual.primal_bound, 10);
  EXPECT_EQ(actual.dual_bound, 20);
}

TEST(ObjectiveBounds, MakeTrivial) {
  EXPECT_THAT(ObjectiveBounds::MaximizeMakeTrivial(),
              ObjectiveBoundsNear(
                  ObjectiveBounds{.primal_bound = -kInf, .dual_bound = kInf}));
  EXPECT_THAT(ObjectiveBounds::MinimizeMakeTrivial(),
              ObjectiveBoundsNear(
                  ObjectiveBounds{.primal_bound = kInf, .dual_bound = -kInf}));
}

TEST(ObjectiveBounds, MakeTrivialIsMaximize) {
  EXPECT_THAT(ObjectiveBounds::MakeTrivial(/*is_maximize=*/true),
              ObjectiveBoundsNear(
                  ObjectiveBounds{.primal_bound = -kInf, .dual_bound = kInf}));
  EXPECT_THAT(ObjectiveBounds::MakeTrivial(/*is_maximize=*/false),
              ObjectiveBoundsNear(
                  ObjectiveBounds{.primal_bound = kInf, .dual_bound = -kInf}));
}

TEST(ObjectiveBounds, MakeUnbounded) {
  EXPECT_THAT(ObjectiveBounds::MaximizeMakeUnbounded(),
              ObjectiveBoundsNear(
                  ObjectiveBounds{.primal_bound = kInf, .dual_bound = kInf}));
  EXPECT_THAT(ObjectiveBounds::MinimizeMakeUnbounded(),
              ObjectiveBoundsNear(
                  ObjectiveBounds{.primal_bound = -kInf, .dual_bound = -kInf}));
}

TEST(ObjectiveBounds, MakeUnboundedIsMaximize) {
  EXPECT_THAT(ObjectiveBounds::MakeUnbounded(/*is_maximize=*/true),
              ObjectiveBoundsNear(
                  ObjectiveBounds{.primal_bound = kInf, .dual_bound = kInf}));
  EXPECT_THAT(ObjectiveBounds::MakeUnbounded(/*is_maximize=*/false),
              ObjectiveBoundsNear(
                  ObjectiveBounds{.primal_bound = -kInf, .dual_bound = -kInf}));
}

TEST(ObjectiveBounds, MakeOptimal) {
  EXPECT_THAT(ObjectiveBounds::MakeOptimal(/*objective_value=*/10.0),
              ObjectiveBoundsNear(
                  ObjectiveBounds{.primal_bound = 10.0, .dual_bound = 10.0}));
}

TEST(ProblemStatusTest, ProtoToProblemStatus) {
  ProblemStatusProto proto;
  proto.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  proto.set_dual_status(FEASIBILITY_STATUS_FEASIBLE);
  proto.set_primal_or_dual_infeasible(true);

  ASSERT_OK_AND_ASSIGN(const ProblemStatus actual_status,
                       ProblemStatus::FromProto(proto));
  EXPECT_EQ(actual_status.primal_status, FeasibilityStatus::kUndetermined);
  EXPECT_EQ(actual_status.dual_status, FeasibilityStatus::kFeasible);
  EXPECT_EQ(actual_status.primal_or_dual_infeasible, true);
}

TEST(ProblemStatusTest, ProblemStatusToProto) {
  const ProblemStatus problem_status = {
      .primal_status = FeasibilityStatus::kUndetermined,
      .dual_status = FeasibilityStatus::kFeasible,
      .primal_or_dual_infeasible = true};
  const ProblemStatusProto proto = problem_status.Proto();

  EXPECT_EQ(proto.primal_status(), FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_EQ(proto.dual_status(), FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_EQ(proto.primal_or_dual_infeasible(), true);
}

TEST(ProblemStatusTest, FromProtoInvalidInputDualStatus) {
  ProblemStatusProto proto;
  proto.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_THAT(
      ProblemStatus::FromProto(proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("dual_status")));
}

TEST(ProblemStatusTest, FromProtoInvalidInputPrimalStatus) {
  ProblemStatusProto proto;
  proto.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_THAT(
      ProblemStatus::FromProto(proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("primal_status")));
}

TEST(ProblemStatusTest, StringTest) {
  const ProblemStatus problem_status = {
      .primal_status = FeasibilityStatus::kInfeasible,
      .dual_status = FeasibilityStatus::kFeasible,
      .primal_or_dual_infeasible = false};

  std::string expected =
      "{primal_status: infeasible, dual_status: feasible, "
      "primal_or_dual_infeasible: false}";
  EXPECT_EQ(problem_status.ToString(), expected);
  EXPECT_EQ(StreamToString(problem_status), expected);
}

TEST(SolveStats, ProtoRoundTripTest) {
  const SolveStats solve_stats = {.solve_time = absl::Seconds(1),
                                  .simplex_iterations = 1,
                                  .barrier_iterations = 2,
                                  .first_order_iterations = 3,
                                  .node_count = 4};
  ASSERT_OK_AND_ASSIGN(const SolveStatsProto proto, solve_stats.Proto());
  ASSERT_OK_AND_ASSIGN(const SolveStats actual, SolveStats::FromProto(proto));
  EXPECT_EQ(actual.solve_time, absl::Seconds(1));
  EXPECT_EQ(actual.simplex_iterations, 1);
  EXPECT_EQ(actual.barrier_iterations, 2);
  EXPECT_EQ(actual.first_order_iterations, 3);
  EXPECT_EQ(actual.node_count, 4);
}

TEST(SolveStats, ToProtoInvalidSolveTime) {
  const SolveStats solve_stats = {.solve_time = absl::InfiniteDuration()};
  EXPECT_THAT(solve_stats.Proto(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("solve_time"), HasSubstr("finite"))));
}

TEST(SolveStats, FromProtoInvalidSolveTime) {
  SolveStatsProto stats;
  stats.mutable_solve_time()->set_nanos(-1);
  stats.mutable_solve_time()->set_seconds(1);
  EXPECT_THAT(
      SolveStats::FromProto(stats),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("solve_time")));
}

TEST(SolveStats, StringTest) {
  const SolveStats solve_stats = {.solve_time = absl::Seconds(1),
                                  .simplex_iterations = 1,
                                  .barrier_iterations = 2,
                                  .first_order_iterations = 3,
                                  .node_count = 4};

  std::string expected =
      "{solve_time: 1s, simplex_iterations: 1, "
      "barrier_iterations: 2, first_order_iterations: 3, node_count: 4}";
  EXPECT_EQ(solve_stats.ToString(), expected);
  EXPECT_EQ(StreamToString(solve_stats), expected);
}

TEST(TerminationTest, ProtoRoundTripTestForFeasible) {
  const Termination termination = Termination::Feasible(
      /*is_maximize=*/true, Limit::kTime, 10.0, 20.0, "hit time limit");
  ASSERT_OK_AND_ASSIGN(const Termination actual,
                       Termination::FromProto(termination.Proto()));
  EXPECT_EQ(actual.reason, TerminationReason::kFeasible);
  EXPECT_EQ(actual.limit, Limit::kTime);
  EXPECT_EQ(actual.detail, "hit time limit");
  EXPECT_EQ(actual.problem_status.primal_status, FeasibilityStatus::kFeasible);
  EXPECT_EQ(actual.problem_status.dual_status, FeasibilityStatus::kFeasible);
  EXPECT_FALSE(actual.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(actual.objective_bounds,
              ObjectiveBoundsNear({.primal_bound = 10.0, .dual_bound = 20.0}));
}

TEST(TerminationTest, InvalidUnspecifiedTerminationReason) {
  TerminationProto termination;
  EXPECT_THAT(
      Termination::FromProto(termination),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("reason")));
}

TEST(TerminationTest, OptimalTwoArgument) {
  const Termination termination = Termination::Optimal(10.0, 20.0, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kOptimal);
  EXPECT_EQ(termination.limit, std::nullopt);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kFeasible);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kFeasible);
  EXPECT_FALSE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear({.primal_bound = 10.0, .dual_bound = 20.0}));
}

TEST(TerminationTest, OptimalOneArgument) {
  const Termination termination = Termination::Optimal(10.0, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kOptimal);
  EXPECT_EQ(termination.limit, std::nullopt);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kFeasible);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kFeasible);
  EXPECT_FALSE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear({.primal_bound = 10.0, .dual_bound = 10.0}));
}

class TerminationMinMaxTest : public ::testing::TestWithParam<bool> {};
INSTANTIATE_TEST_SUITE_P(
    TerminationMinMaxTests, TerminationMinMaxTest, testing::Values(true, false),
    [](const testing::TestParamInfo<TerminationMinMaxTest::ParamType>& info) {
      return info.param ? "max" : "min";
    });

TEST_P(TerminationMinMaxTest, InfeasibleDualNotFeasible) {
  const bool is_maximize = GetParam();
  const ObjectiveBounds expected_bounds =
      ObjectiveBounds::MakeTrivial(is_maximize);
  const Termination termination = Termination::Infeasible(
      is_maximize, FeasibilityStatus::kUndetermined, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kInfeasible);
  EXPECT_EQ(termination.limit, std::nullopt);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kInfeasible);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kUndetermined);
  EXPECT_FALSE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear(expected_bounds));
}

TEST_P(TerminationMinMaxTest, InfeasibleDualFeasible) {
  const bool is_maximize = GetParam();
  const double bound = is_maximize ? -kInf : kInf;
  const ObjectiveBounds expected_bounds = {.primal_bound = bound,
                                           .dual_bound = bound};
  const Termination termination = Termination::Infeasible(
      is_maximize, FeasibilityStatus::kFeasible, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kInfeasible);
  EXPECT_EQ(termination.limit, std::nullopt);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kInfeasible);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kFeasible);
  EXPECT_FALSE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear(expected_bounds));
}

TEST_P(TerminationMinMaxTest, InfeasibleOrUnboundedDualNotInfeasible) {
  const bool is_maximize = GetParam();
  const ObjectiveBounds expected_bounds =
      ObjectiveBounds::MakeTrivial(is_maximize);
  const Termination termination = Termination::InfeasibleOrUnbounded(
      is_maximize, FeasibilityStatus::kUndetermined, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kInfeasibleOrUnbounded);
  EXPECT_EQ(termination.limit, std::nullopt);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kUndetermined);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kUndetermined);
  EXPECT_TRUE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear(expected_bounds));
}

TEST_P(TerminationMinMaxTest, InfeasibleOrUnboundedDualInfeasible) {
  const bool is_maximize = GetParam();
  const ObjectiveBounds expected_bounds =
      ObjectiveBounds::MakeTrivial(is_maximize);
  const Termination termination = Termination::InfeasibleOrUnbounded(
      is_maximize, FeasibilityStatus::kInfeasible, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kInfeasibleOrUnbounded);
  EXPECT_EQ(termination.limit, std::nullopt);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kUndetermined);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kInfeasible);
  EXPECT_FALSE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear(expected_bounds));
}

TEST_P(TerminationMinMaxTest, Unbounded) {
  const bool is_maximize = GetParam();
  const ObjectiveBounds expected_bounds =
      ObjectiveBounds::MakeUnbounded(is_maximize);
  const Termination termination = Termination::Unbounded(is_maximize, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kUnbounded);
  EXPECT_EQ(termination.limit, std::nullopt);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kFeasible);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kInfeasible);
  EXPECT_FALSE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear(expected_bounds));
}

TEST_P(TerminationMinMaxTest, NoSolutionFoundDualFeasible) {
  const bool is_maximize = GetParam();
  const double dual_bound = 20.0;
  ObjectiveBounds expected_bounds = ObjectiveBounds::MakeTrivial(is_maximize);
  expected_bounds.dual_bound = dual_bound;
  const Termination termination = Termination::NoSolutionFound(
      is_maximize, Limit::kTime, dual_bound, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kNoSolutionFound);
  EXPECT_EQ(termination.limit, Limit::kTime);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kUndetermined);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kFeasible);
  EXPECT_FALSE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear(expected_bounds));
}

TEST_P(TerminationMinMaxTest, NoSolutionFoundDualNotfeasible) {
  const bool is_maximize = GetParam();
  const ObjectiveBounds expected_bounds =
      ObjectiveBounds::MakeTrivial(is_maximize);
  const Termination termination = Termination::NoSolutionFound(
      is_maximize, Limit::kTime, std::nullopt, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kNoSolutionFound);
  EXPECT_EQ(termination.limit, Limit::kTime);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kUndetermined);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kUndetermined);
  EXPECT_FALSE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear(expected_bounds));
}

TEST_P(TerminationMinMaxTest, FeasibleDualFeasible) {
  const bool is_maximize = GetParam();
  const double primal_bound = is_maximize ? 10.0 : 20.0;
  ObjectiveBounds expected_bounds = ObjectiveBounds::MakeTrivial(is_maximize);
  expected_bounds.primal_bound = primal_bound;

  const Termination termination = Termination::Feasible(
      is_maximize, Limit::kTime, primal_bound, std::nullopt, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kFeasible);
  EXPECT_EQ(termination.limit, Limit::kTime);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kFeasible);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kUndetermined);
  EXPECT_FALSE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear(expected_bounds));
}

TEST_P(TerminationMinMaxTest, Cutoff) {
  const bool is_maximize = GetParam();
  const ObjectiveBounds expected_bounds =
      ObjectiveBounds::MakeTrivial(is_maximize);
  const Termination termination = Termination::Cutoff(is_maximize, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kNoSolutionFound);
  EXPECT_EQ(termination.limit, Limit::kCutoff);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kUndetermined);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kUndetermined);
  EXPECT_FALSE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear(expected_bounds));
}

TEST_P(TerminationMinMaxTest, FeasibleDualNotFeasible) {
  const bool is_maximize = GetParam();
  const double primal_bound = is_maximize ? 10.0 : 20.0;
  const double dual_bound = is_maximize ? 20.0 : 10.0;
  const ObjectiveBounds expected_bounds = {.primal_bound = primal_bound,
                                           .dual_bound = dual_bound};
  const Termination termination = Termination::Feasible(
      is_maximize, Limit::kTime, primal_bound, dual_bound, "detail");
  EXPECT_EQ(termination.reason, TerminationReason::kFeasible);
  EXPECT_EQ(termination.limit, Limit::kTime);
  EXPECT_EQ(termination.detail, "detail");
  EXPECT_EQ(termination.problem_status.primal_status,
            FeasibilityStatus::kFeasible);
  EXPECT_EQ(termination.problem_status.dual_status,
            FeasibilityStatus::kFeasible);
  EXPECT_FALSE(termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(termination.objective_bounds,
              ObjectiveBoundsNear(expected_bounds));
}

TEST(TerminationTest, StringTestWithLimit) {
  const Termination termination = Termination::Feasible(
      /*is_maximize=*/true, Limit::kTime, 10.0, 20.0, "hit time limit");
  std::string expected =
      "{reason: feasible, limit: time, detail: \"hit time limit\", "
      "problem_status: "
      "{primal_status: feasible, dual_status: feasible, "
      "primal_or_dual_infeasible: false}, objective_bounds: {primal_bound: "
      "10, "
      "dual_bound: 20}}";
  EXPECT_EQ(termination.ToString(), expected);
  EXPECT_EQ(StreamToString(termination), expected);
  EXPECT_EQ(absl::StrFormat("%v", termination), expected);
}

TEST(TerminationTest, StringTestNoLimit) {
  const Termination termination = Termination::Optimal(10.0, "good answer");
  std::string expected =
      "{reason: optimal, detail: \"good answer\", problem_status: "
      "{primal_status: "
      "feasible, dual_status: feasible, primal_or_dual_infeasible: false}, "
      "objective_bounds: {primal_bound: 10, dual_bound: 10}}";
  EXPECT_EQ(termination.ToString(), expected);
  EXPECT_EQ(StreamToString(termination), expected);
  EXPECT_EQ(absl::StrFormat("%v", termination), expected);
}

TEST(TerminationTest, StringTestNoDetail) {
  const Termination termination = Termination::Optimal(10.0);
  std::string expected =
      "{reason: optimal, problem_status: {primal_status: feasible, "
      "dual_status: feasible, primal_or_dual_infeasible: false}, "
      "objective_bounds: {primal_bound: 10, dual_bound: 10}}";
  EXPECT_EQ(termination.ToString(), expected);
  EXPECT_EQ(StreamToString(termination), expected);
  EXPECT_EQ(absl::StrFormat("%v", termination), expected);
}

TEST(TerminationTest, StringTestDetailEscaping) {
  const Termination termination =
      Termination::Optimal(10.0, "hit \"3\" seconds");
  std::string expected =
      "{reason: optimal, detail: \"hit \\\"3\\\" seconds\", problem_status: "
      "{primal_status: feasible, "
      "dual_status: feasible, primal_or_dual_infeasible: false}, "
      "objective_bounds: {primal_bound: 10, dual_bound: 10}}";
  EXPECT_EQ(termination.ToString(), expected);
  EXPECT_EQ(StreamToString(termination), expected);
  EXPECT_EQ(absl::StrFormat("%v", termination), expected);
}

TEST(TerminationTest, LimitReached) {
  const Termination termination_optimal = Termination::Optimal(10.0);
  EXPECT_FALSE(termination_optimal.limit_reached());
  const Termination termination_feasible = Termination::Feasible(
      /*is_maximize=*/true, Limit::kTime, 10.0);
  EXPECT_TRUE(termination_feasible.limit_reached());
  const Termination termination_nosolution = Termination::NoSolutionFound(
      /*is_maximize=*/true, Limit::kTime);
  EXPECT_TRUE(termination_nosolution.limit_reached());
}

TEST(TerminationTest, EnsureReasonIs) {
  EXPECT_OK(Termination(/*is_maximize=*/true, TerminationReason::kImprecise)
                .EnsureReasonIs(TerminationReason::kImprecise));
  EXPECT_THAT(Termination(/*is_maximize=*/true, TerminationReason::kFeasible)
                  .EnsureReasonIs(TerminationReason::kOptimal),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("expected termination reason 'optimal' but "
                                 "got {reason: feasible")));
}

TEST(TerminationTest, EnsureReasonIsAnyOf) {
  EXPECT_OK(Termination(/*is_maximize=*/true, TerminationReason::kImprecise)
                .EnsureReasonIsAnyOf({TerminationReason::kImprecise,
                                      TerminationReason::kInfeasible}));
  EXPECT_THAT(Termination(/*is_maximize=*/true, TerminationReason::kInfeasible)
                  .EnsureReasonIsAnyOf({TerminationReason::kOptimal,
                                        TerminationReason::kFeasible}),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("expected termination reason in {'optimal', "
                                 "'feasible'} but got {reason: infeasible")));
}

TEST(TerminationTest, EnsureIsOptimal) {
  EXPECT_OK(Termination::Optimal(10.0).EnsureIsOptimal());
  EXPECT_THAT(Termination::Feasible(
                  /*is_maximize=*/true, Limit::kTime, 10.0)
                  .EnsureIsOptimal(),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("expected termination reason 'optimal' but "
                                 "got {reason: feasible")));
}

TEST(TerminationTest, IsOptimal) {
  EXPECT_TRUE(Termination::Optimal(10.0).IsOptimal());
  EXPECT_FALSE(Termination::Feasible(
                   /*is_maximize=*/true, Limit::kTime, 10.0)
                   .IsOptimal());
}

TEST(TerminationTest, EnsureIsOptimalOrFeasible) {
  EXPECT_OK(Termination::Optimal(10.0).EnsureIsOptimalOrFeasible());
  EXPECT_OK(Termination::Feasible(
                /*is_maximize=*/true, Limit::kTime, 10.0)
                .EnsureIsOptimalOrFeasible());
  EXPECT_THAT(Termination::Infeasible(
                  /*is_maximize=*/true, FeasibilityStatus::kInfeasible)
                  .EnsureIsOptimalOrFeasible(),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("expected termination reason in {'optimal', "
                                 "'feasible'} but got {reason: infeasible")));
}

TEST(TerminationTest, IsOptimalOrFeasible) {
  EXPECT_TRUE(Termination::Optimal(10.0).IsOptimalOrFeasible());
  EXPECT_TRUE(Termination::Feasible(
                  /*is_maximize=*/true, Limit::kTime, 10.0)
                  .IsOptimalOrFeasible());
  EXPECT_FALSE(Termination::Infeasible(
                   /*is_maximize=*/true, FeasibilityStatus::kInfeasible)
                   .IsOptimalOrFeasible());
}

SolveResultProto MinimalSolveResultProto() {
  SolveResultProto proto;
  *proto.mutable_termination() = OptimalTerminationProto(10.0, 10.0);
  return proto;
}

SolveResultMatcherOptions StrictCheck() {
  return {.tolerance = 0,
          .first_solution_only = false,
          .check_dual = true,
          .check_rays = true,
          .check_solutions_if_inf_or_unbounded = true,
          .check_basis = true,
          .inf_or_unb_soft_match = false};
}

TEST(SolveResultTest, MinimalSolveResultRoundTrips) {
  ModelStorage model;
  SolveResult solve_result(Termination::Optimal(10.0));

  // Half trip
  EXPECT_THAT(SolveResult::FromProto(&model, MinimalSolveResultProto()),
              IsOkAndHolds(IsConsistentWith(solve_result, StrictCheck())));

  ASSERT_OK_AND_ASSIGN(const SolveResultProto proto, solve_result.Proto());
  // Full trip
  EXPECT_THAT(SolveResult::FromProto(&model, proto),
              IsOkAndHolds(IsConsistentWith(solve_result, StrictCheck())));
}

TEST(SolveResultTest, ProblemStatusInStatsButNotTermination) {
  ModelStorage model;
  SolveResultProto proto = MinimalSolveResultProto();
  *proto.mutable_solve_stats()->mutable_problem_status() =
      proto.termination().problem_status();
  proto.mutable_termination()->clear_problem_status();

  ASSERT_OK_AND_ASSIGN(const SolveResult solve_result,
                       SolveResult::FromProto(&model, proto));
  EXPECT_EQ(solve_result.termination.problem_status.primal_status,
            FeasibilityStatus::kFeasible);
  EXPECT_EQ(solve_result.termination.problem_status.dual_status,
            FeasibilityStatus::kFeasible);
  EXPECT_FALSE(
      solve_result.termination.problem_status.primal_or_dual_infeasible);
  EXPECT_THAT(SolveResult::FromProto(&model, proto),
              IsOkAndHolds(IsConsistentWith(solve_result, StrictCheck())));
}

TEST(SolveResultTest, ObjectiveBoundsInStatsButNotTermination) {
  ModelStorage model;
  SolveResultProto proto = MinimalSolveResultProto();
  proto.mutable_solve_stats()->set_best_primal_bound(10.0);
  proto.mutable_solve_stats()->set_best_dual_bound(20.0);
  proto.mutable_termination()->clear_objective_bounds();

  ASSERT_OK_AND_ASSIGN(const SolveResult solve_result,
                       SolveResult::FromProto(&model, proto));
  EXPECT_DOUBLE_EQ(solve_result.termination.objective_bounds.primal_bound,
                   10.0);
  EXPECT_DOUBLE_EQ(solve_result.termination.objective_bounds.dual_bound, 20.0);
  EXPECT_THAT(SolveResult::FromProto(&model, proto),
              IsOkAndHolds(IsConsistentWith(solve_result, StrictCheck())));
}

TEST(SolveResultTest, SolveTimeRoundTrip) {
  ModelStorage model;
  const absl::Duration time = absl::Seconds(10);
  SolveResultProto result_proto = MinimalSolveResultProto();
  ASSERT_OK(util_time::EncodeGoogleApiProto(
      time, result_proto.mutable_solve_stats()->mutable_solve_time()));
  ASSERT_OK_AND_ASSIGN(const auto result,
                       SolveResult::FromProto(&model, result_proto));
  EXPECT_EQ(result.solve_time(), time);
}

TEST(SolveResultTest, ProtoInvalidSolveStats) {
  SolveResult solve_result(Termination::Optimal(10.0));
  solve_result.solve_stats.solve_time = absl::InfiniteDuration();
  EXPECT_THAT(solve_result.Proto(),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(SolveResultTest, FromProtoInvalidTermination) {
  ModelStorage model;
  SolveResultProto result = MinimalSolveResultProto();
  result.clear_termination();
  EXPECT_THAT(SolveResult::FromProto(&model, result),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(SolveResultTest, FromProtoInvalidStats) {
  ModelStorage model;
  SolveResultProto result = MinimalSolveResultProto();
  result.mutable_solve_stats()->mutable_solve_time()->set_nanos(-1);
  result.mutable_solve_stats()->mutable_solve_time()->set_seconds(1);
  EXPECT_THAT(SolveResult::FromProto(&model, result),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(SolveResultTest, FromProtoInvalidSolution) {
  ModelStorage model;
  SolveResultProto result = MinimalSolveResultProto();
  // default primal solution is invalid, no
  result.add_solutions()
      ->mutable_primal_solution()
      ->mutable_variable_values()
      ->add_values(0);
  EXPECT_THAT(SolveResult::FromProto(&model, result),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(SolveResultTest, FromProtoInvalidPrimalRay) {
  ModelStorage model;
  SolveResultProto result = MinimalSolveResultProto();
  // default primal solution is invalid, no
  result.add_primal_rays()->mutable_variable_values()->add_values(0);
  EXPECT_THAT(SolveResult::FromProto(&model, result),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(SolveResultTest, FromProtoInvalidDualRay) {
  ModelStorage model;
  SolveResultProto result = MinimalSolveResultProto();
  // default primal solution is invalid, no
  result.add_dual_rays()->mutable_reduced_costs()->add_values(0);
  EXPECT_THAT(SolveResult::FromProto(&model, result),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

class SolveResultModelTest : public testing::Test {
 protected:
  SolveResultModelTest()
      : x_(&storage_, storage_.AddVariable("x")),
        c_(&storage_, storage_.AddLinearConstraint("c")) {}

  ModelStorage storage_;
  Variable x_;
  LinearConstraint c_;
};

TEST_F(SolveResultModelTest, RoundTripSolutions) {
  SolveResult result(Termination::Optimal(10.0));
  Solution s1;
  s1.primal_solution = {.variable_values = {{x_, 1.0}},
                        .objective_value = 1.0,
                        .feasibility_status = SolutionStatus::kFeasible};
  result.solutions.push_back(s1);
  Solution s2;
  s2.primal_solution = {.variable_values = {{x_, 0.0}},
                        .objective_value = 0.0,
                        .feasibility_status = SolutionStatus::kFeasible};

  ASSERT_OK_AND_ASSIGN(const SolveResultProto proto, result.Proto());
  EXPECT_THAT(SolveResult::FromProto(&storage_, proto),
              IsOkAndHolds(IsConsistentWith(result, StrictCheck())));
}

TEST_F(SolveResultModelTest, RoundTripPrimalRays) {
  SolveResult result(Termination::Unbounded(/*is_maximize=*/true));
  result.primal_rays.push_back({.variable_values = {{x_, 1.0}}});
  result.primal_rays.push_back({.variable_values = {{x_, 0.0}}});
  ASSERT_OK_AND_ASSIGN(const SolveResultProto proto, result.Proto());
  EXPECT_THAT(SolveResult::FromProto(&storage_, proto),
              IsOkAndHolds(IsConsistentWith(result, StrictCheck())));
}

TEST_F(SolveResultModelTest, RoundTripDualRays) {
  SolveResult result(Termination::Infeasible(/*is_maximize=*/true,
                                             FeasibilityStatus::kInfeasible));
  result.dual_rays.push_back(
      {.dual_values = {{c_, 2.0}}, .reduced_costs = {{x_, 1.0}}});
  result.dual_rays.push_back(
      {.dual_values = {{c_, 3.0}}, .reduced_costs = {{x_, 0.0}}});
  ASSERT_OK_AND_ASSIGN(const SolveResultProto proto, result.Proto());
  EXPECT_THAT(SolveResult::FromProto(&storage_, proto),
              IsOkAndHolds(IsConsistentWith(result, StrictCheck())));
}

TEST(SolveResultTest, RoundTripGscipOutput) {
  ModelStorage model;
  SolveResult solve_result(Termination::Optimal(10.0));
  solve_result.gscip_solver_specific_output.set_status_detail("gscip_detail");
  ASSERT_OK_AND_ASSIGN(const SolveResultProto proto, solve_result.Proto());
  ASSERT_OK_AND_ASSIGN(const SolveResult round_trip,
                       SolveResult::FromProto(&model, proto));

  // Full trip
  EXPECT_THAT(round_trip, IsConsistentWith(solve_result, StrictCheck()));
  // Proto matcher is not portable
  EXPECT_EQ(round_trip.gscip_solver_specific_output.status_detail(),
            "gscip_detail");
}

TEST(SolveResultTest, RoundTripPdlpOutput) {
  ModelStorage model;
  SolveResult solve_result(Termination::Optimal(10.0));
  solve_result.pdlp_solver_specific_output.mutable_convergence_information()
      ->set_corrected_dual_objective(2.0);

  ASSERT_OK_AND_ASSIGN(const SolveResultProto proto, solve_result.Proto());
  ASSERT_OK_AND_ASSIGN(const SolveResult round_trip,
                       SolveResult::FromProto(&model, proto));

  // Full trip
  EXPECT_THAT(round_trip, IsConsistentWith(solve_result, StrictCheck()));
  EXPECT_NEAR(round_trip.pdlp_solver_specific_output.convergence_information()
                  .corrected_dual_objective(),
              2.0, 1e-6);
}

TEST(SolveResultTest, StringTestOptimal) {
  if constexpr (!kTargetOsSupportsProtoDescriptor) {
    GTEST_SKIP()
        << "Writing text protos is not supported on portable plateforms.";
  }

  SolveResult solve_result(Termination::Optimal(10.0));

  // One solution.
  solve_result.solutions.emplace_back();
  EXPECT_EQ(StreamToString(solve_result),
            "{termination: {reason: optimal, problem_status: {primal_status: "
            "feasible, dual_status: feasible, primal_or_dual_infeasible: "
            "false}, objective_bounds: {primal_bound: 10, dual_bound: 10}}, "
            "solve_stats: {solve_time: 0, "
            "simplex_iterations: 0, "
            "barrier_iterations: 0, first_order_iterations: 0, node_count: 0}, "
            "solutions: [1 available], primal_rays: [], dual_rays: []}");

  // Two solutions.
  solve_result.solutions.emplace_back();
  EXPECT_EQ(StreamToString(solve_result),
            "{termination: {reason: optimal, problem_status: {primal_status: "
            "feasible, dual_status: feasible, primal_or_dual_infeasible: "
            "false}, objective_bounds: {primal_bound: 10, dual_bound: 10}}, "
            "solve_stats: {solve_time: 0, "
            "simplex_iterations: 0, "
            "barrier_iterations: 0, first_order_iterations: 0, node_count: 0}, "
            "solutions: [2 available], primal_rays: [], dual_rays: []}");
}

TEST(SolveResultTest, StringTestUnbounded) {
  if constexpr (!kTargetOsSupportsProtoDescriptor) {
    GTEST_SKIP()
        << "Writing text protos is not supported on portable plateforms.";
  }

  SolveResult solve_result(Termination::Unbounded(/*is_maximize=*/true));

  // One ray.
  solve_result.primal_rays.emplace_back();
  EXPECT_EQ(StreamToString(solve_result),
            "{termination: {reason: unbounded, problem_status: {primal_status: "
            "feasible, dual_status: infeasible, primal_or_dual_infeasible: "
            "false}, objective_bounds: {primal_bound: inf, dual_bound: inf}}, "
            "solve_stats: {solve_time: 0, "
            "simplex_iterations: 0, "
            "barrier_iterations: 0, first_order_iterations: 0, node_count: 0}, "
            "solutions: [], primal_rays: [1 available], dual_rays: []}");

  // Two rays.
  solve_result.primal_rays.emplace_back();
  EXPECT_EQ(StreamToString(solve_result),
            "{termination: {reason: unbounded, problem_status: {primal_status: "
            "feasible, dual_status: infeasible, primal_or_dual_infeasible: "
            "false}, objective_bounds: {primal_bound: inf, dual_bound: inf}}, "
            "solve_stats: {solve_time: 0, "
            "simplex_iterations: 0, "
            "barrier_iterations: 0, first_order_iterations: 0, node_count: 0}, "
            "solutions: [], primal_rays: [2 available], dual_rays: []}");
}

TEST(SolveResultTest, StringTestInfeasible) {
  if constexpr (!kTargetOsSupportsProtoDescriptor) {
    GTEST_SKIP()
        << "Writing text protos is not supported on portable plateforms.";
  }

  SolveResult solve_result(Termination::Infeasible(
      /*is_maximize=*/true, FeasibilityStatus::kFeasible));

  // One ray.
  solve_result.dual_rays.emplace_back();
  EXPECT_EQ(
      StreamToString(solve_result),
      "{termination: {reason: infeasible, problem_status: {primal_status: "
      "infeasible, dual_status: feasible, primal_or_dual_infeasible: false}, "
      "objective_bounds: {primal_bound: -inf, dual_bound: -inf}}, "
      "solve_stats: "
      "{solve_time: 0, "
      "simplex_iterations: 0, "
      "barrier_iterations: 0, first_order_iterations: 0, node_count: 0}, "
      "solutions: [], primal_rays: [], dual_rays: [1 available]}");

  // Two rays.
  solve_result.dual_rays.emplace_back();
  EXPECT_EQ(
      StreamToString(solve_result),
      "{termination: {reason: infeasible, problem_status: {primal_status: "
      "infeasible, dual_status: feasible, primal_or_dual_infeasible: false}, "
      "objective_bounds: {primal_bound: -inf, dual_bound: -inf}}, "
      "solve_stats: "
      "{solve_time: 0, "
      "simplex_iterations: 0, "
      "barrier_iterations: 0, first_order_iterations: 0, node_count: 0}, "
      "solutions: [], primal_rays: [], dual_rays: [2 available]}");
}

TEST(SolveResultTest, BestPrimalSolution) {
  SolveResult result(Termination::Optimal(3.0));
  result.solutions.emplace_back().primal_solution.emplace() = {
      .objective_value = 3.0, .feasibility_status = SolutionStatus::kFeasible};
  result.solutions.emplace_back().primal_solution.emplace() = {
      .objective_value = 4.0, .feasibility_status = SolutionStatus::kFeasible};
  EXPECT_EQ(result.best_primal_solution().objective_value, 3.0);
  ASSERT_TRUE(result.has_primal_feasible_solution());
  EXPECT_EQ(result.objective_value(), 3.0);
  EXPECT_EQ(result.primal_bound(), 3.0);
  EXPECT_EQ(result.dual_bound(), 3.0);
  EXPECT_EQ(result.best_objective_bound(), 3.0);
}

TEST(SolveResultTest, PrimalAndDualBounds) {
  SolveResult result(Termination::Feasible(
      /*is_maximize=*/true, Limit::kIteration, /*finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/20.0));
  EXPECT_EQ(result.primal_bound(), 10.0);
  EXPECT_EQ(result.dual_bound(), 20.0);
  EXPECT_EQ(result.best_objective_bound(), 20.0);
}

TEST(SolveResultTest, ObjectiveValue) {
  ModelStorage model;
  const Objective p = Objective::Primary(&model);
  const Objective o =
      Objective::Auxiliary(&model, model.AddAuxiliaryObjective(1));
  SolveResult result(Termination::Optimal(10.0));
  result.solutions.emplace_back().primal_solution.emplace() = {
      .objective_value = 3.0,
      .auxiliary_objective_values = {{o, 4.0}},
      .feasibility_status = SolutionStatus::kFeasible};
  result.solutions.emplace_back().primal_solution.emplace() = {
      .objective_value = 5.0,
      .auxiliary_objective_values = {{o, 6.0}},
      .feasibility_status = SolutionStatus::kFeasible};
  EXPECT_EQ(result.objective_value(), 3.0);
  EXPECT_EQ(result.objective_value(p), 3.0);
  EXPECT_EQ(result.objective_value(o), 4.0);
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
