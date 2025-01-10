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

#include "ortools/pdlp/sharded_optimization_utils.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/sharder.h"
#include "ortools/pdlp/solve_log.pb.h"
#include "ortools/pdlp/test_util.h"

namespace operations_research::pdlp {
namespace {

using ::Eigen::VectorXd;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsNan;

TEST(ShardedWeightedAverageTest, SimpleAverage) {
  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2,
                  /*thread_pool=*/nullptr);
  Eigen::VectorXd vec1{{4, 1}};
  Eigen::VectorXd vec2{{1, 7}};

  ShardedWeightedAverage average(&sharder);
  average.Add(vec1, 1.0);
  average.Add(vec2, 2.0);

  ASSERT_TRUE(average.HasNonzeroWeight());
  EXPECT_EQ(average.NumTerms(), 2);

  EXPECT_THAT(average.ComputeAverage(), ElementsAre(2.0, 5.0));

  average.Clear();
  EXPECT_FALSE(average.HasNonzeroWeight());
  EXPECT_EQ(average.NumTerms(), 0);
}

TEST(ShardedWeightedAverageTest, MoveConstruction) {
  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2,
                  /*thread_pool=*/nullptr);
  const Eigen::VectorXd vec{{4, 1}};

  ShardedWeightedAverage average(&sharder);
  average.Add(vec, 2.0);

  ShardedWeightedAverage average2(std::move(average));
  EXPECT_THAT(average2.ComputeAverage(), ElementsAre(4.0, 1.0));
}

TEST(ShardedWeightedAverageTest, MoveAssignment) {
  Sharder sharder1(/*num_elements=*/2, /*num_shards=*/2,
                   /*thread_pool=*/nullptr);
  Sharder sharder2(/*num_elements=*/3, /*num_shards=*/2,
                   /*thread_pool=*/nullptr);
  Eigen::VectorXd vec1{{4, 1}};
  Eigen::VectorXd vec2{{0, 3}};

  ShardedWeightedAverage average1(&sharder1);
  average1.Add(vec1, 2.0);

  ShardedWeightedAverage average2(&sharder2);

  average2 = std::move(average1);
  average2.Add(vec2, 2.0);
  EXPECT_THAT(average2.ComputeAverage(), ElementsAre(2.0, 2.0));
}

TEST(ShardedWeightedAverageTest, ZeroAverage) {
  Sharder sharder(/*num_elements=*/1, /*num_shards=*/1,
                  /*thread_pool=*/nullptr);

  ShardedWeightedAverage average(&sharder);
  ASSERT_FALSE(average.HasNonzeroWeight());

  EXPECT_THAT(average.ComputeAverage(), ElementsAre(0.0));
}

// This test verifies that if we average an identical vector repeatedly the
// average is exactly that vector, with no roundoff.
TEST(ShardedWeightedAverageTest, AveragesEqualWithoutRoundoff) {
  Sharder sharder(/*num_elements=*/4, /*num_shards=*/1,
                  /*thread_pool=*/nullptr);
  ShardedWeightedAverage average(&sharder);
  EXPECT_THAT(average.ComputeAverage(), ElementsAre(0, 0, 0, 0));
  Eigen::VectorXd data{{1.0, 1.0 / 3, 3.0 / 7, 3.14159}};
  average.Add(data, 341.45);
  EXPECT_THAT(average.ComputeAverage(), ElementsAreArray(data));
  average.Add(data, 1.4134);
  EXPECT_THAT(average.ComputeAverage(), ElementsAreArray(data));
  average.Add(data, 7.23);
  EXPECT_THAT(average.ComputeAverage(), ElementsAreArray(data));
}

TEST(ShardedWeightedAverageTest, AddsZeroWeight) {
  Sharder sharder(/*num_elements=*/1, /*num_shards=*/1,
                  /*thread_pool=*/nullptr);

  ShardedWeightedAverage average(&sharder);
  ASSERT_FALSE(average.HasNonzeroWeight());
  Eigen::VectorXd data{{1.0}};
  average.Add(data, 0.0);
  EXPECT_FALSE(average.HasNonzeroWeight());
  EXPECT_THAT(average.ComputeAverage(), ElementsAre(0.0));
}

