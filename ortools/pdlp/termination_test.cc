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

#include "ortools/pdlp/termination.h"

#include <atomic>
#include <cmath>
#include <limits>
#include <optional>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {
bool operator==(const TerminationCriteria::DetailedOptimalityCriteria& lhs,
                const TerminationCriteria::DetailedOptimalityCriteria& rhs) {
  if (lhs.eps_optimal_primal_residual_absolute() !=
      rhs.eps_optimal_primal_residual_absolute()) {
    return false;
  }
  if (lhs.eps_optimal_primal_residual_relative() !=
      rhs.eps_optimal_primal_residual_relative()) {
    return false;
  }
  if (lhs.eps_optimal_dual_residual_absolute() !=
      rhs.eps_optimal_dual_residual_absolute()) {
    return false;
  }
  if (lhs.eps_optimal_dual_residual_relative() !=
      rhs.eps_optimal_dual_residual_relative()) {
    return false;
  }
  if (lhs.eps_optimal_objective_gap_absolute() !=
      rhs.eps_optimal_objective_gap_absolute()) {
    return false;
  }
  if (lhs.eps_optimal_objective_gap_relative() !=
      rhs.eps_optimal_objective_gap_relative()) {
    return false;
  }
  return true;
}

namespace {

using ::google::protobuf::util::ParseTextOrDie;
using ::testing::Eq;
using ::testing::FieldsAre;
using ::testing::Optional;

QuadraticProgramBoundNorms TestLpBoundNorms() {
  return {.l2_norm_primal_linear_objective = std::sqrt(36.25),
          .l2_norm_constraint_bounds = std::sqrt(210.0),
          .l_inf_norm_primal_linear_objective = 5.5,
          .l_inf_norm_constraint_bounds = 12.0};
}

QuadraticProgramBoundNorms ZeroLpBoundNorms() {
  return {.l2_norm_primal_linear_objective = 0.0,
          .l2_norm_constraint_bounds = 0.0,
          .l_inf_norm_primal_linear_objective = 0.0,
          .l_inf_norm_constraint_bounds = 0.0};
}

class SimpleTerminationTest : public testing::Test {
 protected:
  void SetUp() override {
    test_criteria_ = ParseTextOrDie<TerminationCriteria>(R"pb(
      time_sec_limit: 1.0
      kkt_matrix_pass_limit: 2000
      iteration_limit: 10)pb");
  }

  TerminationCriteria test_criteria_;
};

class IterateTerminationTest : public testing::TestWithParam<OptimalityNorm> {
 protected:
  void SetUp() override {
    test_criteria_ = ParseTextOrDie<TerminationCriteria>(R"pb(
      simple_optimality_criteria {
        eps_optimal_absolute: 1.0e-4
        eps_optimal_relative: 1.0e-4
      }
      eps_primal_infeasible: 1.0e-6
      eps_dual_infeasible: 1.0e-6
      time_sec_limit: 1.0
      kkt_matrix_pass_limit: 2000
      iteration_limit: 10)pb");
    test_criteria_.set_optimality_norm(GetParam());
  }

