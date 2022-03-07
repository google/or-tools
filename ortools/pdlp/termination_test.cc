// Copyright 2010-2021 Google LLC
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

#include "ortools/pdlp/termination.h"

#include <cmath>
#include <optional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {
namespace {

using ::google::protobuf::util::ParseTextOrDie;
using ::testing::FieldsAre;
using ::testing::Optional;

QuadraticProgramBoundNorms TestLpBoundNorms() {
  return {.l2_norm_primal_linear_objective = std::sqrt(36.25),
          .l2_norm_constraint_bounds = std::sqrt(210.0),
          .l_inf_norm_primal_linear_objective = 5.5,
          .l_inf_norm_constraint_bounds = 12.0};
}

class TerminationTest : public testing::TestWithParam<OptimalityNorm> {
 protected:
  void SetUp() override {
    test_criteria_ = ParseTextOrDie<TerminationCriteria>(R"pb(
      eps_optimal_absolute: 1.0e-4
      eps_optimal_relative: 1.0e-4
      eps_primal_infeasible: 1.0e-6
      eps_dual_infeasible: 1.0e-6
      time_sec_limit: 1.0
      kkt_matrix_pass_limit: 2000
      iteration_limit: 10)pb");
    test_criteria_.set_optimality_norm(GetParam());
  }

  TerminationCriteria test_criteria_;
};

TEST_P(TerminationTest, NoTerminationWithLargeGap) {
  IterationStats stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      # Ensures that optimality conditions are not met.
      primal_objective: 50.0
      dual_objective: -50.0
    })pb");
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats, TestLpBoundNorms()),
            absl::nullopt);
}

TEST_P(TerminationTest, NoTerminationWithEmptyIterationStats) {
  IterationStats stats;
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats, TestLpBoundNorms()),
            absl::nullopt);
}

TEST_P(TerminationTest, TerminationWithNumericalError) {
  IterationStats stats;
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckTerminationCriteria(test_criteria_, stats, TestLpBoundNorms(),
                               /*force_numerical_termination=*/true);
  EXPECT_THAT(
      maybe_result,
      Optional(FieldsAre(TERMINATION_REASON_NUMERICAL_ERROR, POINT_TYPE_NONE)));
}

TEST_P(TerminationTest, TerminationWithTimeLimit) {
  const auto stats =
      ParseTextOrDie<IterationStats>(R"pb(cumulative_time_sec: 100.0)pb");
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckTerminationCriteria(test_criteria_, stats, TestLpBoundNorms());
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_TIME_LIMIT,
                                               POINT_TYPE_NONE)));
}

TEST_P(TerminationTest, TerminationWithKktMatrixPassLimit) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    cumulative_kkt_matrix_passes: 2500)pb");
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckTerminationCriteria(test_criteria_, stats, TestLpBoundNorms());
  EXPECT_THAT(maybe_result,
              Optional(FieldsAre(TERMINATION_REASON_KKT_MATRIX_PASS_LIMIT,
                                 POINT_TYPE_NONE)));
}

TEST_P(TerminationTest, PrimalInfeasibleFromIterateDifference) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      dual_ray_objective: 1.0
      max_dual_ray_infeasibility: 1.0e-16
      candidate_type: POINT_TYPE_ITERATE_DIFFERENCE
    })pb");
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckTerminationCriteria(test_criteria_, stats, TestLpBoundNorms());
  EXPECT_THAT(maybe_result,
              Optional(FieldsAre(TERMINATION_REASON_PRIMAL_INFEASIBLE,
                                 POINT_TYPE_ITERATE_DIFFERENCE)));
}

TEST_P(TerminationTest, NoTerminationWithInfeasibleDualRay) {
  const auto stats_infeasible_ray = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      dual_ray_objective: 1.0
      max_dual_ray_infeasibility: 1.0e-5  # Too large
    })pb");
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats_infeasible_ray,
                                     TestLpBoundNorms()),
            absl::nullopt);
}

TEST_P(TerminationTest, NoTerminationWithNegativeDualRayObjective) {
  const auto stats_wrong_sign = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      dual_ray_objective: -1.0  # Wrong sign
      max_dual_ray_infeasibility: 0.0
    })pb");
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats_wrong_sign,
                                     TestLpBoundNorms()),
            absl::nullopt);
}

