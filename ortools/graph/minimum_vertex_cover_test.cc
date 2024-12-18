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

#include "ortools/graph/minimum_vertex_cover.h"

#include <vector>

#include "absl/algorithm/container.h"
#include "gtest/gtest.h"

namespace operations_research {
namespace {

// Creates the complete bipartite graph K(n, m).
std::vector<std::vector<int>> MakeCompleteBipartiteGraph(int num_left,
                                                         int num_right) {
  std::vector<int> adjacencies(num_right);
  absl::c_iota(adjacencies, num_left);
  return std::vector<std::vector<int>>(num_left, adjacencies);
}

TEST(BipartiteMinimumVertexCoverTest, BasicBehavior) {
  const int num_right = 4;
  const std::vector<std::vector<int>> left_to_right = {
      {5}, {4, 5, 6}, {5}, {5, 6, 7}};
  const auto cover = BipartiteMinimumVertexCover(left_to_right, num_right);
  EXPECT_EQ(absl::c_count(cover, true), 3);
  EXPECT_EQ(absl::c_count(cover, false), 5);
}

TEST(BipartiteMinimumVertexCoverTest, StarGraph) {
  const std::vector<std::vector<int>> left_to_right =
      MakeCompleteBipartiteGraph(1, 4);
  const auto cover = BipartiteMinimumVertexCover(left_to_right, 4);
  EXPECT_EQ(absl::c_count(cover, true), 1);
  EXPECT_EQ(absl::c_count(cover, false), 4);
}

TEST(BipartiteMinimumVertexCoverTest, UtilityGraph) {
  const std::vector<std::vector<int>> left_to_right =
      MakeCompleteBipartiteGraph(3, 3);
  const auto cover = BipartiteMinimumVertexCover(left_to_right, 3);
  EXPECT_EQ(absl::c_count(cover, true), 3);
  EXPECT_EQ(absl::c_count(cover, false), 3);
}

TEST(BipartiteMinimumVertexCoverTest, DuplicateEdges) {
  const int num_right = 4;
  const std::vector<std::vector<int>> left_to_right = {
      {5, 5}, {4, 4, 5, 6}, {5, 5, 5}, {5, 5, 5, 6, 6, 7}};
  EXPECT_EQ(absl::c_count(BipartiteMinimumVertexCover(left_to_right, num_right),
                          true),
            3);
  EXPECT_EQ(absl::c_count(BipartiteMinimumVertexCover(left_to_right, num_right),
                          false),
            5);
}

TEST(BipartiteMinimumVertexCoverTest, Empty) {
  const int num_right = 4;
  const std::vector<std::vector<int>> left_to_right = {{}, {}};
  EXPECT_EQ(absl::c_count(BipartiteMinimumVertexCover(left_to_right, num_right),
                          false),
            6);
}

}  // namespace
}  // namespace operations_research