  TerminationCriteria test_criteria_;
};

class DetailedRelativeTerminationTest
    : public testing::TestWithParam<OptimalityNorm> {
 protected:
  void SetUp() override {
    test_criteria_ = ParseTextOrDie<TerminationCriteria>(R"pb(
      detailed_optimality_criteria {
        eps_optimal_primal_residual_absolute: 0.0
        eps_optimal_primal_residual_relative: 1.0e-4
        eps_optimal_dual_residual_absolute: 0.0
        eps_optimal_dual_residual_relative: 1.0e-4
        eps_optimal_objective_gap_absolute: 0.0
        eps_optimal_objective_gap_relative: 1.0e-4
      }
    )pb");
    test_criteria_.set_optimality_norm(GetParam());
  }

  TerminationCriteria test_criteria_;
};

class DetailedAbsoluteTerminationTest
    : public testing::TestWithParam<OptimalityNorm> {
 protected:
  void SetUp() override {
    test_criteria_ = ParseTextOrDie<TerminationCriteria>(R"pb(
      detailed_optimality_criteria {
        eps_optimal_primal_residual_absolute: 1.0e-4
        eps_optimal_primal_residual_relative: 0.0
        eps_optimal_dual_residual_absolute: 1.0e-4
        eps_optimal_dual_residual_relative: 0.0
        eps_optimal_objective_gap_absolute: 1.0e-4
        eps_optimal_objective_gap_relative: 0.0
      }
    )pb");
    test_criteria_.set_optimality_norm(GetParam());
  }

  TerminationCriteria test_criteria_;
};

TEST(EffectiveOptimalityCriteriaTest, SimpleOptimalityCriteriaOverload) {
  const auto criteria =
      ParseTextOrDie<TerminationCriteria::SimpleOptimalityCriteria>(
          R"pb(eps_optimal_absolute: 1.0e-4 eps_optimal_relative: 2.0e-4)pb");
  EXPECT_THAT(
      EffectiveOptimalityCriteria(criteria),
      Eq(ParseTextOrDie<TerminationCriteria::DetailedOptimalityCriteria>(
          R"pb(
            eps_optimal_primal_residual_absolute: 1.0e-4
            eps_optimal_primal_residual_relative: 2.0e-4
            eps_optimal_dual_residual_absolute: 1.0e-4
            eps_optimal_dual_residual_relative: 2.0e-4
            eps_optimal_objective_gap_absolute: 1.0e-4
            eps_optimal_objective_gap_relative: 2.0e-4
          )pb")));
}

TEST(EffectiveOptimalityCriteriaTest, SimpleOptimalityCriteriaInput) {
  const auto criteria =
      ParseTextOrDie<TerminationCriteria>(R"pb(simple_optimality_criteria {
                                                 eps_optimal_absolute: 1.0e-4
                                                 eps_optimal_relative: 2.0e-4
                                               })pb");
  EXPECT_THAT(
      EffectiveOptimalityCriteria(criteria),
      Eq(ParseTextOrDie<TerminationCriteria::DetailedOptimalityCriteria>(
          R"pb(
            eps_optimal_primal_residual_absolute: 1.0e-4
            eps_optimal_primal_residual_relative: 2.0e-4
            eps_optimal_dual_residual_absolute: 1.0e-4
            eps_optimal_dual_residual_relative: 2.0e-4
            eps_optimal_objective_gap_absolute: 1.0e-4
            eps_optimal_objective_gap_relative: 2.0e-4
          )pb")));
}

TEST(EffectiveOptimalityCriteriaTest, DetailedOptimalityCriteriaInput) {
  const auto criteria = ParseTextOrDie<TerminationCriteria>(
      R"pb(detailed_optimality_criteria {
             eps_optimal_primal_residual_absolute: 1.0e-4
             eps_optimal_primal_residual_relative: 2.0e-4
             eps_optimal_dual_residual_absolute: 3.0e-4
             eps_optimal_dual_residual_relative: 4.0e-4
             eps_optimal_objective_gap_absolute: 5.0e-4
             eps_optimal_objective_gap_relative: 6.0e-4
           })pb");
  EXPECT_THAT(EffectiveOptimalityCriteria(criteria),
              Eq(criteria.detailed_optimality_criteria()));
}

TEST(EffectiveOptimalityCriteriaTest, DeprecatedInput) {
  const auto criteria = ParseTextOrDie<TerminationCriteria>(
      R"pb(eps_optimal_absolute: 1.0e-4 eps_optimal_relative: 2.0e-4)pb");
  EXPECT_THAT(
      EffectiveOptimalityCriteria(criteria),
      Eq(ParseTextOrDie<TerminationCriteria::DetailedOptimalityCriteria>(
          R"pb(
            eps_optimal_primal_residual_absolute: 1.0e-4
            eps_optimal_primal_residual_relative: 2.0e-4
            eps_optimal_dual_residual_absolute: 1.0e-4
            eps_optimal_dual_residual_relative: 2.0e-4
            eps_optimal_objective_gap_absolute: 1.0e-4
            eps_optimal_objective_gap_relative: 2.0e-4
          )pb")));
}

TEST_P(DetailedRelativeTerminationTest, TerminationWithNearOptimal) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.00019
      dual_objective: 1.0
      l_inf_primal_residual: 11.0e-4
      l_inf_dual_residual: 5.4e-4
      l2_primal_residual: 14.0e-4
      l2_dual_residual: 6.0e-4
      l_inf_componentwise_primal_residual: 9.0e-5
      l_inf_componentwise_dual_residual: 9.0e-5
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  EXPECT_TRUE(ObjectiveGapMet(test_criteria_.detailed_optimality_criteria(),
                              stats.convergence_information(0)));
  EXPECT_TRUE(OptimalityCriteriaMet(
      test_criteria_.detailed_optimality_criteria(),
      stats.convergence_information(0), test_criteria_.optimality_norm(),
      TestLpBoundNorms()));
  EXPECT_THAT(CheckIterateTerminationCriteria(test_criteria_, stats,
                                              TestLpBoundNorms()),
              Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                 POINT_TYPE_CURRENT_ITERATE)));
}

TEST_P(DetailedRelativeTerminationTest, NoTerminationWithExcessiveGap) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.00021
      dual_objective: 1.0
      l_inf_primal_residual: 11.0e-4
      l_inf_dual_residual: 5.4e-4
      l2_primal_residual: 14.0e-4
      l2_dual_residual: 6.0e-4
      l_inf_componentwise_primal_residual: 9.0e-5
      l_inf_componentwise_dual_residual: 9.0e-5
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  EXPECT_FALSE(ObjectiveGapMet(test_criteria_.detailed_optimality_criteria(),
                               stats.convergence_information(0)));
  EXPECT_FALSE(OptimalityCriteriaMet(
      test_criteria_.detailed_optimality_criteria(),
      stats.convergence_information(0), test_criteria_.optimality_norm(),
      TestLpBoundNorms()));
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(DetailedRelativeTerminationTest,
       NoTerminationWithExcessivePrimalResidual) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.00019
      dual_objective: 1.0
      l_inf_primal_residual: 13.0e-4
      l_inf_dual_residual: 5.4e-4
      l2_primal_residual: 15.0e-4
      l2_dual_residual: 6.0e-4
      l_inf_componentwise_primal_residual: 1.1e-4
      l_inf_componentwise_dual_residual: 9.0e-5
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  EXPECT_FALSE(OptimalityCriteriaMet(
      test_criteria_.detailed_optimality_criteria(),
      stats.convergence_information(0), test_criteria_.optimality_norm(),
      TestLpBoundNorms()));
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(DetailedRelativeTerminationTest,
       NoTerminationWithExcessiveDualResidual) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.00019
      dual_objective: 1.0
      l_inf_primal_residual: 11.0e-4
      l_inf_dual_residual: 5.6e-4
      l2_primal_residual: 14.0e-4
      l2_dual_residual: 7.0e-4
      l_inf_componentwise_primal_residual: 9.0e-5
      l_inf_componentwise_dual_residual: 1.1e-4
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  EXPECT_FALSE(OptimalityCriteriaMet(
      test_criteria_.detailed_optimality_criteria(),
      stats.convergence_information(0), test_criteria_.optimality_norm(),
      TestLpBoundNorms()));
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(DetailedAbsoluteTerminationTest, TerminationWithNearOptimal) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.00009
      dual_objective: 1.0
      l_inf_primal_residual: 9.0e-5
      l_inf_dual_residual: 9.0e-5
      l2_primal_residual: 9.0e-5
      l2_dual_residual: 9.0e-5
      l_inf_componentwise_primal_residual: 0.0
      l_inf_componentwise_dual_residual: 0.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  EXPECT_TRUE(ObjectiveGapMet(test_criteria_.detailed_optimality_criteria(),
                              stats.convergence_information(0)));
  EXPECT_TRUE(OptimalityCriteriaMet(
      test_criteria_.detailed_optimality_criteria(),
      stats.convergence_information(0), test_criteria_.optimality_norm(),
      TestLpBoundNorms()));
  EXPECT_THAT(CheckIterateTerminationCriteria(test_criteria_, stats,
                                              TestLpBoundNorms()),
              Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                 POINT_TYPE_CURRENT_ITERATE)));
}