TEST_P(TerminationTest, NoTerminationWithZeroDualRayObjective) {
  const auto stats_objective_zero = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      dual_ray_objective: 0.0
      max_dual_ray_infeasibility: 0.0
    })pb");
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats_objective_zero,
                                     TestLpBoundNorms()),
            absl::nullopt);
}

TEST_P(TerminationTest, DualInfeasibleFromAverageIterate) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      primal_ray_linear_objective: -1.0
      max_primal_ray_infeasibility: 1.0e-16
      candidate_type: POINT_TYPE_AVERAGE_ITERATE
    })pb");
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckTerminationCriteria(test_criteria_, stats, TestLpBoundNorms());
  EXPECT_THAT(maybe_result,
              Optional(FieldsAre(TERMINATION_REASON_DUAL_INFEASIBLE,
                                 POINT_TYPE_AVERAGE_ITERATE)));
}

TEST_P(TerminationTest, NoTerminationWithInfeasiblePrimalRay) {
  const auto stats_infeasible_ray = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      primal_ray_linear_objective: -1.0
      max_primal_ray_infeasibility: 1.0e-5  # Too large
    })pb");
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats_infeasible_ray,
                                     TestLpBoundNorms()),
            absl::nullopt);
}

TEST_P(TerminationTest, NoTerminationWithPositivePrimalRayObjective) {
  const auto stats_wrong_sign = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      primal_ray_linear_objective: 1.0  # Wrong sign
      max_primal_ray_infeasibility: 0.0
    })pb");
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats_wrong_sign,
                                     TestLpBoundNorms()),
            absl::nullopt);
}

TEST_P(TerminationTest, NoTerminationWithZeroPrimalRayObjective) {
  const auto stats_objective_zero = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      primal_ray_linear_objective: 0.0
      max_primal_ray_infeasibility: 0.0
    })pb");
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats_objective_zero,
                                     TestLpBoundNorms()),
            absl::nullopt);
}

TEST_P(TerminationTest, Optimal) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 0.0
      l2_dual_residual: 0.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");

  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckTerminationCriteria(test_criteria_, stats, TestLpBoundNorms());
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                               POINT_TYPE_CURRENT_ITERATE)));
}

TEST_P(TerminationTest, OptimalEvenWithNumericalError) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 0.0
      l2_dual_residual: 0.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  // Tests that OPTIMAL overrides NUMERICAL_ERROR when
  // force_numerical_termination == true.
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckTerminationCriteria(test_criteria_, stats, TestLpBoundNorms(),
                               /*force_numerical_termination=*/true);
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                               POINT_TYPE_CURRENT_ITERATE)));
}

TEST_P(TerminationTest, NoTerminationWithBadGap) {
  const auto stats_bad_gap = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 10.0
      dual_objective: 1.0
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 0.0
      l2_dual_residual: 0.0
    })pb");
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats_bad_gap,
                                     TestLpBoundNorms()),
            absl::nullopt);
}

TEST_P(TerminationTest, NoTerminationWithInfiniteGap) {
  const auto stats_infinite_gap = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 0
      dual_objective: -Inf
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 0.0
      l2_dual_residual: 0.0
    })pb");
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats_infinite_gap,
                                     TestLpBoundNorms()),
            absl::nullopt);
}

TEST_P(TerminationTest, NoTerminationWithBadPrimalResidual) {
  const auto stats_bad_primal = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 1.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 1.0
      l2_dual_residual: 0.0
    })pb");
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats_bad_primal,
                                     TestLpBoundNorms()),
            absl::nullopt);
}

TEST_P(TerminationTest, NoTerminationWithBadDualResidual) {
  const auto stats_bad_dual = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 1.0
      l2_primal_residual: 0.0
      l2_dual_residual: 1.0
    })pb");
  EXPECT_EQ(CheckTerminationCriteria(test_criteria_, stats_bad_dual,
                                     TestLpBoundNorms()),
            absl::nullopt);
}

// Tests that optimality is checked with non-strict inequalities, as per the
// definitions in solvers.proto.
TEST_P(TerminationTest, ZeroToleranceZeroError) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 0.0
      l2_dual_residual: 0.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");

  test_criteria_.set_eps_optimal_absolute(0.0);
  test_criteria_.set_eps_optimal_relative(0.0);

  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckTerminationCriteria(test_criteria_, stats, TestLpBoundNorms());
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                               POINT_TYPE_CURRENT_ITERATE)));
}