// The combined bounds vector for `TestLp()` is [12, 7, 4, 1].
// L_inf norm: 12.0
// L_2 norm: sqrt(210.0) ≈ 14.49

TEST(ProblemStatsTest, TestLp) {
  ShardedQuadraticProgram lp(TestLp(), 2, 2);
  const QuadraticProgramStats stats = ComputeStats(lp);

  EXPECT_EQ(stats.num_variables(), 4);
  EXPECT_EQ(stats.num_constraints(), 4);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_col_min_l_inf_norm(), 1.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_row_min_l_inf_norm(), 1.0);
  EXPECT_EQ(stats.constraint_matrix_num_nonzeros(), 9);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_max(), 4.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_avg(), 14.5 / 9.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_l2_norm(), std::sqrt(31.25));
  EXPECT_DOUBLE_EQ(stats.objective_vector_abs_max(), 5.5);
  EXPECT_DOUBLE_EQ(stats.objective_vector_abs_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.objective_vector_abs_avg(), 2.375);
  EXPECT_DOUBLE_EQ(stats.objective_vector_l2_norm(), std::sqrt(36.25));
  EXPECT_EQ(stats.objective_matrix_num_nonzeros(), 0);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_max(), 0.0);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_min(), 0.0);
  EXPECT_THAT(stats.objective_matrix_abs_avg(), IsNan());
  EXPECT_DOUBLE_EQ(stats.objective_matrix_l2_norm(), 0.0);
  EXPECT_EQ(stats.variable_bound_gaps_num_finite(), 1);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_max(), 1.0);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_avg(), 1.0);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_l2_norm(), 1.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_max(), 12.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_avg(), 6.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_l2_norm(), std::sqrt(210.0));
}

TEST(ProblemStatsTest, TinyLp) {
  ShardedQuadraticProgram lp(TinyLp(), 2, 2);
  const QuadraticProgramStats stats = ComputeStats(lp);

  EXPECT_EQ(stats.num_variables(), 4);
  EXPECT_EQ(stats.num_constraints(), 3);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_col_min_l_inf_norm(), 1.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_row_min_l_inf_norm(), 1.0);
  EXPECT_EQ(stats.constraint_matrix_num_nonzeros(), 8);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_max(), 2.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_avg(), 1.25);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_l2_norm(), std::sqrt(14.0));
  EXPECT_DOUBLE_EQ(stats.objective_vector_abs_max(), 5.0);
  EXPECT_DOUBLE_EQ(stats.objective_vector_abs_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.objective_vector_abs_avg(), 2.25);
  EXPECT_DOUBLE_EQ(stats.objective_vector_l2_norm(), std::sqrt(31.0));
  EXPECT_EQ(stats.objective_matrix_num_nonzeros(), 0);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_max(), 0.0);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_min(), 0.0);
  EXPECT_THAT(stats.objective_matrix_abs_avg(), IsNan());
  EXPECT_DOUBLE_EQ(stats.objective_matrix_l2_norm(), 0.0);
  EXPECT_EQ(stats.variable_bound_gaps_num_finite(), 4);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_max(), 6.0);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_min(), 2.0);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_avg(), 3.75);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_l2_norm(), std::sqrt(65.0));
  EXPECT_DOUBLE_EQ(stats.combined_bounds_max(), 12.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_avg(), 20.0 / 3.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_l2_norm(), std::sqrt(194.0));
}

TEST(ProblemStatsTest, TestDiagonalQp1) {
  ShardedQuadraticProgram qp(TestDiagonalQp1(), 2, 2);
  const QuadraticProgramStats stats = ComputeStats(qp);

  EXPECT_EQ(stats.num_variables(), 2);
  EXPECT_EQ(stats.num_constraints(), 1);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_col_min_l_inf_norm(), 1.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_row_min_l_inf_norm(), 1.0);
  EXPECT_EQ(stats.constraint_matrix_num_nonzeros(), 2);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_max(), 1.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_avg(), 1.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_l2_norm(), std::sqrt(2.0));
  EXPECT_DOUBLE_EQ(stats.objective_vector_abs_max(), 1.0);
  EXPECT_DOUBLE_EQ(stats.objective_vector_abs_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.objective_vector_abs_avg(), 1.0);
  EXPECT_DOUBLE_EQ(stats.objective_vector_l2_norm(), std::sqrt(2.0));
  EXPECT_EQ(stats.objective_matrix_num_nonzeros(), 2);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_max(), 4.0);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_avg(), 2.5);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_l2_norm(), std::sqrt(17.0));
  EXPECT_EQ(stats.variable_bound_gaps_num_finite(), 2);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_max(), 6.0);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_avg(), 3.5);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_l2_norm(), std::sqrt(37.0));
  EXPECT_DOUBLE_EQ(stats.combined_bounds_max(), 1.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_min(), 1.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_avg(), 1.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_l2_norm(), 1.0);
}