TEST_P(DetailedAbsoluteTerminationTest, NoTerminationWithExcessiveGap) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.00011
      dual_objective: 1.0
      l_inf_primal_residual: 9.0e-5
      l_inf_dual_residual: 9.0e-5
      l2_primal_residual: 9.0e-5
      l2_dual_residual: 9.0e-5
      l_inf_componentwise_primal_residual: 0.0
      l_inf_componentwise_dual_residual: 0.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  EXPECT_FALSE(ObjectiveGapMet(test_criteria_.detailed_optimality_criteria(),
                               stats.convergence_information(0)));
  EXPECT_FALSE(OptimalityCriteriaMet(
      test_criteria_.detailed_optimality_criteria(),
      stats.convergence_information(0), test_criteria_.optimality_norm(),
      TestLpBoundNorms()));
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(DetailedAbsoluteTerminationTest,
       NoTerminationWithExcessivePrimalResidual) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.00009
      dual_objective: 1.0
      l_inf_primal_residual: 11.0e-5
      l_inf_dual_residual: 9.0e-5
      l2_primal_residual: 11.0e-5
      l2_dual_residual: 9.0e-5
      l_inf_componentwise_primal_residual: 1.0e-6
      l_inf_componentwise_dual_residual: 0.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  EXPECT_FALSE(OptimalityCriteriaMet(
      test_criteria_.detailed_optimality_criteria(),
      stats.convergence_information(0), test_criteria_.optimality_norm(),
      TestLpBoundNorms()));
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(DetailedAbsoluteTerminationTest,
       NoTerminationWithExcessiveDualResidual) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.00009
      dual_objective: 1.0
      l_inf_primal_residual: 9.0e-5
      l_inf_dual_residual: 11.0e-5
      l2_primal_residual: 9.0e-5
      l2_dual_residual: 11.0e-5
      l_inf_componentwise_primal_residual: 0.0
      l_inf_componentwise_dual_residual: 1.0e-6
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  EXPECT_FALSE(OptimalityCriteriaMet(
      test_criteria_.detailed_optimality_criteria(),
      stats.convergence_information(0), test_criteria_.optimality_norm(),
      TestLpBoundNorms()));
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(IterateTerminationTest, NoTerminationWithLargeGap) {
  IterationStats stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      # Ensures that optimality conditions are not met.
      primal_objective: 50.0
      dual_objective: -50.0
    })pb");
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_F(SimpleTerminationTest, NoTerminationWithEmptyIterationStats) {
  IterationStats stats;
  EXPECT_EQ(CheckSimpleTerminationCriteria(test_criteria_, stats),
            std::nullopt);
}

TEST_P(IterateTerminationTest, NoTerminationWithEmptyIterationStats) {
  IterationStats stats;
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_F(SimpleTerminationTest, TerminationWithInterruptSolve) {
  IterationStats stats;
  std::atomic<bool> interrupt_solve = true;
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckSimpleTerminationCriteria(test_criteria_, stats, &interrupt_solve);
  EXPECT_THAT(maybe_result,
              Optional(FieldsAre(TERMINATION_REASON_INTERRUPTED_BY_USER,
                                 POINT_TYPE_NONE)));
}

TEST_P(IterateTerminationTest, TerminationWithNumericalError) {
  IterationStats stats;
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckIterateTerminationCriteria(test_criteria_, stats, TestLpBoundNorms(),
                                      /*force_numerical_termination=*/true);
  EXPECT_THAT(
      maybe_result,
      Optional(FieldsAre(TERMINATION_REASON_NUMERICAL_ERROR, POINT_TYPE_NONE)));
}

TEST_F(SimpleTerminationTest, TerminationWithTimeLimit) {
  const auto stats =
      ParseTextOrDie<IterationStats>(R"pb(cumulative_time_sec: 100.0)pb");
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckSimpleTerminationCriteria(test_criteria_, stats);
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_TIME_LIMIT,
                                               POINT_TYPE_NONE)));
}

