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

#include "ortools/pdlp/test_util.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "gmock/gmock.h"
#include "ortools/pdlp/quadratic_program.h"

namespace operations_research::pdlp {

using ::testing::ElementsAre;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

QuadraticProgram TestLp() {
  QuadraticProgram lp(4, 4);
  lp.constraint_lower_bounds << 12, -kInfinity, -4, -1;
  lp.constraint_upper_bounds << 12, 7, kInfinity, 1;
  lp.variable_lower_bounds << -kInfinity, -2, -kInfinity, 2.5;
  lp.variable_upper_bounds << kInfinity, kInfinity, 6, 3.5;
  std::vector<Eigen::Triplet<double, int64_t>> triplets = {
      {0, 0, 2}, {0, 1, 1}, {0, 2, 1},   {0, 3, 2}, {1, 0, 1},
      {1, 2, 1}, {2, 0, 4}, {3, 2, 1.5}, {3, 3, -1}};
  lp.constraint_matrix.setFromTriplets(triplets.begin(), triplets.end());

  lp.objective_vector << 5.5, -2, -1, 1;
  lp.objective_offset = -14;
  return lp;
}

void VerifyTestLp(const QuadraticProgram& qp, bool maximize) {
  const double objective_sign = maximize ? -1 : 1;
  EXPECT_THAT(objective_sign * qp.objective_offset, testing::DoubleEq(-14));
  EXPECT_THAT(objective_sign * qp.objective_vector,
              ElementsAre(5.5, -2, -1, 1));
  EXPECT_EQ(qp.objective_scaling_factor, objective_sign);
  EXPECT_FALSE(qp.objective_matrix.has_value());
  EXPECT_THAT(qp.variable_lower_bounds,
              ElementsAre(-kInfinity, -2, -kInfinity, 2.5));
  EXPECT_THAT(qp.variable_upper_bounds,
              ElementsAre(kInfinity, kInfinity, 6, 3.5));
  EXPECT_THAT(qp.constraint_lower_bounds, ElementsAre(12, -kInfinity, -4, -1));
  EXPECT_THAT(qp.constraint_upper_bounds, ElementsAre(12, 7, kInfinity, 1));
  EXPECT_THAT(ToDense(qp.constraint_matrix),
              EigenArrayEq<double>(
                  {{2, 1, 1, 2}, {1, 0, 1, 0}, {4, 0, 0, 0}, {0, 0, 1.5, -1}}));
}

QuadraticProgram TinyLp() {
  QuadraticProgram lp(4, 3);
  lp.objective_offset = -14;
  lp.objective_vector << 5, 2, 1, 1;
  lp.constraint_lower_bounds << 12, 7, 1;
  lp.constraint_upper_bounds << 12, kInfinity, kInfinity;
  lp.variable_lower_bounds << 0, 0, 0, 0;
  lp.variable_upper_bounds << 2, 4, 6, 3;
  lp.constraint_matrix.coeffRef(0, 0) = 2.0;
  lp.constraint_matrix.coeffRef(0, 1) = 1.0;
  lp.constraint_matrix.coeffRef(0, 2) = 1.0;
  lp.constraint_matrix.coeffRef(0, 3) = 2.0;
  lp.constraint_matrix.coeffRef(1, 0) = 1.0;
  lp.constraint_matrix.coeffRef(1, 2) = 1.0;
  lp.constraint_matrix.coeffRef(2, 2) = 1.0;
  lp.constraint_matrix.coeffRef(2, 3) = -1.0;
  lp.constraint_matrix.makeCompressed();
  return lp;
}

QuadraticProgram CorrelationClusteringLp() {
  QuadraticProgram lp(6, 3);
  lp.objective_offset = 4;
  lp.objective_vector << -1.0, -1.0, 1.0, -1.0, 1.0, -1.0;
  lp.constraint_lower_bounds << -1.0, -1.0, -1.0;
  lp.constraint_upper_bounds << kInfinity, kInfinity, kInfinity;
  lp.variable_lower_bounds << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0;
  lp.variable_upper_bounds << 1.0, 1.0, 1.0, 1.0, 1.0, 1.0;
  lp.constraint_matrix.coeffRef(0, 1) = -1.0;
  lp.constraint_matrix.coeffRef(0, 2) = 1.0;
  lp.constraint_matrix.coeffRef(0, 5) = -1.0;
  lp.constraint_matrix.coeffRef(1, 3) = -1.0;
  lp.constraint_matrix.coeffRef(1, 4) = 1.0;
  lp.constraint_matrix.coeffRef(1, 5) = -1.0;
  lp.constraint_matrix.coeffRef(2, 0) = -1.0;
  lp.constraint_matrix.coeffRef(2, 1) = -1.0;
  lp.constraint_matrix.coeffRef(2, 3) = 1.0;
  lp.constraint_matrix.makeCompressed();
  return lp;
}

QuadraticProgram CorrelationClusteringStarLp() {
  QuadraticProgram lp(6, 3);
  lp.objective_offset = 3;
  lp.objective_vector << -1.0, -1.0, -1.0, 1.0, 1.0, 1.0;
  lp.constraint_lower_bounds << -1.0, -1.0, -1.0;
  lp.constraint_upper_bounds << kInfinity, kInfinity, kInfinity;
  lp.variable_lower_bounds << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0;
  lp.variable_upper_bounds << 1.0, 1.0, 1.0, 1.0, 1.0, 1.0;
  lp.constraint_matrix.coeffRef(0, 0) = -1.0;
  lp.constraint_matrix.coeffRef(0, 1) = -1.0;
  lp.constraint_matrix.coeffRef(0, 3) = 1.0;
  lp.constraint_matrix.coeffRef(1, 0) = -1.0;
  lp.constraint_matrix.coeffRef(1, 2) = -1.0;
  lp.constraint_matrix.coeffRef(1, 4) = 1.0;
  lp.constraint_matrix.coeffRef(2, 1) = -1.0;
  lp.constraint_matrix.coeffRef(2, 2) = -1.0;
  lp.constraint_matrix.coeffRef(2, 5) = 1.0;
  lp.constraint_matrix.makeCompressed();
  return lp;
}

namespace {

Eigen::DiagonalMatrix<double, Eigen::Dynamic> ConstructDiagonal(
    const std::vector<double>& vec) {
  Eigen::DiagonalMatrix<double, Eigen::Dynamic> diag;
  diag.resize(vec.size());
  for (int i = 0; i < vec.size(); ++i) {
    diag.diagonal()[i] = vec[i];
  }
  return diag;
}

}  // namespace

QuadraticProgram TestDiagonalQp1() {
  QuadraticProgram qp(2, 1);
  qp.constraint_lower_bounds << -kInfinity;
  qp.constraint_upper_bounds << 1;
  qp.variable_lower_bounds << 1, -2;
  qp.variable_upper_bounds << 2, 4;
  qp.objective_vector << -1, -1;
  qp.objective_offset = 5;
  std::vector<Eigen::Triplet<double, int64_t>> constraint_triplets = {
      {0, 0, 1}, {0, 1, 1}};
  qp.constraint_matrix.setFromTriplets(constraint_triplets.begin(),
                                       constraint_triplets.end());
  qp.objective_matrix = ConstructDiagonal({4.0, 1.0});
  return qp;
}

QuadraticProgram TestDiagonalQp2() {
  QuadraticProgram qp(2, 1);
  qp.constraint_lower_bounds << 2;
  qp.constraint_upper_bounds << 2;
  qp.variable_lower_bounds << 0, 0;
  qp.variable_upper_bounds << kInfinity, kInfinity;
  qp.objective_vector << -3, -1;
  qp.objective_offset = 0;
  std::vector<Eigen::Triplet<double, int64_t>> constraint_triplets = {
      {0, 0, 1}, {0, 1, -1}};
  qp.constraint_matrix.setFromTriplets(constraint_triplets.begin(),
                                       constraint_triplets.end());
  qp.objective_matrix = ConstructDiagonal({1.0, 1.0});
  return qp;
}

QuadraticProgram TestDiagonalQp3() {
  QuadraticProgram qp(3, 2);
  qp.constraint_lower_bounds << 1, 4;
  qp.constraint_upper_bounds << 1, 4;
  qp.variable_lower_bounds << 0, 0, 0;
  qp.variable_upper_bounds << kInfinity, kInfinity, kInfinity;
  qp.objective_vector << 1, 0, -1;
  qp.objective_offset = 0;
  std::vector<Eigen::Triplet<double, int64_t>> constraint_triplets = {
      {0, 0, 1}, {0, 2, -1}, {1, 0, 2}};
  qp.constraint_matrix.setFromTriplets(constraint_triplets.begin(),
                                       constraint_triplets.end());
  qp.objective_matrix = ConstructDiagonal({0.0, 1.0, 2.0});
  return qp;
}

QuadraticProgram SmallInvalidProblemLp() {
  QuadraticProgram lp(2, 1);
  lp.constraint_matrix.coeffRef(0, 0) = 1.0;
  lp.constraint_matrix.coeffRef(0, 1) = -1.0;
  lp.constraint_lower_bounds << 2.0;
  lp.constraint_upper_bounds << 1.0;
  lp.variable_lower_bounds << 0.0, 0.0;
  lp.variable_upper_bounds << kInfinity, kInfinity;
  lp.constraint_matrix.makeCompressed();
  lp.objective_vector << 1.0, 1.0;
  return lp;
}

QuadraticProgram SmallInconsistentVariableBoundsLp() {
  QuadraticProgram lp(2, 1);
  lp.constraint_matrix.coeffRef(0, 0) = 1.0;
  lp.constraint_matrix.coeffRef(0, 1) = -1.0;
  lp.constraint_matrix.makeCompressed();
  lp.constraint_lower_bounds << -kInfinity;
  lp.constraint_upper_bounds << 1.0;
  lp.variable_lower_bounds << 2.0, 0.0;
  lp.variable_upper_bounds << 1.0, kInfinity;
  lp.objective_vector << 1.0, 1.0;
  return lp;
}

QuadraticProgram SmallPrimalInfeasibleLp() {
  QuadraticProgram lp(2, 2);
  lp.constraint_matrix.coeffRef(0, 0) = 1.0;
  lp.constraint_matrix.coeffRef(0, 1) = -1.0;
  lp.constraint_matrix.coeffRef(1, 0) = -1.0;
  lp.constraint_matrix.coeffRef(1, 1) = 1.0;
  lp.constraint_lower_bounds << -kInfinity, -kInfinity;
  lp.variable_lower_bounds << 0.0, 0.0;
  lp.variable_upper_bounds << kInfinity, kInfinity;
  lp.constraint_matrix.makeCompressed();

  lp.constraint_upper_bounds << 1.0, -2.0;
  lp.objective_vector << 1.0, 1.0;
  return lp;
}

QuadraticProgram SmallDualInfeasibleLp() {
  QuadraticProgram lp = SmallPrimalInfeasibleLp();
  lp.constraint_upper_bounds(1) = 2.0;
  lp.objective_vector *= -1.0;
  return lp;
}

QuadraticProgram SmallPrimalDualInfeasibleLp() {
  QuadraticProgram lp = SmallPrimalInfeasibleLp();
  lp.objective_vector *= -1.0;
  return lp;
}

QuadraticProgram SmallInitializationLp() {
  QuadraticProgram lp(2, 2);
  lp.constraint_matrix.coeffRef(0, 0) = 1.0;
  lp.constraint_matrix.coeffRef(0, 1) = 1.0;
  lp.constraint_matrix.coeffRef(1, 0) = 1.0;
  lp.constraint_matrix.coeffRef(1, 1) = 2.0;
  lp.constraint_lower_bounds << -kInfinity, -kInfinity;
  lp.constraint_upper_bounds << 2.0, 2.0;
  lp.variable_lower_bounds << 0.5, 0.5;
  lp.variable_upper_bounds << 2.0, 2.0;
  lp.constraint_matrix.makeCompressed();

  lp.objective_vector << -4.0, 0.0;
  return lp;
}

QuadraticProgram LpWithoutConstraints() {
  QuadraticProgram lp(2, 0);
  lp.variable_lower_bounds << 0.0, -kInfinity;
  lp.variable_upper_bounds << kInfinity, 0.0;
  lp.objective_vector << 4.0, 0.0;
  return lp;
}

void VerifyTestQp(const QuadraticProgram& qp, bool maximize) {
  const double objective_sign = maximize ? -1 : 1;
  EXPECT_EQ(qp.objective_scaling_factor, objective_sign);
  EXPECT_THAT(objective_sign * qp.objective_offset, testing::DoubleEq(5));
  EXPECT_THAT(objective_sign * qp.objective_vector, ElementsAre(-1, -1));
  ASSERT_TRUE(qp.objective_matrix.has_value());
  EXPECT_THAT(objective_sign * qp.objective_matrix->diagonal(),
              EigenArrayEq<double>({4, 1}));
  EXPECT_THAT(qp.variable_lower_bounds, ElementsAre(1, -2));
  EXPECT_THAT(qp.variable_upper_bounds, ElementsAre(2, 4));
  EXPECT_THAT(qp.constraint_lower_bounds, ElementsAre(-kInfinity));
  EXPECT_THAT(qp.constraint_upper_bounds, ElementsAre(1));
  EXPECT_THAT(ToDense(qp.constraint_matrix), EigenArrayEq<double>({{1, 1}}));
}

::Eigen::ArrayXXd ToDense(
    const Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t>& sparse_mat) {
  return ::Eigen::ArrayXXd(::Eigen::MatrixXd(sparse_mat));
}

}  // namespace operations_research::pdlp