TEST(ProblemStatsTest, ModifiedTestDiagonalQp1) {
  QuadraticProgram orig_qp = TestDiagonalQp1();
  // A case where `objective_matrix_num_nonzeros` doesn't match the dimension.
  orig_qp.objective_matrix->diagonal() = Eigen::VectorXd{{2.0, 0.0}};
  ShardedQuadraticProgram qp(orig_qp, 2, 2);
  const QuadraticProgramStats stats = ComputeStats(qp);

  EXPECT_EQ(stats.objective_matrix_num_nonzeros(), 1);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_max(), 2.0);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_min(), 2.0);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_avg(), 1.0);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_l2_norm(), 2.0);
}

TEST(ProblemStatsTest, NoFiniteGaps) {
  ShardedQuadraticProgram lp(SmallInvalidProblemLp(), 2, 2);
  const QuadraticProgramStats stats = ComputeStats(lp);
  // Ensure max/min/avg take their default values when no finite gaps exist.
  EXPECT_EQ(stats.variable_bound_gaps_num_finite(), 0);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_max(), 0.0);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_min(), 0.0);
  EXPECT_THAT(stats.variable_bound_gaps_avg(), IsNan());
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_l2_norm(), 0.0);
}

TEST(ProblemStatsTest, LpWithoutConstraints) {
  ShardedQuadraticProgram lp(LpWithoutConstraints(), 2, 2);
  const QuadraticProgramStats stats = ComputeStats(lp);
  // When there are no constraints, max/min absolute values and infinity norms
  // are assigned 0 by convention. The same is true for the combined bounds.
  EXPECT_EQ(stats.constraint_matrix_num_nonzeros(), 0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_max(), 0.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_min(), 0.0);
  EXPECT_THAT(stats.constraint_matrix_abs_avg(), IsNan());
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_l2_norm(), 0.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_col_min_l_inf_norm(), 0.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_row_min_l_inf_norm(), 0.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_max(), 0.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_min(), 0.0);
  EXPECT_THAT(stats.combined_bounds_avg(), IsNan());
  EXPECT_DOUBLE_EQ(stats.combined_bounds_l2_norm(), 0.0);
}

TEST(ProblemStatsTest, EmptyLp) {
  ShardedQuadraticProgram lp(QuadraticProgram(0, 0), 2, 2);
  const QuadraticProgramStats stats = ComputeStats(lp);
  // When `lp` is empty, everything except averages should be 0 and averages
  // should be NaN.
  EXPECT_EQ(stats.num_variables(), 0);
  EXPECT_EQ(stats.num_constraints(), 0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_col_min_l_inf_norm(), 0.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_row_min_l_inf_norm(), 0.0);
  EXPECT_EQ(stats.constraint_matrix_num_nonzeros(), 0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_max(), 0.0);
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_abs_min(), 0.0);
  EXPECT_THAT(stats.constraint_matrix_abs_avg(), IsNan());
  EXPECT_DOUBLE_EQ(stats.constraint_matrix_l2_norm(), 0.0);
  EXPECT_DOUBLE_EQ(stats.objective_vector_abs_max(), 0.0);
  EXPECT_DOUBLE_EQ(stats.objective_vector_abs_min(), 0.0);
  EXPECT_THAT(stats.objective_vector_abs_avg(), IsNan());
  EXPECT_DOUBLE_EQ(stats.objective_vector_l2_norm(), 0.0);
  EXPECT_EQ(stats.objective_matrix_num_nonzeros(), 0);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_max(), 0.0);
  EXPECT_DOUBLE_EQ(stats.objective_matrix_abs_min(), 0.0);
  EXPECT_THAT(stats.objective_matrix_abs_avg(), IsNan());
  EXPECT_DOUBLE_EQ(stats.objective_matrix_l2_norm(), 0.0);
  EXPECT_EQ(stats.variable_bound_gaps_num_finite(), 0);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_max(), 0.0);
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_min(), 0.0);
  EXPECT_THAT(stats.variable_bound_gaps_avg(), IsNan());
  EXPECT_DOUBLE_EQ(stats.variable_bound_gaps_l2_norm(), 0.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_max(), 0.0);
  EXPECT_DOUBLE_EQ(stats.combined_bounds_min(), 0.0);
  EXPECT_THAT(stats.combined_bounds_avg(), IsNan());
  EXPECT_DOUBLE_EQ(stats.combined_bounds_l2_norm(), 0.0);
}

