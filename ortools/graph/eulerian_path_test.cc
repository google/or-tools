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

#include "ortools/graph/eulerian_path.h"

#include <vector>

#include "absl/base/macros.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/graph_base/graph.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

void TestTour(const int arcs[][2], int num_nodes, int num_arcs, int root,
              bool eulerian, const int expected_tour[]) {
  util::ReverseArcListGraph<int, int> graph(num_nodes, num_arcs);
  for (int i = 0; i < num_arcs; ++i) {
    graph.AddArc(arcs[i][0], arcs[i][1]);
  }
  EXPECT_EQ(eulerian, IsEulerianGraph(graph));
  const std::vector<int> tour = root < 0
                                    ? BuildEulerianTour(graph)
                                    : BuildEulerianTourFromNode(graph, root);
  EXPECT_EQ(tour.size(), (eulerian && num_nodes != 0) ? num_arcs + 1 : 0);
  for (int i = 0; i < tour.size(); ++i) {
    EXPECT_EQ(expected_tour[i], tour[i]);
  }
}

void TestPath(const int arcs[][2], int num_nodes, int num_arcs, bool eulerian,
              const int expected_path[]) {
  util::ReverseArcListGraph<int, int> graph(num_nodes, num_arcs);
  for (int i = 0; i < num_arcs; ++i) {
    graph.AddArc(arcs[i][0], arcs[i][1]);
  }
  std::vector<int> odd_nodes;
  EXPECT_EQ(eulerian, IsSemiEulerianGraph(graph, &odd_nodes));
  const std::vector<int> path = BuildEulerianPath(graph);
  EXPECT_EQ(path.size(), num_nodes != 0 ? num_arcs + 1 : 0);
  for (int i = 0; i < path.size(); ++i) {
    EXPECT_EQ(expected_path[i], path[i]) << i;
  }
}

TEST(EulerianTourTest, EmptyGraph) {
  const auto kArcs = nullptr;
  const auto kExpectedTour = nullptr;
  TestTour(kArcs, 0, 0, -1, true, kExpectedTour);
}

// Builds a tour on the following graph:
//      0---------1
//      |         |
//      |         |
//      |         |
//      3---------2
//
TEST(EulerianTourTest, SimpleCycle) {
  const int kArcs[][2] = {{0, 1}, {0, 3}, {1, 2}, {2, 3}};
  const int kExpectedTour[] = {0, 1, 2, 3, 0};
  TestTour(kArcs, 4, ABSL_ARRAYSIZE(kArcs), 0, true, kExpectedTour);
  TestTour(kArcs, 4, ABSL_ARRAYSIZE(kArcs), -1, true, kExpectedTour);
}

// Builds a tour starting at 1 on the following graph:
//      0---------1
//      |        /|\
//      |       4 | 5
//      |        \|/
//      3---------2
//
TEST(EulerianTourTest, MultiCycle) {
  const int kArcs[][2] = {{0, 1}, {1, 2}, {1, 4}, {1, 5},
                          {2, 3}, {2, 4}, {2, 5}, {3, 0}};
  const int kExpectedTour[] = {1, 4, 2, 5, 1, 2, 3, 0, 1};
  TestTour(kArcs, 6, ABSL_ARRAYSIZE(kArcs), 1, true, kExpectedTour);
}

// Fails to build a tour on the following graph:
//      0---------1
//      |        / \
//      |       4   5
//      |        \ /
//      3---------2
//
TEST(EulerianTourTest, NonEulerian) {
  const int kArcs[][2] = {{0, 1}, {1, 4}, {1, 5}, {2, 3},
                          {2, 4}, {2, 5}, {3, 0}};
  const auto kExpectedTour = nullptr;
  TestTour(kArcs, 6, ABSL_ARRAYSIZE(kArcs), 1, false, kExpectedTour);
}

TEST(EulerianPathTest, EmptyGraph) {
  const auto kArcs = nullptr;
  const auto kExpectedPath = nullptr;
  TestPath(kArcs, 0, 0, true, kExpectedPath);
}

// Builds a path on the following graph:
//      0---------1
//      |        /|\
//      |       4 | 5
//      |        \|/
//      3---------2
//
TEST(EulerianPathTest, MultiCycle) {
  const int kArcs[][2] = {{0, 1}, {1, 2}, {1, 4}, {1, 5},
                          {2, 3}, {2, 4}, {2, 5}, {3, 0}};
  const int kExpectedPath[] = {0, 1, 4, 2, 5, 1, 2, 3, 0};
  TestPath(kArcs, 6, ABSL_ARRAYSIZE(kArcs), true, kExpectedPath);
}

// Builds a path on the following graph:
//      0---3
//      |  /|
//      | / |
//      |/  |
//      1---2
//      |
//      4
TEST(EulerianPathTest, TwoOddNodes1) {
  const int kArcs[][2] = {{0, 1}, {0, 3}, {1, 2}, {1, 3}, {1, 4}, {2, 3}};
  const int kExpectedPath[] = {3, 1, 2, 3, 0, 1, 4};
  TestPath(kArcs, 5, ABSL_ARRAYSIZE(kArcs), true, kExpectedPath);
}

