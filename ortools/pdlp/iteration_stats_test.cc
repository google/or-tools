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

#include "ortools/pdlp/iteration_stats.h"

#include <cmath>
#include <limits>
#include <optional>
#include <utility>

#include "Eigen/Core"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"
#include "ortools/pdlp/test_util.h"

namespace operations_research::pdlp {
namespace {

using ::google::protobuf::util::ParseTextOrDie;

using ::testing::AllOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Ne;
using ::testing::SizeIs;

TEST(CorrectedDualTest, SimpleLpWithSuboptimalDual) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestLp(), num_threads, num_shards);

  // Set the primal variables that have primal gradients at their bounds, so
  // that the primal gradients are reduced costs.
  const Eigen::VectorXd primal_solution{{0, 0, 6, 2.5}};
  const Eigen::VectorXd dual_solution{{-2, 0, 2.375, 1}};
  const ConvergenceInformation stats = ComputeScaledConvergenceInformation(
      PrimalDualHybridGradientParams(), sharded_qp, primal_solution,
      dual_solution,
      /*componentwise_primal_residual_offset=*/1.0,
      /*componentwise_dual_residual_offset=*/1.0, POINT_TYPE_CURRENT_ITERATE);
  // -36.5 = -14 - 24 - 9.5 - 1 - 3 + 15
  EXPECT_DOUBLE_EQ(stats.dual_objective(), -36.5);
  EXPECT_DOUBLE_EQ(stats.corrected_dual_objective(), -36.5);
}

// This is similar to `SimpleLpWithSuboptimalDual`, except with
// x_2 = 2. In the dual correction calculation, the corresponding bound is 6, so
// the primal gradient will be treated as a residual of 0.5 instead of a dual
// correction of -3, but in the corrected dual objective it is still treated as
// a dual correction.
TEST(CorrectedDualTest, SimpleLpWithVariableFarFromBoundAsResiduals) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestLp(), num_threads, num_shards);

  const Eigen::VectorXd primal_solution{{0, 0, 2, 2.5}};
  const Eigen::VectorXd dual_solution{{-2, 0, 2.375, 1}};
  PrimalDualHybridGradientParams params;
  params.set_handle_some_primal_gradients_on_finite_bounds_as_residuals(true);
  const ConvergenceInformation stats = ComputeScaledConvergenceInformation(
      params, sharded_qp, primal_solution, dual_solution,
      /*componentwise_primal_residual_offset=*/1.0,
      /*componentwise_dual_residual_offset=*/1.0, POINT_TYPE_CURRENT_ITERATE);
  // -33.5 = -14 - 24 - 9.5 - 1 + 15
  EXPECT_DOUBLE_EQ(stats.dual_objective(), -33.5);
  EXPECT_DOUBLE_EQ(stats.corrected_dual_objective(), -36.5);
  EXPECT_DOUBLE_EQ(stats.l_inf_dual_residual(), 0.5);
  EXPECT_DOUBLE_EQ(stats.l2_dual_residual(), 0.5);
  EXPECT_DOUBLE_EQ(stats.l_inf_componentwise_dual_residual(), 0.25);
}

TEST(CorrectedDualTest, SimpleLpWithVariableFarFromBoundAsReducedCosts) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestLp(), num_threads, num_shards);

  const Eigen::VectorXd primal_solution{{0, 0, 2, 2.5}};
  const Eigen::VectorXd dual_solution{{-2, 0, 2.375, 1}};
  PrimalDualHybridGradientParams params;
  params.set_handle_some_primal_gradients_on_finite_bounds_as_residuals(false);
  const ConvergenceInformation stats = ComputeScaledConvergenceInformation(
      params, sharded_qp, primal_solution, dual_solution,
      /*componentwise_primal_residual_offset=*/1.0,
      /*componentwise_dual_residual_offset=*/1.0, POINT_TYPE_CURRENT_ITERATE);
  // -36.5 = -14 - 24 - 9.5 - 1 - 3 + 15
  EXPECT_DOUBLE_EQ(stats.dual_objective(), -36.5);
  EXPECT_DOUBLE_EQ(stats.corrected_dual_objective(), -36.5);
  EXPECT_DOUBLE_EQ(stats.l_inf_dual_residual(), 0.0);
  EXPECT_DOUBLE_EQ(stats.l2_dual_residual(), 0.0);
  EXPECT_DOUBLE_EQ(stats.l_inf_componentwise_dual_residual(), 0.0);
}