INSTANTIATE_TEST_SUITE_P(OptNorm, TerminationTest,
                         testing::Values(OPTIMALITY_NORM_L2,
                                         OPTIMALITY_NORM_L_INF));

TEST(TerminationTest, L2AndLInfDiffer) {
  auto test_criteria = ParseTextOrDie<TerminationCriteria>(R"pb(
    eps_optimal_relative: 1.0)pb");

  // For L2, optimality requires norm(primal_residual, 2) <= 14.49
  // For LInf, optimality requires norm(primal_residual, Inf) <= 12.0

  struct {
    double primal_residual;
    std::optional<TerminationReasonAndPointType> expected_l2;
    std::optional<TerminationReasonAndPointType> expected_l_inf;
  } test_configs[] = {
      {10.0,
       TerminationReasonAndPointType{.reason = TERMINATION_REASON_OPTIMAL,
                                     .type = POINT_TYPE_CURRENT_ITERATE},
       TerminationReasonAndPointType{.reason = TERMINATION_REASON_OPTIMAL,
                                     .type = POINT_TYPE_CURRENT_ITERATE}},
      {13.0,
       TerminationReasonAndPointType{.reason = TERMINATION_REASON_OPTIMAL,
                                     .type = POINT_TYPE_CURRENT_ITERATE},
       absl::nullopt},
      {15.0, absl::nullopt, absl::nullopt}};

  for (const auto& config : test_configs) {
    IterationStats stats;
    auto* convergence_info = stats.add_convergence_information();
    convergence_info->set_primal_objective(1.0);
    convergence_info->set_dual_objective(1.0);
    convergence_info->set_l2_primal_residual(config.primal_residual);
    convergence_info->set_l_inf_primal_residual(config.primal_residual);
    convergence_info->set_candidate_type(POINT_TYPE_CURRENT_ITERATE);

    test_criteria.set_optimality_norm(OPTIMALITY_NORM_L2);

    std::optional<TerminationReasonAndPointType> maybe_result =
        CheckTerminationCriteria(test_criteria, stats, TestLpBoundNorms());
    ASSERT_TRUE(maybe_result.has_value() == config.expected_l2.has_value())
        << "primal_residual: " << config.primal_residual;
    if (config.expected_l2.has_value()) {
      EXPECT_EQ(maybe_result->reason, config.expected_l2->reason);
      EXPECT_EQ(maybe_result->type, config.expected_l2->type);
    }

    test_criteria.set_optimality_norm(OPTIMALITY_NORM_L_INF);
    maybe_result =
        CheckTerminationCriteria(test_criteria, stats, TestLpBoundNorms());
    ASSERT_TRUE(maybe_result.has_value() == config.expected_l_inf.has_value())
        << "primal_residual: " << config.primal_residual;
    if (config.expected_l_inf.has_value()) {
      EXPECT_EQ(maybe_result->reason, config.expected_l_inf->reason);
      EXPECT_EQ(maybe_result->type, config.expected_l_inf->type);
    }
  }
}

TEST(BoundNormsFromProblemStats, ExtractsBoundNorms) {
  const auto qp_stats = ParseTextOrDie<QuadraticProgramStats>(R"pb(
    objective_vector_l2_norm: 4.0
    combined_bounds_l2_norm: 3.0
    objective_vector_abs_max: 1.0
    combined_bounds_max: 2.0
  )pb");
  const QuadraticProgramBoundNorms norms = BoundNormsFromProblemStats(qp_stats);
  EXPECT_EQ(norms.l2_norm_primal_linear_objective, 4.0);
  EXPECT_EQ(norms.l2_norm_constraint_bounds, 3.0);
  EXPECT_EQ(norms.l_inf_norm_primal_linear_objective, 1.0);
  EXPECT_EQ(norms.l_inf_norm_constraint_bounds, 2.0);
}

