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

#include "ortools/pdlp/sharded_quadratic_program.h"

#include <limits>
#include <optional>

#include "Eigen/Core"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/sharder.h"
#include "ortools/pdlp/test_util.h"

namespace operations_research::pdlp {
namespace {

using ::testing::ElementsAre;

const double kInfinity = std::numeric_limits<double>::infinity();

TEST(ShardedQuadraticProgramTest, BasicTest) {
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);
  const int primal_size = 2;
  const int dual_size = 1;
  EXPECT_EQ(sharded_qp.DualSize(), dual_size);
  EXPECT_EQ(sharded_qp.PrimalSize(), primal_size);
  EXPECT_THAT(ToDense(sharded_qp.TransposedConstraintMatrix()),
              EigenArrayEq<double>({{1}, {1}}));
  EXPECT_EQ(sharded_qp.ConstraintMatrixSharder().NumElements(), primal_size);
  EXPECT_EQ(sharded_qp.DualSharder().NumElements(), dual_size);
  EXPECT_EQ(sharded_qp.PrimalSharder().NumElements(), primal_size);
  EXPECT_EQ(sharded_qp.TransposedConstraintMatrixSharder().NumElements(),
            dual_size);
}

TEST(ShardedQuadraticProgramTest, SwapVariableBounds) {
  const int num_threads = 2;
  const int num_shards = 2;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);
  Eigen::VectorXd new_lb{{-kInfinity, 0.0}};
  Eigen::VectorXd new_ub{{0.0, 1.0}};
  sharded_qp.SwapVariableBounds(new_lb, new_ub);
  EXPECT_THAT(new_lb, ElementsAre(1.0, -2.0));
  EXPECT_THAT(new_ub, ElementsAre(2.0, 4.0));
  EXPECT_THAT(sharded_qp.Qp().variable_lower_bounds,
              ElementsAre(-kInfinity, 0.0));
  EXPECT_THAT(sharded_qp.Qp().variable_upper_bounds, ElementsAre(0.0, 1.0));
}

TEST(ShardedQuadraticProgramTest, SwapConstraintBounds) {
  const int num_threads = 2;
  const int num_shards = 2;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);
  Eigen::VectorXd new_lb{{1.0}};
  Eigen::VectorXd new_ub{{5.0}};
  sharded_qp.SwapConstraintBounds(new_lb, new_ub);
  EXPECT_THAT(new_lb, ElementsAre(-kInfinity));
  EXPECT_THAT(new_ub, ElementsAre(1.0));
  EXPECT_THAT(sharded_qp.Qp().constraint_lower_bounds, ElementsAre(1.0));
  EXPECT_THAT(sharded_qp.Qp().constraint_upper_bounds, ElementsAre(5.0));
}

TEST(ShardedQuadraticProgramTest, SetConstraintBounds) {
  const int num_threads = 2;
  const int num_shards = 2;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);
  sharded_qp.SetConstraintBounds(/*constraint_index=*/0, /*lower_bound=*/-1.0,
                                 /*upper_bound=*/10.0);
  EXPECT_THAT(sharded_qp.Qp().constraint_lower_bounds, ElementsAre(-1.0));
  EXPECT_THAT(sharded_qp.Qp().constraint_upper_bounds, ElementsAre(10.0));
}

TEST(ShardedQuadraticProgramTest, SetConstraintLowerBound) {
  const int num_threads = 2;
  const int num_shards = 2;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);
  sharded_qp.SetConstraintBounds(/*constraint_index=*/0, /*lower_bound=*/-1.0,
                                 /*upper_bound=*/std::nullopt);
  EXPECT_THAT(sharded_qp.Qp().constraint_lower_bounds, ElementsAre(-1.0));
  EXPECT_THAT(sharded_qp.Qp().constraint_upper_bounds, ElementsAre(1.0));
}

TEST(ShardedQuadraticProgramTest, SetConstraintUpperBound) {
  const int num_threads = 2;
  const int num_shards = 2;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);
  sharded_qp.SetConstraintBounds(/*constraint_index=*/0,
                                 /*lower_bound=*/std::nullopt,
                                 /*upper_bound=*/10.0);
  EXPECT_THAT(sharded_qp.Qp().constraint_lower_bounds, ElementsAre(-kInfinity));
  EXPECT_THAT(sharded_qp.Qp().constraint_upper_bounds, ElementsAre(10.0));
}

TEST(ShardedQuadraticProgramTest, SwapObjectiveVector) {
  const int num_threads = 2;
  const int num_shards = 2;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);
  Eigen::VectorXd new_objective{{1.0, 2.0}};
  sharded_qp.SwapObjectiveVector(new_objective);
  EXPECT_THAT(new_objective, ElementsAre(-1.0, -1.0));
  EXPECT_THAT(sharded_qp.Qp().objective_vector, ElementsAre(1.0, 2.0));
}

TEST(RescaleProblem, BasicTest) {
  // `TestDiagonalQp1()` is:
  // min 4x_1^2 + x_2^2 - x_1 - x_2 +5
  // s.t. -inf <= x_1 + x_2 <=1,
  //       1<=x_1<=2, -2<=x_2<=4.
  // After rescaling with `row_scaling_vec` [0.5] and `col_scaling_vec` [1,0.5],
  // `sharded_qp` becomes:
  // min 4x_1^2 + 0.25x_2^2 - x_1 - 0.5x_2 +5
  // s.t.  -inf <= 0.5x_1 + 0.25x_2 <=1,
  //       1<=x_1<=2, -4<=x_2<=8.
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);
  const Eigen::VectorXd col_scaling_vec{{1, 0.5}};
  const Eigen::VectorXd row_scaling_vec{{0.5}};
  sharded_qp.RescaleQuadraticProgram(col_scaling_vec, row_scaling_vec);

  EXPECT_THAT(sharded_qp.Qp().constraint_lower_bounds, ElementsAre(-kInfinity));
  EXPECT_THAT(sharded_qp.Qp().constraint_upper_bounds, ElementsAre(0.5));
  EXPECT_THAT(sharded_qp.Qp().variable_lower_bounds, ElementsAre(1, -4));
  EXPECT_THAT(sharded_qp.Qp().variable_upper_bounds, ElementsAre(2, 8));
  EXPECT_THAT(sharded_qp.Qp().objective_vector, ElementsAre(-1, -0.5));
  EXPECT_THAT(ToDense(sharded_qp.Qp().constraint_matrix),
              EigenArrayEq<double>({{0.5, 0.25}}));
  EXPECT_THAT(ToDense(sharded_qp.TransposedConstraintMatrix()),
              EigenArrayEq<double>({{0.5}, {0.25}}));
  EXPECT_THAT(sharded_qp.Qp().objective_matrix->diagonal(),
              EigenArrayEq<double>({4, 0.25}));
}

TEST(ShardedQuadraticProgramTest, ReplaceLargeConstraintBoundsWithInfinity) {
  const int num_threads = 2;
  const int num_shards = 2;
  ShardedQuadraticProgram sharded_qp(TestLp(), num_threads, num_shards);
  sharded_qp.ReplaceLargeConstraintBoundsWithInfinity(3.0);
  EXPECT_THAT(sharded_qp.Qp().constraint_lower_bounds,
              ElementsAre(kInfinity, -kInfinity, -kInfinity, -1));
  EXPECT_THAT(sharded_qp.Qp().constraint_upper_bounds,
              ElementsAre(kInfinity, kInfinity, kInfinity, 1));
}

}  // namespace
}  // namespace operations_research::pdlp