// Builds a path on the following graph:
//        5
//       / \
//      0---4
//      |\ /|
//      | X |
//      |/ \|
//      1---2
//      |   |
//      6   3
TEST(EulerianPathTest, TwoOddNodes2) {
  const int kArcs[][2] = {{0, 1}, {0, 2}, {0, 4}, {0, 5}, {1, 2},
                          {1, 4}, {1, 6}, {2, 3}, {2, 4}, {4, 5}};
  const int kExpectedPath[] = {3, 2, 0, 4, 1, 2, 4, 5, 0, 1, 6};
  TestPath(kArcs, 7, ABSL_ARRAYSIZE(kArcs), true, kExpectedPath);
}

TEST(EulerianPathTest, Disconnected) {
  // Graph: 0===1  2===3. Would be Eulerian if connected.
  const int kArcs[][2] = {{0, 1}, {1, 0}, {2, 3}, {3, 2}};
  util::ReverseArcListGraph<int, int> graph(4, ABSL_ARRAYSIZE(kArcs));
  for (const auto& [from, to] : kArcs) {
    graph.AddArc(from, to);
  }
  std::vector<int> odd_nodes;

  // If we do *not* assume the connectivity, we detect that it's disconnected
  // and see that it's not Eulerian.
  EXPECT_FALSE(IsEulerianGraph(graph, /*assume_connectivity=*/false));
  EXPECT_FALSE(
      IsSemiEulerianGraph(graph, &odd_nodes, /*assume_connectivity=*/false));
  EXPECT_THAT(BuildEulerianTour(graph, /*assume_connectivity=*/false),
              IsEmpty());
  EXPECT_THAT(BuildEulerianPath(graph, /*assume_connectivity=*/false),
              IsEmpty());

  // If we assume the connectivity, we do not detect that it's disconnected
  // and we think it's Eulerian.
  EXPECT_TRUE(IsEulerianGraph(graph, /*assume_connectivity=*/true));
  EXPECT_TRUE(
      IsSemiEulerianGraph(graph, &odd_nodes, /*assume_connectivity=*/true));
  EXPECT_THAT(BuildEulerianTour(graph, /*assume_connectivity=*/true),
              ElementsAre(0, 1, 0));
  EXPECT_THAT(BuildEulerianPath(graph, /*assume_connectivity=*/true),
              ElementsAre(0, 1, 0));

  // Test that the default is "assume connectivity".
  EXPECT_EQ(IsEulerianGraph(graph), IsEulerianGraph(graph, true));
  EXPECT_NE(IsEulerianGraph(graph), IsEulerianGraph(graph, false));
}

TEST(EulerianPathTest, EulerianPathWithSuccessfulConnectivityCheck) {
  // Graph entered as 0-->1<--2, but direction doesn't matter.
  const int kArcs[][2] = {{0, 1}, {1, 2}};
  util::ReverseArcListGraph<int, int> graph(3, ABSL_ARRAYSIZE(kArcs));
  for (const auto& [from, to] : kArcs) {
    graph.AddArc(from, to);
  }
  std::vector<int> odd_nodes;
  EXPECT_TRUE(
      IsSemiEulerianGraph(graph, &odd_nodes, /*assume_connectivity=*/false));
  EXPECT_THAT(BuildEulerianPath(graph, /*assume_connectivity=*/false),
              ElementsAre(0, 1, 2));
}

TEST(EulerianPathTest, EulerianTourWithSuccessfulConnectivityCheck) {
  // Graph: 0===1.
  const int kArcs[][2] = {{0, 1}, {1, 0}};
  util::ReverseArcListGraph<int, int> graph(2, ABSL_ARRAYSIZE(kArcs));
  for (const auto& [from, to] : kArcs) {
    graph.AddArc(from, to);
  }
  EXPECT_TRUE(IsEulerianGraph(graph, /*assume_connectivity=*/false));
  EXPECT_THAT(BuildEulerianTour(graph, /*assume_connectivity=*/false),
              ElementsAre(0, 1, 0));
}
template <typename GraphType>
static void BM_EulerianTourOnGrid(benchmark::State& state) {
  int size = state.range(0);
  const int num_nodes = size * size;
  const int num_edges = 2 * size * (size - 1) + 2 * size - 4;
  util::ReverseArcListGraph<int, int> graph(num_nodes, num_edges);
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      if (j < size - 1) {
        graph.AddArc(i * size + j, i * size + j + 1);
      }
      if (i < size - 1) {
        graph.AddArc(i * size + j, (i + 1) * size + j);
      }
    }
  }
  for (int i = 1; i < size - 1; ++i) {
    graph.AddArc(i * size, i * size + size - 1);
    graph.AddArc(i, (size - 1) * size + i);
  }
  for (auto _ : state) {
    ASSERT_EQ(num_edges + 1, BuildEulerianTour(graph).size());
  }
}

BENCHMARK_TEMPLATE(BM_EulerianTourOnGrid, util::ReverseArcStaticGraph<>)
    ->Range(2, 1 << 10);

}  // namespace
}  // namespace operations_research
