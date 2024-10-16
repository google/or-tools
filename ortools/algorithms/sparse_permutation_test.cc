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

#include "ortools/algorithms/sparse_permutation.h"

#include <memory>
#include <random>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/random/distributions.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;

TEST(SparsePermutationTest, SimpleExample) {
  SparsePermutation permutation(12);
  permutation.AddToCurrentCycle(4);
  permutation.AddToCurrentCycle(2);
  permutation.AddToCurrentCycle(7);
  permutation.CloseCurrentCycle();
  permutation.AddToCurrentCycle(6);
  permutation.AddToCurrentCycle(1);
  permutation.CloseCurrentCycle();
  EXPECT_EQ("(1 6) (2 7 4)", permutation.DebugString());
  EXPECT_EQ(2, permutation.NumCycles());
  EXPECT_EQ(5, permutation.Support().size());
  EXPECT_THAT(permutation.Cycle(0), ElementsAre(4, 2, 7));
  EXPECT_THAT(permutation.Cycle(1), ElementsAre(6, 1));
}

TEST(SparsePermutationTest, RemoveCycles) {
  SparsePermutation permutation(12);
  permutation.AddToCurrentCycle(4);
  permutation.AddToCurrentCycle(2);
  permutation.AddToCurrentCycle(7);
  permutation.CloseCurrentCycle();
  permutation.AddToCurrentCycle(6);
  permutation.AddToCurrentCycle(1);
  permutation.CloseCurrentCycle();
  permutation.AddToCurrentCycle(9);
  permutation.AddToCurrentCycle(8);
  permutation.CloseCurrentCycle();
  EXPECT_EQ("(1 6) (2 7 4) (8 9)", permutation.DebugString());
  permutation.RemoveCycles({});
  EXPECT_EQ("(1 6) (2 7 4) (8 9)", permutation.DebugString());
  permutation.RemoveCycles({2, 1});
  EXPECT_EQ("(2 7 4)", permutation.DebugString());
  permutation.RemoveCycles({0});
  EXPECT_EQ("", permutation.DebugString());
  permutation.RemoveCycles({});
  EXPECT_EQ("", permutation.DebugString());
}

TEST(SparsePermutationTest, Identity) {
  SparsePermutation permutation(1000);
  EXPECT_EQ("", permutation.DebugString());
  EXPECT_EQ(0, permutation.Support().size());
  EXPECT_EQ(0, permutation.NumCycles());
}

TEST(SparsePermutationTest, ApplyToVector) {
  std::vector<std::string> v = {"0", "1", "2", "3", "4", "5", "6", "7", "8"};
  SparsePermutation permutation(v.size());
  permutation.AddToCurrentCycle(4);
  permutation.AddToCurrentCycle(2);
  permutation.AddToCurrentCycle(7);
  permutation.CloseCurrentCycle();
  permutation.AddToCurrentCycle(6);
  permutation.AddToCurrentCycle(1);
  permutation.CloseCurrentCycle();
  permutation.ApplyToDenseCollection(v);
  EXPECT_THAT(v, ElementsAre("0", "6", "7", "3", "2", "5", "1", "4", "8"));
}

// Generate a bunch of permutation on a 'huge' space, but that have very few
// displacements. This would OOM if the implementation was O(N); we verify
// that it doesn't.
TEST(SparsePermutationTest, Sparsity) {
  const int kSpaceSize = 1000000000;  // Fits largely in an int32_t.
  const int kNumPermutationsToGenerate = 1000;
  const int kAverageCycleSize = 10;
  const int kAverageNumCycles = 3;
  std::vector<std::unique_ptr<SparsePermutation>> permutations;
  std::mt19937 random(12345);
  absl::flat_hash_set<int> cycle;
  for (int i = 0; i < kNumPermutationsToGenerate; ++i) {
    SparsePermutation* permutation = new SparsePermutation(kSpaceSize);
    permutations.emplace_back(permutation);
    const int num_cycles = absl::Uniform(random, 0, 2 * kAverageNumCycles + 1);
    for (int c = 0; c < num_cycles; ++c) {
      const int cycle_size =
          absl::Uniform(random, 0, 2 * kAverageCycleSize - 1) + 2;
      cycle.clear();
      while (cycle.size() < cycle_size) {
        cycle.insert(absl::Uniform(random, 0, kSpaceSize));
      }
      for (const int e : cycle) permutation->AddToCurrentCycle(e);
      permutation->CloseCurrentCycle();
    }
    EXPECT_LT(permutation->DebugString().size(),
              100 * kAverageCycleSize * kAverageNumCycles)
        << permutation->DebugString();
  }
}

}  // namespace
}  // namespace operations_research
