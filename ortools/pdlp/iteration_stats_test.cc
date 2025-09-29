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
#include "ortools/base/parse_text_proto.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/solvers.pb.h"
#include "ortools/pdlp/test_util.h"

namespace operations_research::pdlp {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTextOrDie;

using ::testing::AllOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::EqualsProto;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Ne;
using ::testing::SizeIs;
using ::testing::proto::Approximately;
using ::testing::proto::Partially;

// The following block relies heavily on `EqualsProto`, which isn't open source.

void CheckScaledAndUnscaledConvergenceInformation(
    QuadraticProgram qp, const Eigen::VectorXd& primal_solution,
    const Eigen::VectorXd& dual_solution,
    const double componentwise_primal_residual_offset,
    const double componentwise_dual_residual_offset,
    const ConvergenceInformation& expected_stats) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(std::move(qp), num_threads, num_shards);
  EXPECT_THAT(
      ComputeScaledConvergenceInformation(
          PrimalDualHybridGradientParams(), sharded_qp, primal_solution,
          dual_solution, componentwise_primal_residual_offset,
          componentwise_dual_residual_offset, POINT_TYPE_CURRENT_ITERATE),
      Partially(Approximately(EqualsProto(expected_stats))));

  // Rescale the problem so that the primal and dual solutions have elements
  // equal to -1.0, 0.0, or 1.0.
  Eigen::VectorXd col_scaling_vec = primal_solution.unaryExpr(
      [](double x) { return x != 0.0 ? std::abs(x) : 1.0; });
  Eigen::VectorXd row_scaling_vec = dual_solution.unaryExpr(
      [](double x) { return x != 0.0 ? std::abs(x) : 1.0; });
  Eigen::VectorXd scaled_primal_solution =
      primal_solution.cwiseQuotient(col_scaling_vec);
  Eigen::VectorXd scaled_dual_solution =
      dual_solution.cwiseQuotient(row_scaling_vec);
  sharded_qp.RescaleQuadraticProgram(col_scaling_vec, row_scaling_vec);
  EXPECT_THAT(
      ComputeConvergenceInformation(
          PrimalDualHybridGradientParams(), sharded_qp, col_scaling_vec,
          row_scaling_vec, scaled_primal_solution, scaled_dual_solution,
          componentwise_primal_residual_offset,
          componentwise_dual_residual_offset, POINT_TYPE_CURRENT_ITERATE),
      Partially(Approximately(EqualsProto(expected_stats))));

  // Also check that the iteration stats for the scaled problem have the correct
  // objectives and norms.
  ConvergenceInformation expected_scaled_stats;
  expected_scaled_stats.set_primal_objective(expected_stats.primal_objective());
  expected_scaled_stats.set_dual_objective(expected_stats.dual_objective());
  expected_scaled_stats.set_l_inf_primal_variable(1.0);
  expected_scaled_stats.set_l_inf_dual_variable(1.0);

  EXPECT_THAT(
      ComputeScaledConvergenceInformation(
          PrimalDualHybridGradientParams(), sharded_qp, scaled_primal_solution,
          scaled_dual_solution, componentwise_primal_residual_offset,
          componentwise_dual_residual_offset, POINT_TYPE_CURRENT_ITERATE),
      Partially(Approximately(EqualsProto(expected_scaled_stats))));
}

