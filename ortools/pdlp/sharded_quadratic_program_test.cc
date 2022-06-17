// Copyright 2010-2022 Google LLC
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

#include "Eigen/Core"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
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

TEST(RescaleProblem, BasicTest) {
  // The original qp is:
  // min 4x_1^2 + x_2^2 - x_1 - x_2 +5
  // s.t. -inf <= x_1 + x_2 <=1,
  //       1<=x_1<=2, -2<=x_2<=4.
  // After rescaling with row_scaling_vec [0.5] and col_scaling_vec [1,0.5],
  // the new qp becomes:
  // min 4x_1^2 + 0.25x_2^2 - x_1 - 0.5x_2 +5
  // s.t.  -inf <= 0.5x_1 + 0.25x_2 <=1,
  //       1<=x_1<=2, -4<=x_2<=8.
  const int num_threads = 2;
  const int num_shards = 10;
  ShardedQuadraticProgram sharded_qp(TestDiagonalQp1(), num_threads,
                                     num_shards);
  Eigen::VectorXd col_scaling_vec(2);
  Eigen::VectorXd row_scaling_vec(1);
  col_scaling_vec << 1, 0.5;
  row_scaling_vec << 0.5;
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

}  // namespace
}  // namespace operations_research::pdlp
