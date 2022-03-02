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

#include "ortools/pdlp/solvers_proto_validation.h"

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ortools/pdlp/solvers.pb.h"

namespace operations_research::pdlp {
namespace {

using ::testing::HasSubstr;

TEST(ValidateTerminationCriteria, DefaultIsValid) {
  TerminationCriteria criteria;
  const absl::Status status = ValidateTerminationCriteria(criteria);
  EXPECT_TRUE(status.ok()) << status;
}

TEST(ValidateTerminationCriteria, BadOptimalityNorm) {
  TerminationCriteria criteria;
  criteria.set_optimality_norm(OPTIMALITY_NORM_UNSPECIFIED);
  const absl::Status status = ValidateTerminationCriteria(criteria);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("optimality_norm"));
}

TEST(ValidateTerminationCriteria, BadEpsOptimalAbsolute) {
  TerminationCriteria criteria;
  criteria.set_eps_optimal_absolute(-1.0);
  const absl::Status status = ValidateTerminationCriteria(criteria);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("eps_optimal_absolute"));
}

TEST(ValidateTerminationCriteria, BadEpsOptimalRelative) {
  TerminationCriteria criteria;
  criteria.set_eps_optimal_relative(-1.0);
  const absl::Status status = ValidateTerminationCriteria(criteria);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("eps_optimal_relative"));
}

TEST(ValidateTerminationCriteria, BadEpsPriamlInfeasible) {
  TerminationCriteria criteria;
  criteria.set_eps_primal_infeasible(-1.0);
  const absl::Status status = ValidateTerminationCriteria(criteria);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("eps_primal_infeasible"));
}

TEST(ValidateTerminationCriteria, BadEpsDualInfeasible) {
  TerminationCriteria criteria;
  criteria.set_eps_dual_infeasible(-1.0);
  const absl::Status status = ValidateTerminationCriteria(criteria);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("eps_dual_infeasible"));
}

TEST(ValidateTerminationCriteria, BadTimeSecLimit) {
  TerminationCriteria criteria;
  criteria.set_time_sec_limit(-1.0);
  const absl::Status status = ValidateTerminationCriteria(criteria);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("time_sec_limit"));
}

TEST(ValidateTerminationCriteria, BadIterationLimit) {
  TerminationCriteria criteria;
  criteria.set_iteration_limit(-1);
  const absl::Status status = ValidateTerminationCriteria(criteria);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("iteration_limit"));
}

TEST(ValidateTerminationCriteria, BadKktMatrixPassLimit) {
  TerminationCriteria criteria;
  criteria.set_kkt_matrix_pass_limit(-1.0);
  const absl::Status status = ValidateTerminationCriteria(criteria);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("kkt_matrix_pass_limit"));
}

TEST(ValidateAdaptiveLinesearchParams, DefaultIsValid) {
  AdaptiveLinesearchParams params;
  const absl::Status status = ValidateAdaptiveLinesearchParams(params);
  EXPECT_TRUE(status.ok()) << status;
}

TEST(ValidateAdaptiveLinesearchParams, BadReductionExponent) {
  AdaptiveLinesearchParams params;
  params.set_step_size_reduction_exponent(0.0);
  const absl::Status status = ValidateAdaptiveLinesearchParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("step_size_reduction_exponent"));
}

TEST(ValidateAdaptiveLinesearchParams, BadGrowthExponent) {
  AdaptiveLinesearchParams params;
  params.set_step_size_growth_exponent(0.0);
  const absl::Status status = ValidateAdaptiveLinesearchParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("step_size_growth_exponent"));
}

TEST(ValidateMalitskyPockParams, DefaultIsValid) {
  MalitskyPockParams params;
  const absl::Status status = ValidateMalitskyPockParams(params);
  EXPECT_TRUE(status.ok()) << status;
}

TEST(ValidateMalitskyPockParams, BadDownscalingFactor) {
  MalitskyPockParams params0;
  params0.set_step_size_downscaling_factor(0.0);
  const absl::Status status0 = ValidateMalitskyPockParams(params0);
  EXPECT_EQ(status0.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status0.message(), HasSubstr("step_size_downscaling_factor"));

  MalitskyPockParams params1;
  params1.set_step_size_downscaling_factor(1.0);
  const absl::Status status1 = ValidateMalitskyPockParams(params1);
  EXPECT_EQ(status1.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status1.message(), HasSubstr("step_size_downscaling_factor"));
}

TEST(ValidateMalitskyPockParams, BadConstractionFactor) {
  MalitskyPockParams params0;
  params0.set_linesearch_contraction_factor(0.0);
  const absl::Status status0 = ValidateMalitskyPockParams(params0);
  EXPECT_EQ(status0.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status0.message(), HasSubstr("linesearch_contraction_factor"));

  MalitskyPockParams params1;
  params1.set_linesearch_contraction_factor(1.0);
  const absl::Status status1 = ValidateMalitskyPockParams(params1);
  EXPECT_EQ(status1.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status1.message(), HasSubstr("linesearch_contraction_factor"));
}

TEST(ValidateMalitskyPockParams, BadStepSizeInterpolation) {
  MalitskyPockParams params;
  params.set_step_size_interpolation(-1.0);
  const absl::Status status = ValidateMalitskyPockParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("step_size_interpolation"));
}