void CheckScaledAndUnscaledInfeasibilityStats(
    QuadraticProgram qp, const Eigen::VectorXd& primal_ray,
    const Eigen::VectorXd& dual_ray,
    const Eigen::VectorXd& primal_solution_for_residual_tests,
    const InfeasibilityInformation& expected_infeasibility_info) {
  const int num_threads = 2;
  const int num_shards = 2;
  ShardedQuadraticProgram sharded_qp(std::move(qp), num_threads, num_shards);
  EXPECT_THAT(
      ComputeInfeasibilityInformation(
          PrimalDualHybridGradientParams(), sharded_qp,
          Eigen::VectorXd::Ones(sharded_qp.PrimalSize()),
          Eigen::VectorXd::Ones(sharded_qp.DualSize()), primal_ray, dual_ray,
          primal_solution_for_residual_tests, POINT_TYPE_CURRENT_ITERATE),
      Partially(Approximately(EqualsProto(expected_infeasibility_info))));

  // Rescale the problem so that the primal and dual certificates have elements
  // equal to -1.0, 0.0, or 1.0.
  Eigen::VectorXd col_scaling_vec = primal_ray.unaryExpr(
      [](double x) { return x != 0.0 ? std::abs(x) : 1.0; });
  Eigen::VectorXd row_scaling_vec =
      dual_ray.unaryExpr([](double x) { return x != 0.0 ? std::abs(x) : 1.0; });
  Eigen::VectorXd scaled_primal_solution =
      primal_ray.cwiseQuotient(col_scaling_vec);
  Eigen::VectorXd scaled_dual_solution =
      dual_ray.cwiseQuotient(row_scaling_vec);
  Eigen::VectorXd scaled_primal_solution_for_residual_tests =
      primal_solution_for_residual_tests.cwiseQuotient(col_scaling_vec);
  sharded_qp.RescaleQuadraticProgram(col_scaling_vec, row_scaling_vec);
  EXPECT_THAT(
      ComputeInfeasibilityInformation(
          PrimalDualHybridGradientParams(), sharded_qp, col_scaling_vec,
          row_scaling_vec, scaled_primal_solution, scaled_dual_solution,
          scaled_primal_solution_for_residual_tests,
          POINT_TYPE_CURRENT_ITERATE),
      Partially(Approximately(EqualsProto(expected_infeasibility_info))));
}