TEST_F(SimpleTerminationTest, TerminationWithKktMatrixPassLimit) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    cumulative_kkt_matrix_passes: 2500)pb");
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckSimpleTerminationCriteria(test_criteria_, stats);
  EXPECT_THAT(maybe_result,
              Optional(FieldsAre(TERMINATION_REASON_KKT_MATRIX_PASS_LIMIT,
                                 POINT_TYPE_NONE)));
}

TEST_F(SimpleTerminationTest, TerminationWithIterationLimit) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    iteration_number: 20)pb");
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckSimpleTerminationCriteria(test_criteria_, stats);
  EXPECT_THAT(
      maybe_result,
      Optional(FieldsAre(TERMINATION_REASON_ITERATION_LIMIT, POINT_TYPE_NONE)));
}

TEST_P(IterateTerminationTest, PrimalInfeasibleFromIterateDifference) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      dual_ray_objective: 1.0
      max_dual_ray_infeasibility: 1.0e-16
      candidate_type: POINT_TYPE_ITERATE_DIFFERENCE
    })pb");
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckIterateTerminationCriteria(test_criteria_, stats,
                                      TestLpBoundNorms());
  EXPECT_THAT(maybe_result,
              Optional(FieldsAre(TERMINATION_REASON_PRIMAL_INFEASIBLE,
                                 POINT_TYPE_ITERATE_DIFFERENCE)));
}

TEST_P(IterateTerminationTest, NoTerminationWithInfeasibleDualRay) {
  const auto stats_infeasible_ray = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      dual_ray_objective: 1.0
      max_dual_ray_infeasibility: 1.0e-5  # Too large
    })pb");
  EXPECT_EQ(CheckIterateTerminationCriteria(
                test_criteria_, stats_infeasible_ray, TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(IterateTerminationTest, NoTerminationWithNegativeDualRayObjective) {
  const auto stats_wrong_sign = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      dual_ray_objective: -1.0  # Wrong sign
      max_dual_ray_infeasibility: 0.0
    })pb");
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats_wrong_sign,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(IterateTerminationTest, NoTerminationWithZeroDualRayObjective) {
  const auto stats_objective_zero = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      dual_ray_objective: 0.0
      max_dual_ray_infeasibility: 0.0
    })pb");
  EXPECT_EQ(CheckIterateTerminationCriteria(
                test_criteria_, stats_objective_zero, TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(IterateTerminationTest, DualInfeasibleFromAverageIterate) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      primal_ray_linear_objective: -1.0
      max_primal_ray_infeasibility: 1.0e-16
      candidate_type: POINT_TYPE_AVERAGE_ITERATE
    })pb");
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckIterateTerminationCriteria(test_criteria_, stats,
                                      TestLpBoundNorms());
  EXPECT_THAT(maybe_result,
              Optional(FieldsAre(TERMINATION_REASON_DUAL_INFEASIBLE,
                                 POINT_TYPE_AVERAGE_ITERATE)));
}

TEST_P(IterateTerminationTest, NoTerminationWithInfeasiblePrimalRay) {
  const auto stats_infeasible_ray = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      primal_ray_linear_objective: -1.0
      max_primal_ray_infeasibility: 1.0e-5  # Too large
    })pb");
  EXPECT_EQ(CheckIterateTerminationCriteria(
                test_criteria_, stats_infeasible_ray, TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(IterateTerminationTest, NoTerminationWithPositivePrimalRayObjective) {
  const auto stats_wrong_sign = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      primal_ray_linear_objective: 1.0  # Wrong sign
      max_primal_ray_infeasibility: 0.0
    })pb");
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats_wrong_sign,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(IterateTerminationTest, NoTerminationWithZeroPrimalRayObjective) {
  const auto stats_objective_zero = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information: {
      primal_ray_linear_objective: 0.0
      max_primal_ray_infeasibility: 0.0
    })pb");
  EXPECT_EQ(CheckIterateTerminationCriteria(
                test_criteria_, stats_objective_zero, TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(IterateTerminationTest, TerminationWithOptimal) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 0.0
      l2_dual_residual: 0.0
      l_inf_componentwise_primal_residual: 0.0
      l_inf_componentwise_dual_residual: 0.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");

  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckIterateTerminationCriteria(test_criteria_, stats,
                                      TestLpBoundNorms());
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                               POINT_TYPE_CURRENT_ITERATE)));
}

TEST_P(IterateTerminationTest, TerminationWithNearOptimal) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.00019
      dual_objective: 1.0
      l_inf_primal_residual: 11.0e-4
      l_inf_dual_residual: 5.4e-4
      l2_primal_residual: 14.0e-4
      l2_dual_residual: 6.0e-4
      l_inf_componentwise_primal_residual: 9.0e-5
      l_inf_componentwise_dual_residual: 9.0e-5
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");

  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckIterateTerminationCriteria(test_criteria_, stats,
                                      TestLpBoundNorms());
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                               POINT_TYPE_CURRENT_ITERATE)));
}

TEST_P(IterateTerminationTest,
       TerminationWithNonOptimalInfiniteAbsoluteTolerances) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 1.0
      l_inf_dual_residual: 1.0
      l2_primal_residual: 1.0
      l2_dual_residual: 1.0
      l_inf_componentwise_primal_residual: 1.0
      l_inf_componentwise_dual_residual: 1.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  test_criteria_.mutable_simple_optimality_criteria()->set_eps_optimal_absolute(
      std::numeric_limits<double>::infinity());
  test_criteria_.mutable_simple_optimality_criteria()->set_eps_optimal_relative(
      0);
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckIterateTerminationCriteria(test_criteria_, stats,
                                      ZeroLpBoundNorms());
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                               POINT_TYPE_CURRENT_ITERATE)));
}