// The `TestLp()` matrix is [ 2 1 1 2; 1 0 1 0; 4 0 0 0; 0 0 1.5 -1],
// the scaled matrix is [ 0 1 2 -2; 0 0 4 0; 0 0 0 0; 0 0 9 3],
// so the row LInf norms are [2 4 0 9] and the column LInf norms are [0 1 9 3].
// Rescaling divides the scaling vectors by sqrt(norms).
TEST(LInfRuizRescaling, OneIteration) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);
  Eigen::VectorXd row_scaling_vec{{1, 2, 1, 3}};
  Eigen::VectorXd col_scaling_vec{{0, 1, 2, -1}};
  LInfRuizRescaling(lp, /*num_iterations=*/1, row_scaling_vec, col_scaling_vec);
  EXPECT_THAT(row_scaling_vec, ElementsAre(1 / std::sqrt(2), 1.0, 1.0, 1.0));
  EXPECT_THAT(col_scaling_vec,
              ElementsAre(0.0, 1.0, 2.0 / 3.0, -1.0 / std::sqrt(3.0)));
}

// The `TestLp()` matrix is [ 2 1 1 2; 1 0 1 0; 4 0 0 0; 0 0 1.5 -1],
// the scaled matrix is [ 0 1 2 -2; 0 0 4 0; 0 0 0 0; 0 0 9 3],
// so the row L2 norms are [3 4 0 sqrt(90)] and the column L2 norms are [0 1
// sqrt(101) sqrt(13)]. Rescaling divides the scaling vectors by sqrt(norms).
TEST(L2RuizRescaling, OneIteration) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);
  Eigen::VectorXd row_scaling_vec{{1, 2, 1, 3}};
  Eigen::VectorXd col_scaling_vec{{0, 1, 2, -1}};
  L2NormRescaling(lp, row_scaling_vec, col_scaling_vec);
  EXPECT_THAT(row_scaling_vec, ElementsAre(1.0 / std::pow(3.0, 0.5), 1.0, 1.0,
                                           3.0 / std::pow(90.0, 0.25)));
  EXPECT_THAT(col_scaling_vec, ElementsAre(0.0, 1.0, 2.0 / std::pow(101, 0.25),
                                           -1.0 / std::pow(13.0, 0.25)));
}

// The `test_lp` matrix is [2 3], so the row L2 norms are [sqrt(13)] and the
// column L2 norms are [2 3]. Rescaling divides the scaling vectors by
// sqrt(norms).
TEST(L2RuizRescaling, OneIterationNonSquare) {
  QuadraticProgram test_lp(/*num_variables=*/2, /*num_constraints=*/1);
  std::vector<Eigen::Triplet<double, int64_t>> triplets = {{0, 0, 2.0},
                                                           {0, 1, 3.0}};
  test_lp.constraint_matrix.setFromTriplets(triplets.begin(), triplets.end());
  ShardedQuadraticProgram lp(std::move(test_lp), /*num_threads=*/2,
                             /*num_shards=*/2);
  VectorXd row_scaling_vec = VectorXd::Ones(1);
  VectorXd col_scaling_vec = VectorXd::Ones(2);
  L2NormRescaling(lp, row_scaling_vec, col_scaling_vec);
  EXPECT_THAT(row_scaling_vec, ElementsAre(1.0 / std::pow(13.0, 0.25)));
  EXPECT_THAT(col_scaling_vec,
              ElementsAre(1.0 / std::sqrt(2.0), 1.0 / std::sqrt(3.0)));
}