TEST(IterationStatsTest, SimpleLpAtOptimum) {
  const Eigen::VectorXd primal_solution{{-1.0, 8.0, 1.0, 2.5}};
  const Eigen::VectorXd dual_solution{{-2.0, 0.0, 2.375, 2.0 / 3}};
  CheckScaledAndUnscaledConvergenceInformation(
      TestLp(), primal_solution, dual_solution,
      /*componentwise_primal_residual_offset=*/1.0,
      /*componentwise_dual_residual_offset=*/1.0,
      ParseTextOrDie<ConvergenceInformation>(R"pb(
        primal_objective: -34.0
        dual_objective: -34.0
        corrected_dual_objective: -34.0
        l_inf_primal_residual: 0.0
        l2_primal_residual: 0.0
        l_inf_componentwise_primal_residual: 0.0
        l_inf_dual_residual: 0.0
        l2_dual_residual: 0.0
        l_inf_componentwise_dual_residual: 0.0
        l_inf_primal_variable: 8.0
        l2_primal_variable: 8.5
        l_inf_dual_variable: 2.375
        l2_dual_variable: 3.1756998353818715
      )pb"));
}

TEST(IterationStatsTest, SimpleLpWithPrimalResidual) {
  // This is the optimal solution, except that x_3 (`primal_solution[3]`) has
  // been changed from 2.5 to 3.5, increasing the objective by 1, but causing
  // the first constraint to be violated by 2 and the last constraint by 1.
  const Eigen::VectorXd primal_solution{{-1.0, 8.0, 1.0, 3.5}};
  const Eigen::VectorXd dual_solution{{-2.0, 0.0, 2.375, 2.0 / 3}};
  CheckScaledAndUnscaledConvergenceInformation(
      TestLp(), primal_solution, dual_solution,
      /*componentwise_primal_residual_offset=*/1.0,
      /*componentwise_dual_residual_offset=*/1.0,
      ParseTextOrDie<ConvergenceInformation>(R"pb(
        primal_objective: -33.0
        dual_objective: -34.0
        corrected_dual_objective: -34.0
        l_inf_primal_residual: 2.0
        l2_primal_residual: 2.2360679774997896
        l_inf_componentwise_primal_residual: 0.5
        l_inf_dual_residual: 0.0
        l2_dual_residual: 0.0
        l_inf_componentwise_dual_residual: 0.0
        l_inf_primal_variable: 8.0
        l2_primal_variable: 8.8459030064770662
        l_inf_dual_variable: 2.375
        l2_dual_variable: 3.1756998353818715
      )pb"));
}

TEST(IterationStatsTest, SimpleLpWithDualResidual) {
  // This is the optimal solution, except that y_1 (`dual_solution[1]`) has been
  // changed from 0 to -1, causing x_0 and x_2 to have primal gradients (dual
  // residuals) of 1.0.
  const Eigen::VectorXd primal_solution{{-1.0, 8.0, 1.0, 2.5}};
  const Eigen::VectorXd dual_solution{{-2.0, -1.0, 2.375, 2.0 / 3}};
  CheckScaledAndUnscaledConvergenceInformation(
      TestLp(), primal_solution, dual_solution,
      /*componentwise_primal_residual_offset=*/1.0,
      /*componentwise_dual_residual_offset=*/1.0,
      ParseTextOrDie<ConvergenceInformation>(R"pb(
        primal_objective: -34.0
        dual_objective: -41.0
        corrected_dual_objective: -inf
        l_inf_primal_residual: 0.0
        l2_primal_residual: 0.0
        l_inf_componentwise_primal_residual: 0.0
        l_inf_dual_residual: 1.0
        l2_dual_residual: 1.4142135623730950
        l_inf_componentwise_dual_residual: 0.5
        l_inf_primal_variable: 8.0
        l2_primal_variable: 8.5
        l_inf_dual_variable: 2.375
        l2_dual_variable: 3.3294247918288294
      )pb"));
}

TEST(IterationStatsTest, SimpleLpWithBothResiduals) {
  // This is the optimal solution, except that x_3 (`primal_solution[3]`) has
  // been changed from 2.5 to 3.5, increasing the objective by 1, but causing
  // the first constraint to be violated by 2 and the last constraint by 1, and
  // y_1 (`dual_solution[1]`) has been changed from 0 to -1, causing x_0 and x_2
  // to have primal gradients (dual residuals) of 1.0. The primal and dual
  // componentwise_residual_offset values are different, to check that the
  // correct offset is applied when computing the
  // l_inf_componentwise_XXX_residual values.
  const Eigen::VectorXd primal_solution{{-1.0, 8.0, 1.0, 3.5}};
  const Eigen::VectorXd dual_solution{{-2.0, -1.0, 2.375, 2.0 / 3}};
  CheckScaledAndUnscaledConvergenceInformation(
      TestLp(), primal_solution, dual_solution,
      /*componentwise_primal_residual_offset=*/3.0,
      /*componentwise_dual_residual_offset=*/1.0,
      ParseTextOrDie<ConvergenceInformation>(R"pb(
        primal_objective: -33.0
        dual_objective: -41.0
        corrected_dual_objective: -inf
        l_inf_primal_residual: 2.0
        l2_primal_residual: 2.2360679774997896
        l_inf_componentwise_primal_residual: 0.25
        l_inf_dual_residual: 1.0
        l2_dual_residual: 1.4142135623730950
        l_inf_componentwise_dual_residual: 0.5
        l_inf_primal_variable: 8.0
        l2_primal_variable: 8.8459030064770662
        l_inf_dual_variable: 2.375
        l2_dual_variable: 3.3294247918288294
      )pb"));
}

TEST(IterationStatsTest, SimpleQpAtOptimum) {
  const Eigen::VectorXd primal_solution{{1.0, 0.0}};
  const Eigen::VectorXd dual_solution{{-1.0}};
  CheckScaledAndUnscaledConvergenceInformation(
      TestDiagonalQp1(), primal_solution, dual_solution,
      /*componentwise_primal_residual_offset=*/1.0,
      /*componentwise_dual_residual_offset=*/1.0,
      ParseTextOrDie<ConvergenceInformation>(R"pb(
        primal_objective: 6.0
        dual_objective: 6.0
        corrected_dual_objective: 6.0
        l_inf_primal_residual: 0.0
        l2_primal_residual: 0.0
        l_inf_componentwise_primal_residual: 0.0
        l_inf_dual_residual: 0.0
        l2_dual_residual: 0.0
        l_inf_componentwise_dual_residual: 0.0
        l_inf_primal_variable: 1.0
        l2_primal_variable: 1.0
        l_inf_dual_variable: 1.0
        l2_dual_variable: 1.0
      )pb"));
}

TEST(IterationStatsTest, SimpleLpWithGapResidualsAndZeroPrimalSolution) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestLp(), num_threads, num_shards);

  const Eigen::VectorXd primal_solution = Eigen::VectorXd::Zero(4);
  const Eigen::VectorXd dual_solution{{1.0, 0.0, 0.0, -1.0}};

  PrimalDualHybridGradientParams params_true, params_false;
  params_true.set_handle_some_primal_gradients_on_finite_bounds_as_residuals(
      true);
  params_false.set_handle_some_primal_gradients_on_finite_bounds_as_residuals(
      false);

  // c is: [5.5, -2, -1, 1]
  // -A^T y is: [-2, -1, 0.5, -3]
  // c - A^T y is: [3.5, -3.0, -0.5, -2.0].
  // When the primal variable is 0.0 and the bound is not 0.0, the bound
  // corresponding to c - A^T y is handled as infinite when
  // `handle_some_primal_gradients_on_finite_bounds_as_residuals` is true.
  // Thus, for the all zero primal solution: when
  // `handle_some_primal_gradients_on_finite_bounds_as_residuals` is true, the
  // residuals are [3.5, -3.0, -0.5, -2.0] and all bounds are treated as
  // infinite. When `handle_some_primal_gradients_on_finite_bounds_as_residuals`
  // is false, the residuals are [3.5, -3.0, 0, 0] and the corresponding bound
  // terms are [0.0, -2, 6, 3.5].
  EXPECT_THAT(ComputeScaledConvergenceInformation(
                  params_true, sharded_qp, primal_solution, dual_solution,
                  /*componentwise_primal_residual_offset=*/1.0,
                  /*componentwise_dual_residual_offset=*/1.0,
                  POINT_TYPE_CURRENT_ITERATE),
              Partially(Approximately(EqualsProto(R"pb(
                dual_objective: -3.0
                corrected_dual_objective: -inf
                l_inf_dual_residual: 3.5
                # 5.0497524691810389 = L_2(3.5, -3.0, -0.5, -2.0)
                l2_dual_residual: 5.0497524691810389
              )pb"))));
  EXPECT_THAT(ComputeScaledConvergenceInformation(
                  params_false, sharded_qp, primal_solution, dual_solution,
                  /*componentwise_primal_residual_offset=*/1.0,
                  /*componentwise_dual_residual_offset=*/1.0,
                  POINT_TYPE_CURRENT_ITERATE),
              Partially(Approximately(EqualsProto(R"pb(
                dual_objective: -7.0
                corrected_dual_objective: -inf
                l_inf_dual_residual: 3.5
                # 4.6097722286464436 = L_2(3.5, -3.0, 0.0, 0.0)
                l2_dual_residual: 4.6097722286464436
              )pb"))));
}

TEST(IterationStatsTest, SimpleLpWithGapResidualsAndNonZeroPrimalSolution) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestLp(), num_threads, num_shards);

  const Eigen::VectorXd primal_solution{{0.0, 0.0, 4.0, 3.0}};
  const Eigen::VectorXd dual_solution{{1.0, 0.0, 0.0, -1.0}};

  PrimalDualHybridGradientParams params_true, params_false;
  params_true.set_handle_some_primal_gradients_on_finite_bounds_as_residuals(
      true);
  params_false.set_handle_some_primal_gradients_on_finite_bounds_as_residuals(
      false);

  // c is: [5.5, -2, -1, 1]
  // -A^T y is: [-2, -1, 0.5, -3]
  // c - A^T y is: [3.5, -3.0, -0.5, -2.0].
  // When the primal variable is 0.0 and the bound is not 0.0, the bound
  // corresponding to c - A^T y is treated as infinite when
  // `handle_some_primal_gradients_on_finite_bounds_as_residuals` is true.
  // Thus, for primal_solution [0, 0, 4, 3], whether
  // `handle_some_primal_gradients_on_finite_bounds_as_residuals` is true or
  // not, the residuals are [3.5, -3.0, 0.0, 0.0] and the corresponding bound
  // terms are [0.0, -2, 6, 3.5].
  EXPECT_THAT(ComputeScaledConvergenceInformation(
                  params_true, sharded_qp, primal_solution, dual_solution,
                  /*componentwise_primal_residual_offset=*/1.0,
                  /*componentwise_dual_residual_offset=*/1.0,
                  POINT_TYPE_CURRENT_ITERATE),
              Partially(Approximately(EqualsProto(R"pb(
                dual_objective: -13.0
                corrected_dual_objective: -inf
                l_inf_dual_residual: 3.5
                # 4.6097722286464436 = L_2(3.5, -3.0, 0.0, 0.0)
                l2_dual_residual: 4.6097722286464436
              )pb"))));
  EXPECT_THAT(ComputeScaledConvergenceInformation(
                  params_false, sharded_qp, primal_solution, dual_solution,
                  /*componentwise_primal_residual_offset=*/1.0,
                  /*componentwise_dual_residual_offset=*/1.0,
                  POINT_TYPE_CURRENT_ITERATE),
              Partially(Approximately(EqualsProto(R"pb(
                dual_objective: -7.0
                corrected_dual_objective: -inf
                l_inf_dual_residual: 3.5
                # 4.6097722286464436 = L_2(3.5, -3.0, 0.0, 0.0)
                l2_dual_residual: 4.6097722286464436
              )pb"))));
}

TEST(IterationStatsTest, SimpleQp) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);

  const Eigen::VectorXd primal_solution{{1.0, 2.0}};
  const Eigen::VectorXd dual_solution{{0.0}};
  PrimalDualHybridGradientParams params_true, params_false;
  params_true.set_handle_some_primal_gradients_on_finite_bounds_as_residuals(
      true);
  params_false.set_handle_some_primal_gradients_on_finite_bounds_as_residuals(
      false);
  // Q*x is: [4.0, 2.0]
  // c is: [-1, -1]
  // A^T y is zero.
  // If `handle_some_primal_gradients_on_finite_bounds_as_residuals` is
  // true the second primal gradient term is handled as a residual, not a
  // reduced cost.
  // Other than the reduced cost terms, the dual objective is 5 (objective
  // offset) - 4 (1/2 x^T Q x) = 1
  EXPECT_THAT(ComputeScaledConvergenceInformation(
                  params_true, sharded_qp, primal_solution, dual_solution,
                  /*componentwise_primal_residual_offset=*/1.0,
                  /*componentwise_dual_residual_offset=*/1.0,
                  POINT_TYPE_CURRENT_ITERATE),
              Partially(Approximately(EqualsProto(R"pb(
                dual_objective: 8
                corrected_dual_objective: 2
                l_inf_dual_residual: 1.0
                l2_dual_residual: 1.0
              )pb"))));
  EXPECT_THAT(ComputeScaledConvergenceInformation(
                  params_false, sharded_qp, primal_solution, dual_solution,
                  /*componentwise_primal_residual_offset=*/1.0,
                  /*componentwise_dual_residual_offset=*/1.0,
                  POINT_TYPE_CURRENT_ITERATE),
              Partially(Approximately(EqualsProto(R"pb(
                dual_objective: 2
                corrected_dual_objective: 2
                l_inf_dual_residual: 0.0
                l2_dual_residual: 0.0
              )pb"))));
}

TEST(IterationStatsTest, InfeasibilityInformationWithCertificateLp) {
  const Eigen::VectorXd primal_ray{{0.0, 0.0}};
  const Eigen::VectorXd dual_ray{{-1.0, -1.0}};
  CheckScaledAndUnscaledInfeasibilityStats(
      SmallPrimalInfeasibleLp(), primal_ray, dual_ray, primal_ray,
      ParseTextOrDie<InfeasibilityInformation>(R"pb(
        max_primal_ray_infeasibility: 0
        primal_ray_linear_objective: 0
        primal_ray_quadratic_norm: 0
        max_dual_ray_infeasibility: 0
        dual_ray_objective: 1
      )pb"));
}

TEST(IterationStatsTest, InfeasibilityInformationWithoutCertificateLp) {
  const Eigen::VectorXd primal_ray{{2.0, 1.0}};
  const Eigen::VectorXd dual_ray{{-1.0, -3.0}};
  CheckScaledAndUnscaledInfeasibilityStats(
      SmallPrimalInfeasibleLp(), primal_ray, dual_ray, primal_ray,
      ParseTextOrDie<InfeasibilityInformation>(R"pb(
        max_primal_ray_infeasibility: 0.5
        primal_ray_linear_objective: 1.5
        primal_ray_quadratic_norm: 0
        max_dual_ray_infeasibility: 0.66666666666666663
        dual_ray_objective: 1.6666666666666667
      )pb"));
}

TEST(IterationStatsTest, DetectsDualRayHasInfeasibleComponent) {
  const Eigen::VectorXd primal_ray{{0.0, 0.0}};
  const Eigen::VectorXd dual_ray{{1.0, 1.0}};
  // Components with the wrong sign cause the dual ray objective to be -inf.
  CheckScaledAndUnscaledInfeasibilityStats(
      SmallPrimalInfeasibleLp(), primal_ray, dual_ray, primal_ray,
      ParseTextOrDie<InfeasibilityInformation>(R"pb(
        max_dual_ray_infeasibility: 0.0
        dual_ray_objective: -inf
      )pb"));
}

// Regression test for failures of math_opt's
// SimpleLpTest.OptimalAfterInfeasible test.
TEST(IterationStatsTest, HandlesReducedCostsOnDualRayCorrectly) {
  // A trivial LP mimicking the one used in math_opt's test:
  //     min x
  //     Constraint: 2 <= x
  //     Variable: 0 <= x <= 1
  QuadraticProgram lp(1, 1);
  lp.objective_vector = Eigen::VectorXd{{1}};
  lp.constraint_lower_bounds = Eigen::VectorXd{{2}};
  lp.constraint_upper_bounds =
      Eigen::VectorXd{{std::numeric_limits<double>::infinity()}};
  lp.variable_lower_bounds = Eigen::VectorXd{{0}};
  lp.variable_upper_bounds = Eigen::VectorXd{{1}};
  lp.constraint_matrix.coeffRef(0, 0) = 1.0;
  lp.constraint_matrix.makeCompressed();
  const Eigen::VectorXd primal_solution{{1.0}};
  const Eigen::VectorXd primal_ray{{0.0}};
  const Eigen::VectorXd dual_ray{{1.0}};
  // `dual_ray_objective` = 2 (objective term) - 1 (reduced cost on x) = 1.
  CheckScaledAndUnscaledInfeasibilityStats(
      lp, primal_ray, dual_ray, primal_solution,
      ParseTextOrDie<InfeasibilityInformation>(R"pb(
        max_dual_ray_infeasibility: 0.0
        dual_ray_objective: 1.0
      )pb"));
}

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
