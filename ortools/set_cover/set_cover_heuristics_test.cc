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

#include "ortools/set_cover/set_cover_heuristics.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {
namespace {

std::vector<bool> SieveOfEratosthenes(const int n) {
  if (n < 2) return std::vector<bool>(n + 1, false);
  std::vector<bool> is_prime(n + 1, true);
  is_prime[0] = false;
  is_prime[1] = false;
  for (int p = 2; p * p <= n; ++p) {
    if (is_prime[p]) {
      for (int i = p * p; i <= n; i += p) {
        is_prime[i] = false;
      }
    }
  }
  return is_prime;
}

SetCoverModel CreateSimpleModel() {
  SetCoverModel model;
  model.AddEmptySubset(1.0);  // S0 = {0, 1}
  model.AddElementToLastSubset(0);
  model.AddElementToLastSubset(1);

  model.AddEmptySubset(2.0);  // S1 = {1, 2}
  model.AddElementToLastSubset(1);
  model.AddElementToLastSubset(2);

  model.AddEmptySubset(3.0);  // S2 = {0, 2}
  model.AddElementToLastSubset(0);
  model.AddElementToLastSubset(2);

  model.CreateSparseRowView();
  return model;
}

TEST(SetCoverHeuristicsTest, DualAscentOptimizer) {
  SetCoverModel model = CreateSimpleModel();
  SetCoverInvariant inv(&model);
  DualAscentOptimizer lb(&inv);
  EXPECT_TRUE(lb.SetNumRandomPasses(10).Optimize());
  EXPECT_DOUBLE_EQ(inv.LowerBound(), 3.0);
}

TEST(SetCoverHeuristicsTest, DualAscentOptimizerFullRandom) {
  SetCoverModel model = CreateSimpleModel();
  SetCoverInvariant inv(&model);
  DualAscentOptimizer lb(&inv);
  EXPECT_TRUE(lb.UseFullRandomization(true).SetNumRandomPasses(10).Optimize());
  EXPECT_DOUBLE_EQ(inv.LowerBound(), 3.0);
}

TEST(SetCoverHeuristicsTest, GreedySolutionOptimizer) {
  SetCoverModel model = CreateSimpleModel();
  SetCoverInvariant inv(&model);
  GreedySolutionOptimizer greedy(&inv);
  EXPECT_TRUE(greedy.Optimize());
  EXPECT_LE(inv.cost(), 6.0);
}

TEST(SetCoverHeuristicsTest, SteepestSearch) {
  SetCoverModel model = CreateSimpleModel();
  SetCoverInvariant inv(&model);
  GreedySolutionOptimizer greedy(&inv);
  EXPECT_TRUE(greedy.Optimize());
  SteepestSearch steepest(&inv);
  EXPECT_TRUE(steepest.SetMaxIterations(10).Optimize());
}

TEST(SetCoverHeuristicsTest, GenerateFirstNPrimes) {
  const std::vector<int32_t> primes = internal::GenerateFirstNPrimes(1000);
  ASSERT_EQ(primes.size(), 1000);
  const int32_t largest_prime = primes.back();
  const std::vector<bool> is_prime = SieveOfEratosthenes(largest_prime);

  std::vector<int32_t> sieved_primes;
  for (int i = 2; i <= largest_prime; ++i) {
    if (is_prime[i]) {
      sieved_primes.push_back(i);
    }
  }
  EXPECT_EQ(primes, sieved_primes);
}

TEST(SetCoverHeuristicsTest, ComputeSegmentStarts) {
  SetCoverModel model;
  model.AddEmptySubset(1.0);  // S0 = {0, 1, 3}
  model.AddElementToLastSubset(0);
  model.AddElementToLastSubset(1);
  model.AddElementToLastSubset(3);

  model.AddEmptySubset(2.0);  // S1 = {1, 2}
  model.AddElementToLastSubset(1);
  model.AddElementToLastSubset(2);

  model.AddEmptySubset(3.0);  // S2 = {0, 2}
  model.AddElementToLastSubset(0);
  model.AddElementToLastSubset(2);

  model.CreateSparseRowView();

  // Degrees:
  // elt 3: degree 1
  // elt 0: degree 2
  // elt 1: degree 2
  // elt 2: degree 2
  const std::vector<ElementIndex> sorted_elements = {
      ElementIndex(3), ElementIndex(0), ElementIndex(1), ElementIndex(2)};
  const std::vector<size_t> segment_starts =
      internal::ComputeSegmentStarts(sorted_elements, model.rows());
  EXPECT_THAT(segment_starts, ::testing::ElementsAre(0, 1, 4));

  const std::vector<ElementIndex> empty_elements;
  const std::vector<size_t> empty_segment_starts =
      internal::ComputeSegmentStarts(empty_elements, model.rows());
  EXPECT_THAT(empty_segment_starts, ::testing::ElementsAre(0));
}

}  // namespace
}  // namespace operations_research
