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

#include "ortools/sat/symmetry_util.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;

TEST(GetOrbitsTest, BasicExample) {
  const int n = 10;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  generators.push_back(std::make_unique<SparsePermutation>(n));
  generators[0]->AddToCurrentCycle(0);
  generators[0]->AddToCurrentCycle(1);
  generators[0]->AddToCurrentCycle(2);
  generators[0]->CloseCurrentCycle();
  generators[0]->AddToCurrentCycle(7);
  generators[0]->AddToCurrentCycle(8);
  generators[0]->CloseCurrentCycle();

  generators.push_back(std::make_unique<SparsePermutation>(n));
  generators[1]->AddToCurrentCycle(3);
  generators[1]->AddToCurrentCycle(2);
  generators[1]->AddToCurrentCycle(7);
  generators[1]->CloseCurrentCycle();
  const std::vector<int> orbits = GetOrbits(n, generators);
  for (const int i : std::vector<int>{0, 1, 2, 3, 7, 8}) {
    EXPECT_EQ(orbits[i], 0);
  }
  for (const int i : std::vector<int>{4, 5, 6, 9}) {
    EXPECT_EQ(orbits[i], -1);
  }
}

// Recover for generators (in a particular form)
// [0, 1, 2]
// [4, 5, 3]
// [8, 7, 6]
TEST(BasicOrbitopeExtractionTest, BasicExample) {
  const int n = 10;
  std::vector<std::unique_ptr<SparsePermutation>> generators;

  generators.push_back(std::make_unique<SparsePermutation>(n));
  generators[0]->AddToCurrentCycle(0);
  generators[0]->AddToCurrentCycle(1);
  generators[0]->CloseCurrentCycle();
  generators[0]->AddToCurrentCycle(4);
  generators[0]->AddToCurrentCycle(5);
  generators[0]->CloseCurrentCycle();
  generators[0]->AddToCurrentCycle(8);
  generators[0]->AddToCurrentCycle(7);
  generators[0]->CloseCurrentCycle();

  generators.push_back(std::make_unique<SparsePermutation>(n));
  generators[1]->AddToCurrentCycle(2);
  generators[1]->AddToCurrentCycle(1);
  generators[1]->CloseCurrentCycle();
  generators[1]->AddToCurrentCycle(5);
  generators[1]->AddToCurrentCycle(3);
  generators[1]->CloseCurrentCycle();
  generators[1]->AddToCurrentCycle(6);
  generators[1]->AddToCurrentCycle(7);
  generators[1]->CloseCurrentCycle();

  const std::vector<std::vector<int>> orbitope =
      BasicOrbitopeExtraction(generators);
  ASSERT_EQ(orbitope.size(), 3);
  EXPECT_THAT(orbitope[0], ElementsAre(0, 1, 2));
  EXPECT_THAT(orbitope[1], ElementsAre(4, 5, 3));
  EXPECT_THAT(orbitope[2], ElementsAre(8, 7, 6));
}

// This one is trickier and is not an orbitope because 8 appear twice. So it
// would be incorrect to "grow" the first two columns with the 3rd one.
// [0, 1, 2]
// [4, 5, 8]
// [8, 7, 9]
TEST(BasicOrbitopeExtractionTest, NotAnOrbitopeBecauseOfDuplicates) {
  const int n = 10;
  std::vector<std::unique_ptr<SparsePermutation>> generators;

  generators.push_back(std::make_unique<SparsePermutation>(n));
  generators[0]->AddToCurrentCycle(0);
  generators[0]->AddToCurrentCycle(1);
  generators[0]->CloseCurrentCycle();
  generators[0]->AddToCurrentCycle(4);
  generators[0]->AddToCurrentCycle(5);
  generators[0]->CloseCurrentCycle();
  generators[0]->AddToCurrentCycle(8);
  generators[0]->AddToCurrentCycle(7);
  generators[0]->CloseCurrentCycle();

  generators.push_back(std::make_unique<SparsePermutation>(n));
  generators[1]->AddToCurrentCycle(1);
  generators[1]->AddToCurrentCycle(2);
  generators[1]->CloseCurrentCycle();
  generators[1]->AddToCurrentCycle(5);
  generators[1]->AddToCurrentCycle(8);
  generators[1]->CloseCurrentCycle();
  generators[1]->AddToCurrentCycle(6);
  generators[1]->AddToCurrentCycle(9);
  generators[1]->CloseCurrentCycle();

  const std::vector<std::vector<int>> orbitope =
      BasicOrbitopeExtraction(generators);
  ASSERT_EQ(orbitope.size(), 3);
  EXPECT_THAT(orbitope[0], ElementsAre(0, 1));
  EXPECT_THAT(orbitope[1], ElementsAre(4, 5));
  EXPECT_THAT(orbitope[2], ElementsAre(8, 7));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