// With many iterations of `LInfRuizRescaling`, the scaled matrix should
// converge to have col LInf norm 1 and row LInf norm 1.
TEST(LInfRuizRescaling, Convergence) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);
  VectorXd col_norm(4), row_norm(4);
  Eigen::VectorXd row_scaling_vec{{1, 1, 1, 1}};
  Eigen::VectorXd col_scaling_vec{{1, 1, 1, 1}};
  LInfRuizRescaling(lp, /*num_iterations=*/20, row_scaling_vec,
                    col_scaling_vec);
  col_norm = ScaledColLInfNorm(lp.Qp().constraint_matrix, row_scaling_vec,
                               col_scaling_vec, lp.ConstraintMatrixSharder());
  row_norm = ScaledColLInfNorm(lp.TransposedConstraintMatrix(), col_scaling_vec,
                               row_scaling_vec,
                               lp.TransposedConstraintMatrixSharder());
  EXPECT_THAT(row_norm, EigenArrayNear<double>({1.0, 1.0, 1.0, 1.0}, 1.0e-4));
  EXPECT_THAT(col_norm, EigenArrayNear<double>({1.0, 1.0, 1.0, 1.0}, 1.0e-4));
}

// This applies one round of l_inf and one round of L2 rescaling.
// The `TestLp()` matrix is [ 2 1 1 2; 1 0 1 0; 4 0 0 0; 0 0 1.5 -1],
// so the row LInf norms are [2 1 4 1.5] and column LInf norms are [4 1 1.5 2].
// l_inf divides by sqrt(norms), giving
// [0.7071 0.7071 0.5773 1; 0.5 0 0.8165 0; 1 0 0 0; 0 0 1 -0.5773]
// which has row L2 norms [1.5275 0.957429 1 1.1547] and col L2 norms
// [1.3229 0.7071 1.4142 1.1547]. The resulting scaling vectors are
// 1/sqrt((l_inf norms).*(l2 norms)).
TEST(ApplyRescaling, ApplyRescalingWorksForTestLp) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);
  ScalingVectors scaling = ApplyRescaling(
      RescalingOptions{.l_inf_ruiz_iterations = 1, .l2_norm_rescaling = true},
      lp);
  EXPECT_THAT(scaling.row_scaling_vec,
              EigenArrayNear<double>(
                  {1.0 / sqrt(2.0 * 1.5275), 1.0 / sqrt(1.0 * 0.9574),
                   1.0 / sqrt(4.0 * 1.0), 1.0 / sqrt(1.5 * 1.1547)},
                  1.0e-4));
  EXPECT_THAT(scaling.col_scaling_vec,
              EigenArrayNear<double>(
                  {1.0 / sqrt(4.0 * 1.3229), 1.0 / sqrt(1.0 * 0.7071),
                   1.0 / sqrt(1.5 * 1.4142), 1.0 / sqrt(2.0 * 1.1547)},
                  1.0e-4));
}

TEST(ComputePrimalGradientTest, CorrectForLp) {
  // The choice of two shards is intentional, to help catch bugs in the sharded
  // computations.
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);

  const Eigen::VectorXd primal_solution{{0.0, 0.0, 0.0, 3.0}};
  const Eigen::VectorXd dual_solution{{-1.0, 0.0, 1.0, 1.0}};

  const LagrangianPart primal_part = ComputePrimalGradient(
      lp, primal_solution, lp.TransposedConstraintMatrix() * dual_solution);
  // Using notation consistent with
  // https://developers.google.com/optimization/lp/pdlp_math.
  // c - A^T y
  EXPECT_THAT(primal_part.gradient,
              ElementsAre(5.5 - 2.0, -2.0 + 1.0, -1.0 - 0.5, 1.0 + 3.0));
  // c^T x - y^T Ax.
  EXPECT_DOUBLE_EQ(primal_part.value, 3.0 + 9.0);
}