TEST_P(IterateTerminationTest,
       TerminationWithNonOptimalInfiniteRelativeTolerances) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 0.0
      dual_objective: 0.0
      l_inf_primal_residual: 1.0
      l_inf_dual_residual: 1.0
      l2_primal_residual: 1.0
      l2_dual_residual: 1.0
      l_inf_componentwise_primal_residual: 1.0
      l_inf_componentwise_dual_residual: 1.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  test_criteria_.mutable_simple_optimality_criteria()->set_eps_optimal_absolute(
      0);
  test_criteria_.mutable_simple_optimality_criteria()->set_eps_optimal_relative(
      std::numeric_limits<double>::infinity());
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckIterateTerminationCriteria(test_criteria_, stats,
                                      ZeroLpBoundNorms());
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                               POINT_TYPE_CURRENT_ITERATE)));
}

TEST_P(IterateTerminationTest,
       TerminationWithNonOptimalInfiniteAbsoluteAndRelativeTolerances) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 1.0
      l_inf_dual_residual: 1.0
      l2_primal_residual: 1.0
      l2_dual_residual: 1.0
      l_inf_componentwise_primal_residual: 1.0
      l_inf_componentwise_dual_residual: 1.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  test_criteria_.mutable_simple_optimality_criteria()->set_eps_optimal_absolute(
      std::numeric_limits<double>::infinity());
  test_criteria_.mutable_simple_optimality_criteria()->set_eps_optimal_relative(
      std::numeric_limits<double>::infinity());
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckIterateTerminationCriteria(test_criteria_, stats,
                                      ZeroLpBoundNorms());
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                               POINT_TYPE_CURRENT_ITERATE)));
}

TEST_P(IterateTerminationTest, OptimalEvenWithNumericalError) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 0.0
      l2_dual_residual: 0.0
      l_inf_componentwise_primal_residual: 0.0
      l_inf_componentwise_dual_residual: 0.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");
  // Tests that `TERMINATION_REASON_OPTIMAL` overrides
  // `TERMINATION_REASON_NUMERICAL_ERROR` when
  // `force_numerical_termination == true`.
  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckIterateTerminationCriteria(test_criteria_, stats, TestLpBoundNorms(),
                                      /*force_numerical_termination=*/true);
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                               POINT_TYPE_CURRENT_ITERATE)));
}

TEST_P(IterateTerminationTest, NoTerminationWithBadGap) {
  const auto stats_bad_gap = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 10.0
      dual_objective: 1.0
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 0.0
      l2_dual_residual: 0.0
      l_inf_componentwise_primal_residual: 0.0
      l_inf_componentwise_dual_residual: 0.0
    })pb");
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats_bad_gap,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(IterateTerminationTest, NoTerminationWithInfiniteGap) {
  const auto stats_infinite_gap = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 0
      dual_objective: -Inf
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 0.0
      l2_dual_residual: 0.0
      l_inf_componentwise_primal_residual: 0.0
      l_inf_componentwise_dual_residual: 0.0
    })pb");
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats_infinite_gap,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(IterateTerminationTest, NoTerminationWithBadPrimalResidual) {
  const auto stats_bad_primal = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 1.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 1.0
      l2_dual_residual: 0.0
      l_inf_componentwise_primal_residual: 1.0
      l_inf_componentwise_dual_residual: 0.0
    })pb");
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats_bad_primal,
                                            TestLpBoundNorms()),
            std::nullopt);
}

TEST_P(IterateTerminationTest, NoTerminationWithBadDualResidual) {
  const auto stats_bad_dual = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 1.0
      l2_primal_residual: 0.0
      l2_dual_residual: 1.0
      l_inf_componentwise_primal_residual: 0.0
      l_inf_componentwise_dual_residual: 1.0
    })pb");
  EXPECT_EQ(CheckIterateTerminationCriteria(test_criteria_, stats_bad_dual,
                                            TestLpBoundNorms()),
            std::nullopt);
}

// Tests that optimality is checked with non-strict inequalities, as per the
// definitions in solvers.proto.
TEST_P(IterateTerminationTest, ZeroToleranceZeroError) {
  const auto stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      primal_objective: 1.0
      dual_objective: 1.0
      l_inf_primal_residual: 0.0
      l_inf_dual_residual: 0.0
      l2_primal_residual: 0.0
      l2_dual_residual: 0.0
      l_inf_componentwise_primal_residual: 0.0
      l_inf_componentwise_dual_residual: 0.0
      candidate_type: POINT_TYPE_CURRENT_ITERATE
    })pb");

  test_criteria_.mutable_simple_optimality_criteria()->set_eps_optimal_absolute(
      0.0);
  test_criteria_.mutable_simple_optimality_criteria()->set_eps_optimal_relative(
      0.0);

  std::optional<TerminationReasonAndPointType> maybe_result =
      CheckIterateTerminationCriteria(test_criteria_, stats,
                                      TestLpBoundNorms());
  EXPECT_THAT(maybe_result, Optional(FieldsAre(TERMINATION_REASON_OPTIMAL,
                                               POINT_TYPE_CURRENT_ITERATE)));
}