TEST(CorrectedDualObjective, QpSuboptimal) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);

  const Eigen::VectorXd primal_solution{{-2.0, 2.0}};
  const Eigen::VectorXd dual_solution{{-3}};
  const ConvergenceInformation stats = ComputeScaledConvergenceInformation(
      PrimalDualHybridGradientParams(), sharded_qp, primal_solution,
      dual_solution,
      /*componentwise_primal_residual_offset=*/1.0,
      /*componentwise_dual_residual_offset=*/1.0, POINT_TYPE_CURRENT_ITERATE);
  // primal gradient vector: [-6, 4]
  // Constant term: 5
  // Quadratic term: -(16+4)/2 = -10
  // Dual objective term: -3 * 1
  // Primal variables at bounds term: 2*-6 + -2*4 = -20
  // -28.0 = 5 - 10 - 3 - 20
  EXPECT_DOUBLE_EQ(stats.corrected_dual_objective(), -28.0);
}

TEST(RandomProjectionsTest, OneRandomProjectionsOfZeroVector) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestLp(), num_threads, num_shards);

  PointMetadata metadata;
  SetRandomProjections(sharded_qp, /*primal_solution=*/Eigen::VectorXd::Zero(4),
                       /*dual_solution=*/Eigen::VectorXd::Zero(4),
                       /*random_projection_seeds=*/{1}, metadata);
  EXPECT_THAT(metadata.random_primal_projections(), ElementsAre(0.0));
  EXPECT_THAT(metadata.random_dual_projections(), ElementsAre(0.0));
}

TEST(RandomProjectionsTest, TwoRandomProjectionsOfVector) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestLp(), num_threads, num_shards);

  PointMetadata metadata;
  SetRandomProjections(sharded_qp, /*primal_solution=*/Eigen::VectorXd::Ones(4),
                       /*dual_solution=*/Eigen::VectorXd::Zero(4),
                       /*random_projection_seeds=*/{1, 2}, metadata);
  EXPECT_THAT(metadata.random_primal_projections(), SizeIs(2));
  EXPECT_THAT(metadata.random_dual_projections(), SizeIs(2));
  // The primal solution has norm 2; the random projection should only reduce
  // the norm. Obtaining 0.0 is a probability-zero event.
  EXPECT_THAT(metadata.random_primal_projections(),
              Each(AllOf(Ge(-2.0), Le(2.0), Ne(0.0))));
  EXPECT_THAT(metadata.random_dual_projections(), Each(Eq(0.0)));
}

TEST(ReducedCostsTest, SimpleLp) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestLp(), num_threads, num_shards);

  // Use a primal solution at the relevant bounds, to ensure handling as
  // reduced costs.
  const Eigen::VectorXd primal_solution{{0.0, -2.0, 6.0, 3.5}};
  const Eigen::VectorXd dual_solution{{1.0, 0.0, 0.0, -2.0}};
  // c is: [5.5, -2, -1, 1]
  // -A^T y is: [-2, -1, 2, -4]
  // c - A^T y is: [3.5, -3.0, 1.0, -3.0].
  EXPECT_THAT(ReducedCosts(PrimalDualHybridGradientParams(), sharded_qp,
                           primal_solution, dual_solution),
              ElementsAre(3.5, -3.0, 1.0, -3.0));
  EXPECT_THAT(ReducedCosts(PrimalDualHybridGradientParams(), sharded_qp,
                           primal_solution, dual_solution,
                           /*use_zero_primal_objective=*/true),
              ElementsAre(-2.0, -1.0, 2.0, -4.0));
}

TEST(ReducedCostsTest, SimpleQp) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);

  const Eigen::VectorXd primal_solution{{1.0, 2.0}};
  const Eigen::VectorXd dual_solution{{0.0}};
  // Q*x is: [4.0, 2.0]
  // c is: [-1, -1]
  // A^T y is zero.
  // If `handle_some_primal_gradients_on_finite_bounds_as_residuals` is
  // true the second primal gradient term is handled as a residual, not a
  // reduced cost.
  EXPECT_THAT(ReducedCosts(PrimalDualHybridGradientParams(), sharded_qp,
                           primal_solution, dual_solution),
              ElementsAre(3.0, 1.0));
  EXPECT_THAT(ReducedCosts(PrimalDualHybridGradientParams(), sharded_qp,
                           primal_solution, dual_solution,
                           /*use_zero_primal_objective=*/true),
              ElementsAre(0.0, 0.0));
}