TEST(ComputeDualGradientTest, CorrectForLp) {
  // The choice of two shards is intentional, to help catch bugs in the sharded
  // computations.
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);

  const Eigen::VectorXd primal_solution{{0.0, 0.0, 0.0, 3.0}};
  const Eigen::VectorXd dual_solution{{-1.0, 0.0, 1.0, 1.0}};

  const LagrangianPart dual_part = ComputeDualGradient(
      lp, dual_solution, lp.Qp().constraint_matrix * primal_solution);
  // Using notation consistent with
  // https://developers.google.com/optimization/lp/pdlp_math.
  // active_constraint_right_hand_side - Ax
  EXPECT_THAT(dual_part.gradient,
              ElementsAre(12.0 - 6.0, 7.0, -4.0, -1.0 + 3.0));
  // y^T active_constraint_right_hand_side
  EXPECT_DOUBLE_EQ(dual_part.value, 12.0 * -1.0 + -4.0 * 1.0 + -1.0 * 1.0);
}

TEST(ComputeDualGradientTest, CorrectOnTwoSidedConstraints) {
  QuadraticProgram qp = TestLp();
  // Makes the constraints all two-sided. The primal solution is feasible in
  // the first constraint, below the lower bound of the second constraint, and
  // above the upper bound of the third constraint.
  qp.constraint_lower_bounds[0] = 4;
  qp.constraint_lower_bounds[1] = 5;
  qp.constraint_upper_bounds[2] = -1;
  ShardedQuadraticProgram sharded_qp(std::move(qp), /*num_threads=*/2,
                                     /*num_shards=*/2);

  const Eigen::VectorXd primal_solution{{0.0, 0.0, 0.0, 3.0}};
  const Eigen::VectorXd dual_solution{{0.0, 0.0, 0.0, -1.0}};

  const LagrangianPart dual_part =
      ComputeDualGradient(sharded_qp, dual_solution,
                          sharded_qp.Qp().constraint_matrix * primal_solution);
  // Using notation consistent with
  // https://developers.google.com/optimization/lp/pdlp_math.
  // active_constraint_right_hand_side - Ax
  EXPECT_THAT(dual_part.gradient,
              ElementsAre(0.0, 5.0 - 0.0, -1.0 - 0.0, 1.0 + 3.0));
  // y^T active_constraint_right_hand_side
  EXPECT_DOUBLE_EQ(dual_part.value, 1.0 * -1.0);
}

TEST(HasValidBoundsTest, InconsistentConstraintBounds) {
  ShardedQuadraticProgram lp(SmallInvalidProblemLp(), /*num_threads=*/2,
                             /*num_shards=*/2);
  EXPECT_FALSE(HasValidBounds(lp));
}

TEST(HasValidBoundsTest, InconsistentVariableBounds) {
  ShardedQuadraticProgram lp(SmallInconsistentVariableBoundsLp(),
                             /*num_threads=*/2,
                             /*num_shards=*/2);
  EXPECT_FALSE(HasValidBounds(lp));
}

TEST(HasValidBoundsTest, SmallValidLp) {
  ShardedQuadraticProgram lp(SmallPrimalInfeasibleLp(), /*num_threads=*/2,
                             /*num_shards=*/2);
  EXPECT_TRUE(HasValidBounds(lp));
}

TEST(HasValidBoundsTest, EqualInfiniteConstraintBounds) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  QuadraticProgram lp = SmallPrimalInfeasibleLp();
  lp.constraint_lower_bounds[1] = kInfinity;
  lp.constraint_upper_bounds[1] = kInfinity;
  EXPECT_FALSE(HasValidBounds(ShardedQuadraticProgram(lp, /*num_threads=*/2,
                                                      /*num_shards=*/2)));
  lp.constraint_lower_bounds[1] = -kInfinity;
  lp.constraint_upper_bounds[1] = -kInfinity;
  EXPECT_FALSE(HasValidBounds(ShardedQuadraticProgram(lp, /*num_threads=*/2,
                                                      /*num_shards=*/2)));
}

