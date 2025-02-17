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

#include "ortools/sat/symmetry_util.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

std::unique_ptr<SparsePermutation> MakePerm(
    int size, absl::Span<const std::initializer_list<int>> cycles) {
  auto perm = std::make_unique<SparsePermutation>(size);
  for (const auto& cycle : cycles) {
    for (const int x : cycle) {
      perm->AddToCurrentCycle(x);
    }
    perm->CloseCurrentCycle();
  }
  return perm;
}

TEST(GetOrbitsTest, BasicExample) {
  const int n = 10;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  generators.push_back(MakePerm(n, {{0, 1, 2}, {7, 8}}));
  generators.push_back(MakePerm(n, {{3, 2, 7}}));
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

  generators.push_back(MakePerm(n, {{0, 1}, {4, 5}, {8, 7}}));
  generators.push_back(MakePerm(n, {{2, 1}, {5, 3}, {6, 7}}));

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

  generators.push_back(MakePerm(n, {{0, 1}, {4, 5}, {8, 7}}));
  generators.push_back(MakePerm(n, {{1, 2}, {5, 8}, {6, 9}}));

  const std::vector<std::vector<int>> orbitope =
      BasicOrbitopeExtraction(generators);
  ASSERT_EQ(orbitope.size(), 3);
  EXPECT_THAT(orbitope[0], ElementsAre(0, 1));
  EXPECT_THAT(orbitope[1], ElementsAre(4, 5));
  EXPECT_THAT(orbitope[2], ElementsAre(8, 7));
}

TEST(GetSchreierVectorTest, Square) {
  const int n = 4;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  generators.push_back(MakePerm(n, {{0, 1, 2, 3}}));
  generators.push_back(MakePerm(n, {{1, 3}}));

  std::vector<int> schrier_vector, orbit;
  GetSchreierVectorAndOrbit(0, generators, &schrier_vector, &orbit);
  EXPECT_THAT(schrier_vector, ElementsAre(-1, 0, 0, 1));
}

TEST(GetSchreierVectorTest, ComplicatedGroup) {
  // See Chapter 7 of Butler, Gregory, ed. Fundamental algorithms for
  // permutation groups. Berlin, Heidelberg: Springer Berlin Heidelberg, 1991.
  const int n = 11;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  generators.push_back(MakePerm(n, {{0, 3, 4, 10, 5, 9, 2, 1}, {6, 7}}));
  generators.push_back(MakePerm(n, {{0, 3, 4, 10, 5, 9, 2, 1}, {7, 8}}));
  generators.push_back(MakePerm(n, {{0, 3, 1, 2}, {4, 10, 9, 5}}));

  std::vector<int> schrier_vector, orbit;
  GetSchreierVectorAndOrbit(0, generators, &schrier_vector, &orbit);
  EXPECT_THAT(schrier_vector, ElementsAre(-1, 2, 2, 0, 0, 0, -1, -1, -1, 2, 0));
  std::vector<int> generators_idx = TracePoint(9, schrier_vector, generators);
  std::vector<std::string> points = {"0", "1", "2", "3", "4", "5",
                                     "6", "7", "8", "9", "10"};
  for (const int i : generators_idx) {
    generators[i]->ApplyToDenseCollection(points);
  }
  // It needs to take the base point 0 to the traced point 9.
  EXPECT_THAT(points, ElementsAre("9", "10", "1", "4", "5", "2", "7", "6", "8",
                                  "3", "0"));
  GetSchreierVectorAndOrbit(6, generators, &schrier_vector, &orbit);
  EXPECT_THAT(orbit, UnorderedElementsAre(6, 7, 8));
  EXPECT_THAT(schrier_vector,
              ElementsAre(-1, -1, -1, -1, -1, -1, -1, 0, 1, -1, -1));
}

TEST(GetSchreierVectorTest, ProjectivePlaneOrderTwo) {
  const int n = 7;
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  generators.push_back(MakePerm(n, {{0, 1, 3, 4, 6, 2, 5}}));
  generators.push_back(MakePerm(n, {{1, 3}, {2, 4}}));

  std::vector<int> schrier_vector, orbit;
  GetSchreierVectorAndOrbit(0, generators, &schrier_vector, &orbit);
  EXPECT_THAT(schrier_vector, ElementsAre(-1, 0, 1, 0, 0, 0, 0));
  EXPECT_THAT(orbit, UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6));

  // Now let's see the stabilizer of the point 0.
  std::vector<std::unique_ptr<SparsePermutation>> stabilizer;

  stabilizer.push_back(MakePerm(n, {{1, 3}, {2, 4}}));
  stabilizer.push_back(MakePerm(n, {{3, 4}, {5, 6}}));
  stabilizer.push_back(MakePerm(n, {{3, 5}, {4, 6}}));

  GetSchreierVectorAndOrbit(1, stabilizer, &schrier_vector, &orbit);
  EXPECT_THAT(schrier_vector, ElementsAre(-1, -1, 0, 0, 1, 2, 2));
}

TEST(CreateSparsePermutationFromProtoTest, BasicReading) {
  const SparsePermutationProto input = ParseTestProto(R"pb(
    support: [ 1, 0, 3, 2, 7, 8, 9 ]
    cycle_sizes: [ 2, 2, 3 ]
  )pb");
  std::unique_ptr<SparsePermutation> sp =
      CreateSparsePermutationFromProto(10, input);
  EXPECT_EQ(sp->DebugString(), "(0 1) (2 3) (7 8 9)");
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