INSTANTIATE_TEST_SUITE_P(OptNorm, IterateTerminationTest,
                         testing::Values(OPTIMALITY_NORM_L_INF,
                                         OPTIMALITY_NORM_L2,
                                         OPTIMALITY_NORM_L_INF_COMPONENTWISE));

INSTANTIATE_TEST_SUITE_P(DetailedRelativeOptNorm,
                         DetailedRelativeTerminationTest,
                         testing::Values(OPTIMALITY_NORM_L_INF,
                                         OPTIMALITY_NORM_L2,
                                         OPTIMALITY_NORM_L_INF_COMPONENTWISE));
INSTANTIATE_TEST_SUITE_P(DetailedAbsoluteOptNorm,
                         DetailedAbsoluteTerminationTest,
                         testing::Values(OPTIMALITY_NORM_L_INF,
                                         OPTIMALITY_NORM_L2,
                                         OPTIMALITY_NORM_L_INF_COMPONENTWISE));

TEST(IterateTerminationTest, OptimalityNormsDiffer) {
  auto test_criteria = ParseTextOrDie<TerminationCriteria>(R"pb(
    simple_optimality_criteria { eps_optimal_relative: 1.0 })pb");

  // For L2, optimality requires norm(`primal_residual`, 2) <= 14.49
  // For L_inf, optimality requires norm(`primal_residual`, Inf) <= 12.0
  // For L_inf_componentwise, optimality requires norm(`primal_residual`) <= 1.0

  struct {
    double primal_residual;
    std::optional<TerminationReasonAndPointType> expected_l2;
    std::optional<TerminationReasonAndPointType> expected_l_inf;
    std::optional<TerminationReasonAndPointType> expected_l_inf_relative;
  } test_configs[] = {
      {0.5,
       TerminationReasonAndPointType{.reason = TERMINATION_REASON_OPTIMAL,
                                     .type = POINT_TYPE_CURRENT_ITERATE},
       TerminationReasonAndPointType{.reason = TERMINATION_REASON_OPTIMAL,
                                     .type = POINT_TYPE_CURRENT_ITERATE},
       TerminationReasonAndPointType{.reason = TERMINATION_REASON_OPTIMAL,
                                     .type = POINT_TYPE_CURRENT_ITERATE}},
      {10.0,
       TerminationReasonAndPointType{.reason = TERMINATION_REASON_OPTIMAL,
                                     .type = POINT_TYPE_CURRENT_ITERATE},
       TerminationReasonAndPointType{.reason = TERMINATION_REASON_OPTIMAL,
                                     .type = POINT_TYPE_CURRENT_ITERATE},
       std::nullopt},
      {13.0,
       TerminationReasonAndPointType{.reason = TERMINATION_REASON_OPTIMAL,
                                     .type = POINT_TYPE_CURRENT_ITERATE},
       std::nullopt, std::nullopt},
      {15.0, std::nullopt, std::nullopt, std::nullopt}};

  for (const auto& config : test_configs) {
    IterationStats stats;
    auto* convergence_info = stats.add_convergence_information();
    convergence_info->set_primal_objective(1.0);
    convergence_info->set_dual_objective(1.0);
    convergence_info->set_l_inf_primal_residual(config.primal_residual);
    convergence_info->set_l2_primal_residual(config.primal_residual);
    convergence_info->set_l_inf_componentwise_primal_residual(
        config.primal_residual);
    convergence_info->set_candidate_type(POINT_TYPE_CURRENT_ITERATE);

    test_criteria.set_optimality_norm(OPTIMALITY_NORM_L_INF);
    std::optional<TerminationReasonAndPointType> maybe_result =
        CheckIterateTerminationCriteria(test_criteria, stats,
                                        TestLpBoundNorms());
    ASSERT_TRUE(maybe_result.has_value() == config.expected_l_inf.has_value())
        << "primal_residual: " << config.primal_residual;
    if (config.expected_l_inf.has_value()) {
      EXPECT_EQ(maybe_result->reason, config.expected_l_inf->reason);
      EXPECT_EQ(maybe_result->type, config.expected_l_inf->type);
    }

    test_criteria.set_optimality_norm(OPTIMALITY_NORM_L2);

    maybe_result = CheckIterateTerminationCriteria(test_criteria, stats,
                                                   TestLpBoundNorms());
    ASSERT_TRUE(maybe_result.has_value() == config.expected_l2.has_value())
        << "primal_residual: " << config.primal_residual;
    if (config.expected_l2.has_value()) {
      EXPECT_EQ(maybe_result->reason, config.expected_l2->reason);
      EXPECT_EQ(maybe_result->type, config.expected_l2->type);
    }

    test_criteria.set_optimality_norm(OPTIMALITY_NORM_L_INF_COMPONENTWISE);
    maybe_result = CheckIterateTerminationCriteria(test_criteria, stats,
                                                   TestLpBoundNorms());
    ASSERT_TRUE(maybe_result.has_value() ==
                config.expected_l_inf_relative.has_value())
        << "primal_residual: " << config.primal_residual;
    if (config.expected_l_inf_relative.has_value()) {
      EXPECT_EQ(maybe_result->reason, config.expected_l_inf_relative->reason);
      EXPECT_EQ(maybe_result->type, config.expected_l_inf_relative->type);
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

TEST(EpsilonRatio, SimpleChecks) {
  EXPECT_EQ(EpsilonRatio(0.0, 0.0), 1.0);
  EXPECT_EQ(EpsilonRatio(1.0, 1.0), 1.0);
  EXPECT_EQ(EpsilonRatio(std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::infinity()),
            1.0);
  EXPECT_EQ(EpsilonRatio(1.0, 2.0), 0.5);
  EXPECT_EQ(EpsilonRatio(2.0, 1.0), 2.0);
  EXPECT_EQ(EpsilonRatio(0.0, std::numeric_limits<double>::infinity()), 0.0);
  EXPECT_EQ(EpsilonRatio(std::numeric_limits<double>::infinity(), 0.0),
            std::numeric_limits<double>::infinity());
}

TEST(ComputeRelativeResiduals,
     ComputesRelativeResidualsForZeroAbsoluteTolerance) {
  ConvergenceInformation stats;
  // If the absolute error tolerance is 0.0 and the relative error tolerance is
  // nonzero, the relative residuals are just the absolute residuals divided by
  // the corresponding norms (the actual nonzero value of the relative error
  // tolerance doesn't matter).
  stats.set_primal_objective(10.0);
  stats.set_dual_objective(5.0);
  stats.set_l_inf_primal_residual(1.0);
  stats.set_l2_primal_residual(1.0);
  stats.set_l_inf_dual_residual(1.0);
  stats.set_l2_dual_residual(1.0);
  TerminationCriteria termination_criteria;
  termination_criteria.mutable_simple_optimality_criteria()
      ->set_eps_optimal_absolute(0.0);
  termination_criteria.mutable_simple_optimality_criteria()
      ->set_eps_optimal_relative(1.0e-6);
  const RelativeConvergenceInformation relative_info = ComputeRelativeResiduals(
      EffectiveOptimalityCriteria(termination_criteria), stats,
      TestLpBoundNorms());

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
  // If the relative error tolerance is 0.0 and the absolute error tolerance
  // is nonzero, all of the relative residuals and the relative optimality gap
  // should be 0.0, no matter what the absolute error tolerance is.
  stats.set_primal_objective(10.0);
  stats.set_dual_objective(5.0);
  stats.set_l_inf_primal_residual(1.0);
  stats.set_l2_primal_residual(1.0);
  stats.set_l_inf_dual_residual(1.0);
  stats.set_l2_dual_residual(1.0);
  TerminationCriteria::SimpleOptimalityCriteria opt_criteria;
  opt_criteria.set_eps_optimal_absolute(1.0e-6);
  opt_criteria.set_eps_optimal_relative(0.0);
  const RelativeConvergenceInformation relative_info = ComputeRelativeResiduals(
      EffectiveOptimalityCriteria(opt_criteria), stats, TestLpBoundNorms());

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
  TerminationCriteria::SimpleOptimalityCriteria opt_criteria;
  opt_criteria.set_eps_optimal_absolute(1.0e-6);
  opt_criteria.set_eps_optimal_relative(1.0e-6);
  const RelativeConvergenceInformation relative_info = ComputeRelativeResiduals(
      EffectiveOptimalityCriteria(opt_criteria), stats, TestLpBoundNorms());

  EXPECT_EQ(relative_info.relative_l_inf_primal_residual, 1.0 / (1.0 + 12.0));
  EXPECT_EQ(relative_info.relative_l2_primal_residual,
            1.0 / (1.0 + std::sqrt(210.0)));

  EXPECT_EQ(relative_info.relative_l_inf_dual_residual, 1.0 / (1.0 + 5.5));
  EXPECT_EQ(relative_info.relative_l2_dual_residual, 1.0 / (1.0 + sqrt(36.25)));

  // The relative optimality gap should just be the objective difference divided
  // by 1.0 + the sum of absolute values.
  EXPECT_EQ(relative_info.relative_optimality_gap, 5.0 / (1.0 + 15.0));
}

TEST(ComputeRelativeResiduals,
     ComputesCorrectRelativeResidualsForBothTolerancesZero) {
  ConvergenceInformation stats;
  // If the absolute error tolerance and relative error tolerance are both zero,
  // the relative residuals are the same as when the tolerances are equal and
  // nonzero.
  stats.set_primal_objective(10.0);
  stats.set_dual_objective(5.0);
  stats.set_l_inf_primal_residual(1.0);
  stats.set_l2_primal_residual(1.0);
  stats.set_l_inf_dual_residual(1.0);
  stats.set_l2_dual_residual(1.0);
  TerminationCriteria::SimpleOptimalityCriteria opt_criteria;
  opt_criteria.set_eps_optimal_absolute(0.0);
  opt_criteria.set_eps_optimal_relative(0.0);
  const RelativeConvergenceInformation relative_info = ComputeRelativeResiduals(
      EffectiveOptimalityCriteria(opt_criteria), stats, TestLpBoundNorms());

  EXPECT_EQ(relative_info.relative_l_inf_primal_residual, 1.0 / (1.0 + 12.0));
  EXPECT_EQ(relative_info.relative_l2_primal_residual,
            1.0 / (1.0 + std::sqrt(210.0)));

  EXPECT_EQ(relative_info.relative_l_inf_dual_residual, 1.0 / (1.0 + 5.5));
  EXPECT_EQ(relative_info.relative_l2_dual_residual, 1.0 / (1.0 + sqrt(36.25)));

  // The relative optimality gap should just be the objective difference divided
  // by 1.0 + the sum of absolute values.
  EXPECT_EQ(relative_info.relative_optimality_gap, 5.0 / (1.0 + 15.0));
}

TEST(ComputeRelativeResiduals,
     ComputesCorrectRelativeResidualsForDetailedTerminationCriteria) {
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
  TerminationCriteria::DetailedOptimalityCriteria opt_criteria;
  opt_criteria.set_eps_optimal_primal_residual_absolute(2.0e-6);
  opt_criteria.set_eps_optimal_primal_residual_relative(2.0e-4);
  opt_criteria.set_eps_optimal_dual_residual_absolute(1.0e-3);
  opt_criteria.set_eps_optimal_dual_residual_relative(1.0e-4);
  opt_criteria.set_eps_optimal_objective_gap_absolute(3.0e-8);
  opt_criteria.set_eps_optimal_objective_gap_relative(3.0e-7);
  const RelativeConvergenceInformation relative_info =
      ComputeRelativeResiduals(opt_criteria, stats, TestLpBoundNorms());

  EXPECT_EQ(relative_info.relative_l_inf_primal_residual, 1.0 / (0.01 + 12.0));
  EXPECT_EQ(relative_info.relative_l2_primal_residual,
            1.0 / (0.01 + std::sqrt(210.0)));

  EXPECT_EQ(relative_info.relative_l_inf_dual_residual, 1.0 / (10.0 + 5.5));
  EXPECT_EQ(relative_info.relative_l2_dual_residual,
            1.0 / (10.0 + sqrt(36.25)));

  // The relative optimality gap should just be the objective difference divided
  // by 0.1 + the sum of absolute values.
  EXPECT_EQ(relative_info.relative_optimality_gap, 5.0 / (0.1 + 15.0));
}

TEST(ComputeRelativeResiduals,
     ComputesCorrectRelativeResidualsForInfiniteAbsoluteTolerances) {
  ConvergenceInformation stats;
  stats.set_primal_objective(10.0);
  stats.set_dual_objective(5.0);
  stats.set_l_inf_primal_residual(1.0);
  stats.set_l2_primal_residual(1.0);
  stats.set_l_inf_dual_residual(1.0);
  stats.set_l2_dual_residual(1.0);
  TerminationCriteria::SimpleOptimalityCriteria opt_criteria;
  opt_criteria.set_eps_optimal_absolute(
      std::numeric_limits<double>::infinity());
  opt_criteria.set_eps_optimal_relative(1.0e-6);
  const RelativeConvergenceInformation relative_info = ComputeRelativeResiduals(
      EffectiveOptimalityCriteria(opt_criteria), stats, TestLpBoundNorms());

  // If absolute tolerance is infinite the relative residuals are zero.
  EXPECT_EQ(relative_info.relative_l_inf_primal_residual, 0.0);
  EXPECT_EQ(relative_info.relative_l2_primal_residual, 0.0);
  EXPECT_EQ(relative_info.relative_l_inf_dual_residual, 0.0);
  EXPECT_EQ(relative_info.relative_l2_dual_residual, 0.0);
  EXPECT_EQ(relative_info.relative_optimality_gap, 0.0);
}

TEST(ComputeRelativeResiduals,
     ComputesCorrectRelativeResidualsForInfiniteRelativeTolerances) {
  ConvergenceInformation stats;
  stats.set_primal_objective(10.0);
  stats.set_dual_objective(5.0);
  stats.set_l_inf_primal_residual(1.0);
  stats.set_l2_primal_residual(1.0);
  stats.set_l_inf_dual_residual(1.0);
  stats.set_l2_dual_residual(1.0);
  TerminationCriteria::SimpleOptimalityCriteria opt_criteria;
  opt_criteria.set_eps_optimal_absolute(1.0e-6);
  opt_criteria.set_eps_optimal_relative(
      std::numeric_limits<double>::infinity());
  const RelativeConvergenceInformation relative_info = ComputeRelativeResiduals(
      EffectiveOptimalityCriteria(opt_criteria), stats, TestLpBoundNorms());

  EXPECT_EQ(relative_info.relative_l_inf_primal_residual, 1.0 / 12.0);
  EXPECT_EQ(relative_info.relative_l2_primal_residual, 1.0 / std::sqrt(210.0));

  EXPECT_EQ(relative_info.relative_l_inf_dual_residual, 1.0 / 5.5);
  EXPECT_EQ(relative_info.relative_l2_dual_residual, 1.0 / sqrt(36.25));

  // The relative optimality gap should just be the objective difference divided
  // by the sum of absolute values.
  EXPECT_EQ(relative_info.relative_optimality_gap, 5.0 / 15.0);
}

TEST(ComputeRelativeResiduals,
     ComputesCorrectRelativeResidualsForInfiniteAbsoluteAndRelativeTolerances) {
  ConvergenceInformation stats;
  // If the absolute error tolerance and relative error tolerance are both
  // infinity (and nonzero), the relative residuals are the absolute residuals
  // divided by 1.0 plus the corresponding norms.
  stats.set_primal_objective(10.0);
  stats.set_dual_objective(5.0);
  stats.set_l_inf_primal_residual(1.0);
  stats.set_l2_primal_residual(1.0);
  stats.set_l_inf_dual_residual(1.0);
  stats.set_l2_dual_residual(1.0);
  TerminationCriteria::SimpleOptimalityCriteria opt_criteria;
  opt_criteria.set_eps_optimal_absolute(
      std::numeric_limits<double>::infinity());
  opt_criteria.set_eps_optimal_relative(
      std::numeric_limits<double>::infinity());
  const RelativeConvergenceInformation relative_info = ComputeRelativeResiduals(
      EffectiveOptimalityCriteria(opt_criteria), stats, TestLpBoundNorms());

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