TEST(HasValidBoundsTest, EqualInfiniteVariableBounds) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  QuadraticProgram lp = SmallPrimalInfeasibleLp();
  lp.variable_lower_bounds[1] = kInfinity;
  lp.variable_upper_bounds[1] = kInfinity;
  EXPECT_FALSE(HasValidBounds(ShardedQuadraticProgram(lp, /*num_threads=*/2,
                                                      /*num_shards=*/2)));
  lp.variable_lower_bounds[1] = -kInfinity;
  lp.variable_upper_bounds[1] = -kInfinity;
  EXPECT_FALSE(HasValidBounds(ShardedQuadraticProgram(lp, /*num_threads=*/2,
                                                      /*num_shards=*/2)));
}

TEST(ComputePrimalGradientTest, CorrectForQp) {
  ShardedQuadraticProgram qp(TestDiagonalQp1(), /*num_threads=*/2,
                             /*num_shards=*/2);

  const Eigen::VectorXd primal_solution{{1.0, 2.0}};
  const Eigen::VectorXd dual_solution{{-2.0}};

  const LagrangianPart primal_part = ComputePrimalGradient(
      qp, primal_solution, qp.TransposedConstraintMatrix() * dual_solution);

  // Using notation consistent with
  // https://developers.google.com/optimization/lp/pdlp_math.
  // c - A^T y + Qx
  EXPECT_THAT(primal_part.gradient,
              ElementsAre(-1.0 + 2.0 + 4.0, -1.0 + 2.0 + 2.0));
  // (1/2) x^T Qx + c^T x - y^T Ax.
  EXPECT_DOUBLE_EQ(primal_part.value, 4.0 - 3.0 + 2.0 * 3.0);
}

TEST(ComputeDualGradientTest, CorrectForQp) {
  ShardedQuadraticProgram qp(TestDiagonalQp1(), /*num_threads=*/2,
                             /*num_shards=*/2);

  const Eigen::VectorXd primal_solution{{1.0, 2.0}};
  const Eigen::VectorXd dual_solution{{-2.0}};

  const LagrangianPart dual_part = ComputeDualGradient(
      qp, dual_solution, qp.Qp().constraint_matrix * primal_solution);

  // Using notation consistent with
  // https://developers.google.com/optimization/lp/pdlp_math.
  // active_constraint_right_hand_side - Ax
  EXPECT_THAT(dual_part.gradient, ElementsAre(1.0 - (1.0 + 2.0)));
  // y^T active_constraint_right_hand_side
  EXPECT_DOUBLE_EQ(dual_part.value, -2.0);
}

TEST(EstimateSingularValuesTest, CorrectForTestLp) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);

  // The `TestLp()` matrix is [ 2 1 1 2; 1 0 1 0; 4 0 0 0; 0 0 1.5 -1].
  std::mt19937 random(1);
  auto result = EstimateMaximumSingularValueOfConstraintMatrix(
      lp, std::nullopt, std::nullopt,
      /*desired_relative_error=*/0.01,
      /*failure_probability=*/0.001, random);
  EXPECT_NEAR(result.singular_value, 4.76945, 0.01);
  EXPECT_LT(result.num_iterations, 300);
}

TEST(EstimateSingularValuesTest, CorrectForTestLpWithActivePrimalSubspace) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);

  // Chosen so x_1 is at its bound, and all other variables are not at bounds.
  const Eigen::VectorXd primal_solution{{0.0, -2.0, 0.0, 3.0}};
  // The `TestLp()` matrix is [ 2 1 1 2; 1 0 1 0; 4 0 0 0; 0 0 1.5 -1],
  // so the projected matrix is [ 2 1 2; 1 1 0; 4 0 0; 0 1.5 -1].
  std::mt19937 random(1);
  auto result = EstimateMaximumSingularValueOfConstraintMatrix(
      lp, primal_solution, std::nullopt, /*desired_relative_error=*/0.01,
      /*failure_probability=*/0.001, random);
  EXPECT_NEAR(result.singular_value, 4.73818, 0.01);
  EXPECT_LT(result.num_iterations, 300);
}