TEST(GetConvergenceInformation, GetsCorrectEntry) {
  const auto test_stats = ParseTextOrDie<IterationStats>(R"pb(
    convergence_information {
      candidate_type: POINT_TYPE_CURRENT_ITERATE
      primal_objective: 1.0
    }
    convergence_information {
      candidate_type: POINT_TYPE_AVERAGE_ITERATE
      primal_objective: 2.0
    }
  )pb");
  const auto average_info =
      GetConvergenceInformation(test_stats, POINT_TYPE_AVERAGE_ITERATE);
  ASSERT_TRUE(average_info.has_value());
  EXPECT_EQ(average_info->candidate_type(), POINT_TYPE_AVERAGE_ITERATE);
  EXPECT_EQ(average_info->primal_objective(), 2.0);

  const auto current_info =
      GetConvergenceInformation(test_stats, POINT_TYPE_CURRENT_ITERATE);
  ASSERT_TRUE(current_info.has_value());
  EXPECT_EQ(current_info->candidate_type(), POINT_TYPE_CURRENT_ITERATE);
  EXPECT_EQ(current_info->primal_objective(), 1.0);

  EXPECT_THAT(
      GetConvergenceInformation(test_stats, POINT_TYPE_ITERATE_DIFFERENCE),
      Eq(std::nullopt));
}

TEST(GetInfeasibilityInformation, GetsCorrectEntry) {
  const auto test_stats = ParseTextOrDie<IterationStats>(R"pb(
    infeasibility_information {
      candidate_type: POINT_TYPE_CURRENT_ITERATE
      primal_ray_linear_objective: 1.0
    }
    infeasibility_information {
      candidate_type: POINT_TYPE_AVERAGE_ITERATE
      primal_ray_linear_objective: 2.0
    }
  )pb");
  const auto average_info =
      GetInfeasibilityInformation(test_stats, POINT_TYPE_AVERAGE_ITERATE);
  ASSERT_TRUE(average_info.has_value());
  EXPECT_EQ(average_info->candidate_type(), POINT_TYPE_AVERAGE_ITERATE);
  EXPECT_EQ(average_info->primal_ray_linear_objective(), 2.0);

  const auto current_info =
      GetInfeasibilityInformation(test_stats, POINT_TYPE_CURRENT_ITERATE);
  ASSERT_TRUE(current_info.has_value());
  EXPECT_EQ(current_info->candidate_type(), POINT_TYPE_CURRENT_ITERATE);
  EXPECT_EQ(current_info->primal_ray_linear_objective(), 1.0);

  EXPECT_THAT(
      GetInfeasibilityInformation(test_stats, POINT_TYPE_ITERATE_DIFFERENCE),
      Eq(std::nullopt));
}

TEST(GetPointMetadata, GetsCorrectEntry) {
  const auto test_stats = ParseTextOrDie<IterationStats>(R"pb(
    point_metadata {
      point_type: POINT_TYPE_CURRENT_ITERATE
      active_primal_variable_count: 1
    }
    point_metadata {
      point_type: POINT_TYPE_AVERAGE_ITERATE
      active_primal_variable_count: 2
    }
  )pb");
  const auto average_info =
      GetPointMetadata(test_stats, POINT_TYPE_AVERAGE_ITERATE);
  ASSERT_TRUE(average_info.has_value());
  EXPECT_EQ(average_info->point_type(), POINT_TYPE_AVERAGE_ITERATE);
  EXPECT_EQ(average_info->active_primal_variable_count(), 2);

  const auto current_info =
      GetPointMetadata(test_stats, POINT_TYPE_CURRENT_ITERATE);
  ASSERT_TRUE(current_info.has_value());
  EXPECT_EQ(current_info->point_type(), POINT_TYPE_CURRENT_ITERATE);
  EXPECT_EQ(current_info->active_primal_variable_count(), 1);

  EXPECT_THAT(GetPointMetadata(test_stats, POINT_TYPE_ITERATE_DIFFERENCE),
              Eq(std::nullopt));
}

}  // namespace
}  // namespace operations_research::pdlp