TEST(ValidatePrimalDualHybridGradientParams, DefaultIsValid) {
  PrimalDualHybridGradientParams params;
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_TRUE(status.ok()) << status;
}

TEST(ValidatePrimalDualHybridGradientParams, BadTerminationCriteria) {
  PrimalDualHybridGradientParams params;
  params.mutable_termination_criteria()->set_eps_dual_infeasible(-1);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("eps_dual_infeasible"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadNumThreads) {
  PrimalDualHybridGradientParams params;
  params.set_num_threads(0);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("num_threads"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadVerbosityLevel) {
  PrimalDualHybridGradientParams params;
  params.set_verbosity_level(-1);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("verbosity_level"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadMajorIterationFrequency) {
  PrimalDualHybridGradientParams params;
  params.set_major_iteration_frequency(0);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("major_iteration_frequency"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadTerminationCheckFrequency) {
  PrimalDualHybridGradientParams params;
  params.set_termination_check_frequency(0);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("termination_check_frequency"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadRestartStrategy) {
  PrimalDualHybridGradientParams params;
  params.set_restart_strategy(
      PrimalDualHybridGradientParams::RESTART_STRATEGY_UNSPECIFIED);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("restart_strategy"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadPrimalWeightUpdateSmoothing) {
  PrimalDualHybridGradientParams params_high;
  params_high.set_primal_weight_update_smoothing(1.1);
  const absl::Status status_high =
      ValidatePrimalDualHybridGradientParams(params_high);
  EXPECT_EQ(status_high.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status_high.message(),
              HasSubstr("primal_weight_update_smoothing"));

  PrimalDualHybridGradientParams params_low;
  params_low.set_primal_weight_update_smoothing(-0.1);
  const absl::Status status_low =
      ValidatePrimalDualHybridGradientParams(params_low);
  EXPECT_EQ(status_low.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status_low.message(),
              HasSubstr("primal_weight_update_smoothing"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadInitialPrimalWeight) {
  PrimalDualHybridGradientParams params;
  params.set_initial_primal_weight(-1);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("initial_primal_weight"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadLInfRuizIterations) {
  PrimalDualHybridGradientParams params;
  params.set_l_inf_ruiz_iterations(-1);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("l_inf_ruiz_iterations"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadSufficientReductionForRestart) {
  PrimalDualHybridGradientParams params_high;
  params_high.set_sufficient_reduction_for_restart(1.0);
  const absl::Status status_high =
      ValidatePrimalDualHybridGradientParams(params_high);
  EXPECT_EQ(status_high.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status_high.message(),
              HasSubstr("sufficient_reduction_for_restart"));

  PrimalDualHybridGradientParams params_low;
  params_low.set_sufficient_reduction_for_restart(0.0);
  const absl::Status status_low =
      ValidatePrimalDualHybridGradientParams(params_low);
  EXPECT_EQ(status_low.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status_low.message(),
              HasSubstr("sufficient_reduction_for_restart"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadNecessaryReductionForRestart) {
  PrimalDualHybridGradientParams params_high;
  params_high.set_necessary_reduction_for_restart(1.0);
  const absl::Status status_high =
      ValidatePrimalDualHybridGradientParams(params_high);
  EXPECT_EQ(status_high.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status_high.message(),
              HasSubstr("necessary_reduction_for_restart"));

  PrimalDualHybridGradientParams params_low;
  params_low.set_sufficient_reduction_for_restart(0.5);
  params_low.set_necessary_reduction_for_restart(0.4);
  const absl::Status status_low =
      ValidatePrimalDualHybridGradientParams(params_low);
  EXPECT_EQ(status_low.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status_low.message(),
              HasSubstr("necessary_reduction_for_restart"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadLinesearchRule) {
  PrimalDualHybridGradientParams params;
  params.set_linesearch_rule(
      PrimalDualHybridGradientParams::LINESEARCH_RULE_UNSPECIFIED);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("linesearch_rule"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadAdaptiveLinesearchParameters) {
  PrimalDualHybridGradientParams params;
  params.mutable_adaptive_linesearch_parameters()
      ->set_step_size_reduction_exponent(-1);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("step_size_reduction_exponent"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadMalitskyPockParameters) {
  PrimalDualHybridGradientParams params;
  params.mutable_malitsky_pock_parameters()->set_linesearch_contraction_factor(
      -1);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("linesearch_contraction_factor"));
}

TEST(ValidatePrimalDualHybridGradientParams, BadInitialStepSizeScaling) {
  PrimalDualHybridGradientParams params;
  params.set_initial_step_size_scaling(-1.0);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("initial_step_size_scaling"));
}

TEST(ValidatePrimalDualHybridGradientParams,
     BadInfiniteConstraintBoundThreshold) {
  PrimalDualHybridGradientParams params;
  params.set_infinite_constraint_bound_threshold(-1.0);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(),
              HasSubstr("infinite_constraint_bound_threshold"));
}

TEST(ValidatePrimalDualHybridGradientParams,
     BadDiagonalTrustRegionSolverTolerance) {
  PrimalDualHybridGradientParams params;
  params.set_diagonal_qp_trust_region_solver_tolerance(-1.0);
  const absl::Status status = ValidatePrimalDualHybridGradientParams(params);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(),
              HasSubstr("diagonal_qp_trust_region_solver_tolerance"));
}

}  // namespace
}  // namespace operations_research::pdlp
