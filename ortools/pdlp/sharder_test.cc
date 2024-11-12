// Copyright 2010-2024 Google LLC
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

#include "ortools/pdlp/sharder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/random/distributions.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/pdlp/scheduler.h"

namespace operations_research::pdlp {
namespace {

using ::Eigen::DiagonalMatrix;
using ::Eigen::VectorXd;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::Test;
using Shard = Sharder::Shard;

// Returns a sparse representation of the matrix
//  7 -0.5 . .
//  1   .  3 2
// -1  .   . 5
Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> TestSparseMatrix() {
  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> mat(3, 4);
  mat.coeffRef(0, 0) = 7;
  mat.coeffRef(0, 1) = -0.5;
  mat.coeffRef(1, 0) = 1;
  mat.coeffRef(1, 2) = 3;
  mat.coeffRef(1, 3) = 2;
  mat.coeffRef(2, 0) = -1;
  mat.coeffRef(2, 3) = 5;
  mat.makeCompressed();
  return mat;
}

// A random matrix with a power law distribution of non-zeros per col.
// Specifically col i has order n/(i+1) non-zeros in expectation.
Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> LargeSparseMatrix(
    const int64_t size) {
  // Deterministic RNG.
  std::mt19937 rand(48709241);
  std::vector<Eigen::Triplet<double, int64_t>> triplets;
  for (int64_t col = 0; col < size; ++col) {
    int64_t row = -1;
    while (row < size) {
      row += absl::Uniform(rand, 1, col + 2);
      if (row < size) {
        double value = absl::Uniform(rand, 1, 10);
        triplets.emplace_back(row, col, value);
      }
    }
  }
  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> mat(size, size);
  mat.setFromTriplets(triplets.begin(), triplets.end());
  return mat;
}

// Verify that `sharder` is consistent and has shards of reasonable mass.
// Requires `target_num_shards > 0` and `!element_masses.empty()`.
void VerifySharder(const Sharder& sharder, const int target_num_shards,
                   const std::vector<int64_t>& element_masses) {
  int64_t num_elements = element_masses.size();
  int num_shards = sharder.NumShards();
  ASSERT_EQ(sharder.NumElements(), num_elements);
  ASSERT_GE(num_elements, 1);
  ASSERT_GE(num_shards, 1);
  int64_t elements_so_far = 0;
  for (int shard = 0; shard < num_shards; ++shard) {
    int64_t shard_start = sharder.ShardStart(shard);
    EXPECT_EQ(shard_start, elements_so_far) << " in shard: " << shard;
    int64_t shard_mass = 0;
    EXPECT_GE(sharder.ShardSize(shard), 1) << " in shard: " << shard;
    EXPECT_GE(sharder.ShardMass(shard), 1) << " in shard: " << shard;
    for (int64_t i = 0; i < sharder.ShardSize(shard); ++i) {
      shard_mass += element_masses[shard_start + i];
    }
    EXPECT_EQ(shard_mass, sharder.ShardMass(shard)) << " in shard: " << shard;
    elements_so_far += sharder.ShardSize(shard);
  }
  EXPECT_EQ(elements_so_far, num_elements);

  EXPECT_LE(num_shards, 2 * target_num_shards);
  ASSERT_GE(target_num_shards, 1);
  const int64_t overall_mass =
      std::accumulate(element_masses.begin(), element_masses.end(), int64_t{0});
  const int64_t max_element_mass =
      *std::max_element(element_masses.begin(), element_masses.end());
  const int64_t upper_mass_limit = std::max(
      max_element_mass,
      MathUtil::CeilOfRatio(max_element_mass, int64_t{2}) +
          MathUtil::CeilOfRatio(overall_mass, int64_t{target_num_shards}));
  const int64_t lower_mass_limit =
      overall_mass / target_num_shards -
      MathUtil::CeilOfRatio(max_element_mass, int64_t{2});
  for (int shard = 0; shard < sharder.NumShards(); ++shard) {
    EXPECT_LE(sharder.ShardMass(shard), upper_mass_limit)
        << " in shard: " << shard;
    if (shard + 1 < sharder.NumShards()) {
      EXPECT_GE(sharder.ShardMass(shard), lower_mass_limit)
          << " in shard: " << shard;
    }
  }
}

TEST(SharderTest, SharderFromMatrix) {
  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> mat =
      TestSparseMatrix();
  Sharder sharder(mat, /*num_shards=*/2, nullptr);
  VerifySharder(sharder, 2, {4, 2, 2, 3});
}

TEST(SharderTest, UniformSharder) {
  Sharder sharder(/*num_elements=*/10, /*num_shards=*/3, nullptr);
  VerifySharder(sharder, 3, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
}

TEST(SharderTest, UniformSharderFromOtherSharder) {
  Sharder other_sharder(/*num_elements=*/5, /*num_shards=*/3, nullptr);
  Sharder sharder(other_sharder, /*num_elements=*/10);
  VerifySharder(sharder, other_sharder.NumShards(),
                {1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
}

TEST(SharderTest, UniformSharderExcessiveShards) {
  Sharder sharder(/*num_elements=*/5, /*num_shards=*/7, nullptr);
  EXPECT_THAT(sharder.ShardStartsForTesting(), ElementsAre(0, 1, 2, 3, 4, 5));
  VerifySharder(sharder, 7, {1, 1, 1, 1, 1});
}

TEST(SharderTest, UniformSharderHugeNumShards) {
  Sharder sharder(/*num_elements=*/5, /*num_shards=*/1'000'000'000, nullptr);
  EXPECT_THAT(sharder.ShardStartsForTesting(), ElementsAre(0, 1, 2, 3, 4, 5));
  VerifySharder(sharder, 7, {1, 1, 1, 1, 1});
}

TEST(SharderTest, UniformSharderOneShard) {
  Sharder sharder(/*num_elements=*/5, /*num_shards=*/1, nullptr);
  EXPECT_THAT(sharder.ShardStartsForTesting(), ElementsAre(0, 5));
  VerifySharder(sharder, 1, {1, 1, 1, 1, 1});
}

TEST(SharderTest, UniformSharderOneElementVector) {
  Sharder sharder(/*num_elements=*/1, /*num_shards=*/5, nullptr);
  EXPECT_THAT(sharder.ShardStartsForTesting(), ElementsAre(0, 1));
  VerifySharder(sharder, 5, {1});
}

TEST(SharderTest, UniformSharderZeroElementVector) {
  Sharder sharder(/*num_elements=*/0, /*num_shards=*/3, nullptr);
  EXPECT_THAT(sharder.ShardStartsForTesting(), ElementsAre(0));
  EXPECT_EQ(sharder.NumShards(), 0);
  EXPECT_EQ(sharder.NumElements(), 0);
  sharder.ParallelForEachShard([](const Shard& /*shard*/) {
    LOG(FATAL) << "There are no shards so this shouldn't be called.";
  });
}

TEST(SharderTest, UniformSharderFromOtherZeroElementSharder) {
  Sharder empty_sharder(/*num_elements=*/0, /*num_shards=*/3, nullptr);
  EXPECT_THAT(empty_sharder.ShardStartsForTesting(), ElementsAre(0));
  EXPECT_EQ(empty_sharder.NumShards(), 0);
  EXPECT_EQ(empty_sharder.NumElements(), 0);
  Sharder sharder(empty_sharder, /*num_elements=*/5);
  EXPECT_THAT(sharder.ShardStartsForTesting(), ElementsAre(0, 5));
  VerifySharder(sharder, 1, {1, 1, 1, 1, 1});
}

TEST(ParallelSumOverShards, SmallExample) {
  const VectorXd vec{{1, 2, 3}};
  Sharder sharder(vec.size(), /*num_shards=*/2, nullptr);
  const double sum = sharder.ParallelSumOverShards(
      [&vec](const Shard& shard) { return shard(vec).sum(); });
  EXPECT_EQ(sum, 6.0);
}

TEST(ParallelSumOverShards, SmallExampleUsingVectorBlock) {
  VectorXd vec{{1, 2, 3}};
  auto vec_block = vec.segment(1, 2);
  Sharder sharder(vec_block.size(), /*num_shards=*/2, nullptr);
  const double sum = sharder.ParallelSumOverShards(
      [&vec_block](const Shard& shard) { return shard(vec_block).sum(); });
  EXPECT_EQ(sum, 5.0);
}

TEST(ParallelSumOverShards, SmallExampleUsingConstVectorBlock) {
  const VectorXd const_vec{{1, 2, 3}};
  auto vec_block = const_vec.segment(1, 2);
  Sharder sharder(vec_block.size(), /*num_shards=*/2, nullptr);
  const double sum = sharder.ParallelSumOverShards(
      [&vec_block](const Shard& shard) { return shard(vec_block).sum(); });
  EXPECT_EQ(sum, 5.0);
}

TEST(ParallelSumOverShards, SmallExampleUsingDiagonalMatrix) {
  DiagonalMatrix<double, Eigen::Dynamic> diag{{1, 2, 3}};
  Sharder sharder(diag.cols(), /*num_shards=*/2, nullptr);
  const double sum = sharder.ParallelSumOverShards(
      [&diag](const Shard& shard) { return shard(diag).diagonal().sum(); });
  EXPECT_EQ(sum, 6.0);
}

TEST(ParallelSumOverShards, SmallExampleUsingDiagonalMatrixMultiplication) {
  DiagonalMatrix<double, Eigen::Dynamic> diag{{1, 2, 3}};
  VectorXd vec{{1, 1, 1}};
  VectorXd answer(3);
  Sharder sharder(diag.cols(), /*num_shards=*/2, nullptr);
  sharder.ParallelForEachShard(
      [&](const Shard& shard) { shard(answer) = shard(diag) * shard(vec); });
  EXPECT_THAT(answer, ElementsAre(1.0, 2.0, 3.0));
}

TEST(ParallelTrueForAllShards, SmallTrueExample) {
  const VectorXd vec{{1, 2, 3}};
  Sharder sharder(vec.size(), /*num_shards=*/2, nullptr);
  const bool result = sharder.ParallelTrueForAllShards(
      [&vec](const Shard& shard) { return (shard(vec).array() > 0.0).all(); });
  EXPECT_TRUE(result);
}

TEST(ParallelTrueForAllShards, SmallFalseExample) {
  const VectorXd vec{{1, 2, 3}};
  Sharder sharder(vec.size(), /*num_shards=*/2, nullptr);
  const bool result = sharder.ParallelTrueForAllShards(
      [&vec](const Shard& shard) { return (shard(vec).array() < 2.5).all(); });
  EXPECT_FALSE(result);
}

TEST(MatrixVectorProductTest, SmallExample) {
  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> mat =
      TestSparseMatrix();
  Sharder sharder(mat, /*num_shards=*/3, nullptr);
  const VectorXd vec{{1, 2, 3}};
  VectorXd ans = TransposedMatrixVectorProduct(mat, vec, sharder);
  EXPECT_THAT(ans, ElementsAre(6.0, -0.5, 6.0, 19));
}

TEST(SetZeroTest, SmallExample) {
  Sharder sharder(3, /*num_shards=*/2, nullptr);
  VectorXd vec{{1, 7}};
  SetZero(sharder, vec);
  EXPECT_THAT(vec, ElementsAre(0.0, 0.0, 0.0));
}

TEST(ZeroVectorTest, SmallExample) {
  Sharder sharder(3, /*num_shards=*/2, nullptr);
  EXPECT_THAT(ZeroVector(sharder), ElementsAre(0.0, 0.0, 0.0));
}

TEST(OnesVectorTest, SmallExample) {
  Sharder sharder(3, /*num_shards=*/2, nullptr);
  EXPECT_THAT(OnesVector(sharder), ElementsAre(1.0, 1.0, 1.0));
}

TEST(AddScaledVectorTest, SmallExample) {
  Sharder sharder(3, /*num_shards=*/2, nullptr);
  VectorXd vec1{{4, 5, 20}};
  const VectorXd vec2{{1, 7, 3}};
  AddScaledVector(2.0, vec2, sharder, /*dest=*/vec1);
  EXPECT_THAT(vec1, ElementsAre(6, 19, 26));
}

TEST(AssignVectorTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  VectorXd vec1;
  const VectorXd vec2{{1, 7, 3}};
  AssignVector(vec2, sharder, /*dest=*/vec1);
  EXPECT_THAT(vec1, ElementsAre(1, 7, 3));
}

TEST(CloneVectorTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  const VectorXd vec{{1, 7, 3}};
  EXPECT_THAT(CloneVector(vec, sharder), ElementsAre(1, 7, 3));
}

TEST(CoefficientWiseProductInPlaceTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  VectorXd vec1{{4, 5, 20}};
  const VectorXd vec2{{1, 2, 3}};
  CoefficientWiseProductInPlace(/*scale=*/vec2, sharder,
                                /*dest=*/vec1);
  EXPECT_THAT(vec1, ElementsAre(4, 10, 60));
}

TEST(CoefficientWiseQuotientInPlaceTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  VectorXd vec1{{4, 6, 20}};
  const VectorXd vec2{{1, 2, 5}};
  CoefficientWiseQuotientInPlace(/*scale=*/vec2, sharder,
                                 /*dest=*/vec1);
  EXPECT_THAT(vec1, ElementsAre(4, 3, 4));
}

TEST(DotTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  const VectorXd vec1{{1, 2, 3}};
  const VectorXd vec2{{4, 5, 6}};
  double ans = Dot(vec1, vec2, sharder);
  EXPECT_THAT(ans, DoubleNear(4 + 10 + 18, 1.0e-13));
}

TEST(LInfNormTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  const VectorXd vec{{-1, 2, -3}};
  double ans = LInfNorm(vec, sharder);
  EXPECT_EQ(ans, 3);
}

TEST(LInfNormTest, EmptyExample) {
  Sharder sharder(/*num_elements=*/0, /*num_shards=*/2, nullptr);
  VectorXd vec(0);
  double ans = LInfNorm(vec, sharder);
  EXPECT_EQ(ans, 0);
}

TEST(L1NormTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  const VectorXd vec{{-1, 2, -3}};
  double ans = L1Norm(vec, sharder);
  EXPECT_EQ(ans, 6);
}

TEST(L1NormTest, EmptyExample) {
  Sharder sharder(/*num_elements=*/0, /*num_shards=*/2, nullptr);
  VectorXd vec(0);
  double ans = L1Norm(vec, sharder);
  EXPECT_EQ(ans, 0);
}

TEST(SquaredNormTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  const VectorXd vec{{1, 2, 3}};
  double ans = SquaredNorm(vec, sharder);
  EXPECT_THAT(ans, DoubleNear(1 + 4 + 9, 1.0e-13));
}

TEST(NormTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  const VectorXd vec{{1, 2, 3}};
  double ans = Norm(vec, sharder);
  EXPECT_THAT(ans, DoubleNear(std::sqrt(1 + 4 + 9), 1.0e-13));
}

TEST(SquaredDistanceTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  const VectorXd vec1{{1, 1, 1}};
  const VectorXd vec2{{1, 2, 3}};
  double ans = SquaredDistance(vec1, vec2, sharder);
  EXPECT_THAT(ans, DoubleNear(5, 1.0e-13));
}

TEST(DistanceTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  const VectorXd vec1{{1, 1, 1}};
  const VectorXd vec2{{1, 2, 3}};
  double ans = Distance(vec1, vec2, sharder);
  EXPECT_THAT(ans, DoubleNear(std::sqrt(5), 1.0e-13));
}

TEST(ScaledLInfNormTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  const VectorXd vec{{-1, 2, -3}};
  const VectorXd scale{{4, 6, 1}};
  double ans = ScaledLInfNorm(vec, scale, sharder);
  EXPECT_EQ(ans, 12);
}

TEST(ScaledSquaredNormTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  const VectorXd vec{{-1, 2, -3}};
  const VectorXd scale{{4, 6, 1}};
  double ans = ScaledSquaredNorm(vec, scale, sharder);
  EXPECT_EQ(ans, 169);
}

TEST(ScaledNormTest, SmallExample) {
  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);
  const VectorXd vec{{-1, 2, -3}};
  const VectorXd scale{{4, 6, 1}};
  double ans = ScaledNorm(vec, scale, sharder);
  EXPECT_EQ(ans, std::sqrt(169));
}

TEST(ScaledColLInfNorm, SmallExample) {
  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> mat =
      TestSparseMatrix();
  Sharder sharder(mat, /*num_shards=*/3, nullptr);
  const VectorXd row_scaling_vec{{1, -2, 1}};
  const VectorXd col_scaling_vec{{1, 2, -1, -1}};
  VectorXd answer =
      ScaledColLInfNorm(mat, row_scaling_vec, col_scaling_vec, sharder);
  EXPECT_THAT(answer, ElementsAre(7, 1, 6, 5));
}

TEST(ScaledColL2Norm, SmallExample) {
  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> mat =
      TestSparseMatrix();
  Sharder sharder(mat, /*num_shards=*/3, nullptr);
  const VectorXd row_scaling_vec{{1, -2, 1}};
  const VectorXd col_scaling_vec{{1, 2, -1, -1}};
  VectorXd answer =
      ScaledColL2Norm(mat, row_scaling_vec, col_scaling_vec, sharder);
  EXPECT_THAT(answer, ElementsAre(std::sqrt(54), 1.0, 6.0, std::sqrt(41)));
}

class VariousSizesTest : public testing::TestWithParam<int64_t> {};

TEST_P(VariousSizesTest, LargeMatVec) {
  const int64_t size = GetParam();
  Eigen::SparseMatrix<double, Eigen::ColMajor, int64_t> mat =
      LargeSparseMatrix(size);
  const int num_threads = 5;
  const int shards_per_thread = 3;
  GoogleThreadPoolScheduler scheduler(num_threads);
  Sharder sharder(mat, shards_per_thread * num_threads, &scheduler);
  VectorXd rhs = VectorXd::Random(size);
  VectorXd direct = mat.transpose() * rhs;
  VectorXd threaded = TransposedMatrixVectorProduct(mat, rhs, sharder);
  EXPECT_LE((direct - threaded).norm(), 1.0e-8);
}

TEST_P(VariousSizesTest, LargeVectors) {
  const int64_t size = GetParam();
  const int num_threads = 5;
  GoogleThreadPoolScheduler scheduler(num_threads);
  Sharder sharder(size, num_threads, &scheduler);
  VectorXd vec = VectorXd::Random(size);
  const double direct = vec.squaredNorm();
  const double threaded = SquaredNorm(vec, sharder);
  EXPECT_THAT(threaded, DoubleNear(direct, size * 1.0e-14));
}

INSTANTIATE_TEST_SUITE_P(VariousSizesTestInstantiation, VariousSizesTest,
                         testing::Values(10, 1000, 100 * 1000));

}  // namespace
}  // namespace operations_research::pdlp