TEST(ComputeRelativeResiduals,
     ComputesRelativeResidualsForZeroAbsoluteTolerance) {
  ConvergenceInformation stats;
  // If the absolute error tolerance is 0.0, the relative residuals are just the
  // absolute residuals divided by the corresponding norms (the actual nonzero
  // value of the relative error tolerance doesn't matter).
  stats.set_primal_objective(10.0);
  stats.set_dual_objective(5.0);
  stats.set_l_inf_primal_residual(1.0);
  stats.set_l2_primal_residual(1.0);
  stats.set_l_inf_dual_residual(1.0);
  stats.set_l2_dual_residual(1.0);
  const RelativeConvergenceInformation relative_info = ComputeRelativeResiduals(
      /*eps_optimal_absolute=*/0.0, /*eps_optimal_relative=*/1.0e-6,
      TestLpBoundNorms(), stats);

  EXPECT_EQ(relative_info.relative_l_inf_primal_residual, 1.0 / 12.0);
  EXPECT_EQ(relative_info.relative_l2_primal_residual, 1.0 / std::sqrt(210.0));

  EXPECT_EQ(relative_info.relative_l_inf_dual_residual, 1.0 / 5.5);
  EXPECT_EQ(relative_info.relative_l2_dual_residual, 1.0 / sqrt(36.25));

  // The relative optimality gap should just be the objective difference divided
  // by the sum of absolute values (the actual nonzero value of the relative
  // error tolerance doesn't matter).
  EXPECT_EQ(relative_info.relative_optimality_gap, 5.0 / 15.0);
}

TEST(ComputeRelativeResiduals,
     ComputesRelativeResidualsForZeroRelativeTolerance) {
  ConvergenceInformation stats;
  // If the relative error tolerance is 0.0, all of the relative residuals and
  // the relative optimality gap should be 0.0, no matter what the absolute
  // error tolerance is.
  stats.set_primal_objective(10.0);
  stats.set_dual_objective(5.0);
  stats.set_l_inf_primal_residual(1.0);
  stats.set_l2_primal_residual(1.0);
  stats.set_l_inf_dual_residual(1.0);
  stats.set_l2_dual_residual(1.0);
  const RelativeConvergenceInformation relative_info = ComputeRelativeResiduals(
      /*eps_optimal_absolute=*/0.0, /*eps_optimal_relative=*/0.0,
      TestLpBoundNorms(), stats);

  EXPECT_EQ(relative_info.relative_l_inf_primal_residual, 0.0);
  EXPECT_EQ(relative_info.relative_l2_primal_residual, 0.0);
  EXPECT_EQ(relative_info.relative_l_inf_dual_residual, 0.0);
  EXPECT_EQ(relative_info.relative_l2_dual_residual, 0.0);
  EXPECT_EQ(relative_info.relative_optimality_gap, 0.0);
}

TEST(ComputeRelativeResiduals,
     ComputesCorrectRelativeResidualsForEqualTolerances) {
  ConvergenceInformation stats;
  // If the absolute error tolerance and relative error tolerance are equal (and
  // nonzero), the relative residuals are the absolute residuals divided by 1.0
  // plus the corresponding norms.
  stats.set_primal_objective(10.0);
  stats.set_dual_objective(5.0);
  stats.set_l_inf_primal_residual(1.0);
  stats.set_l2_primal_residual(1.0);
  stats.set_l_inf_dual_residual(1.0);
  stats.set_l2_dual_residual(1.0);
  const RelativeConvergenceInformation relative_info = ComputeRelativeResiduals(
      /*eps_optimal_absolute=*/1.0e-6, /*eps_optimal_relative=*/1.0e-6,
      TestLpBoundNorms(), stats);

  EXPECT_EQ(relative_info.relative_l_inf_primal_residual, 1.0 / (1.0 + 12.0));
  EXPECT_EQ(relative_info.relative_l2_primal_residual,
            1.0 / (1.0 + std::sqrt(210.0)));

  EXPECT_EQ(relative_info.relative_l_inf_dual_residual, 1.0 / (1.0 + 5.5));
  EXPECT_EQ(relative_info.relative_l2_dual_residual, 1.0 / (1.0 + sqrt(36.25)));

  // The relative optimality gap should just be the objective difference divided
  // by 1.0 + the sum of absolute values.
  EXPECT_EQ(relative_info.relative_optimality_gap, 5.0 / (1.0 + 15.0));
}

}  // namespace
}  // namespace operations_research::pdlp