TEST(EstimateSingularValuesTest, CorrectForTestLpWithActiveDualSubspace) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);

  // Chosen so the second dual is at its bound, and all other duals are not at
  // bounds.
  const Eigen::VectorXd dual_solution{{1.0, 0.0, 1.0, 3.0}};
  // The `TestLp()` matrix is [ 2 1 1 2; 1 0 1 0; 4 0 0 0; 0 0 1.5 -1],
  // so the projected matrix is [ 2 1 1 2; 4 0 0 0; 0 0 1.5 -1].
  std::mt19937 random(1);
  auto result = EstimateMaximumSingularValueOfConstraintMatrix(
      lp, std::nullopt, dual_solution, /*desired_relative_error=*/0.01,
      /*failure_probability=*/0.001, random);
  EXPECT_NEAR(result.singular_value, 4.64203, 0.01);
  EXPECT_LT(result.num_iterations, 300);
}

TEST(EstimateSingularValuesTest, CorrectForTestLpWithBothActiveSubspaces) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);

  // Chosen so x_1 is at its bound, and all other variables are not at bounds.
  const Eigen::VectorXd primal_solution{{0.0, -2.0, 0.0, 3.0}};
  // Chosen so the second dual is at its bound, and all other duals are not at
  // bounds.
  const Eigen::VectorXd dual_solution{{1.0, 0.0, 1.0, 3.0}};
  // The `TestLp()` matrix is [ 2 1 1 2; 1 0 1 0; 4 0 0 0; 0 0 1.5 -1],
  // so the projected matrix is [ 2 1 2; 4 0 0; 0 1.5 -1].
  std::mt19937 random(1);
  auto result = EstimateMaximumSingularValueOfConstraintMatrix(
      lp, primal_solution, dual_solution, /*desired_relative_error=*/0.01,
      /*failure_probability=*/0.001, random);
  EXPECT_NEAR(result.singular_value, 4.60829, 0.01);
  EXPECT_LT(result.num_iterations, 300);
}

TEST(EstimateSingularValuesTest, CorrectForDiagonalLp) {
  QuadraticProgram diagonal_lp = TestLp();
  std::vector<Eigen::Triplet<double, int64_t>> triplets = {
      {0, 0, 2}, {1, 1, 1}, {2, 2, -3}, {3, 3, -1}};
  diagonal_lp.constraint_matrix.setFromTriplets(triplets.begin(),
                                                triplets.end());
  ShardedQuadraticProgram lp(diagonal_lp, /*num_threads=*/2, /*num_shards=*/2);

  // The `diagonal_lp` matrix is [ 2 0 0 0; 0 1 0 0; 0 0 -3 0; 0 0 0 -1].
  std::mt19937 random(1);
  auto result = EstimateMaximumSingularValueOfConstraintMatrix(
      lp, std::nullopt, std::nullopt,
      /*desired_relative_error=*/0.01,
      /*failure_probability=*/0.001, random);
  EXPECT_NEAR(result.singular_value, 3, 0.0001);
  EXPECT_LT(result.num_iterations, 300);
}

TEST(ProjectToPrimalVariableBoundsTest, TestLp) {
  ShardedQuadraticProgram qp(TestLp(), /*num_threads=*/2,
                             /*num_shards=*/2);
  Eigen::VectorXd primal{{-3, -3, 5, 5}};
  ProjectToPrimalVariableBounds(qp, primal);
  EXPECT_THAT(primal, ElementsAre(-3, -2, 5, 3.5));
}

TEST(ProjectToPrimalVariableBoundsTest, TestLpWithFeasibility) {
  ShardedQuadraticProgram qp(TestLp(), /*num_threads=*/2,
                             /*num_shards=*/2);
  Eigen::VectorXd primal{{-3, -3, 5, 5}};
  ProjectToPrimalVariableBounds(qp, primal, /*use_feasibility_bounds=*/true);
  EXPECT_THAT(primal, ElementsAre(-3, 0, 0, 0));
}

TEST(ProjectToDualVariableBoundsTest, TestLp) {
  ShardedQuadraticProgram qp(TestLp(), /*num_threads=*/2,
                             /*num_shards=*/2);
  Eigen::VectorXd dual{{1, 1, -1, -1}};
  ProjectToDualVariableBounds(qp, dual);
  EXPECT_THAT(dual, ElementsAre(1, 0, 0, -1));
}

}  // namespace
}  // namespace operations_research::pdlp
