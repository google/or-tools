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

#include "ortools/graph/strongly_connected_components.h"

#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/graph/generic_graph.h"
#include "ortools/graph/graph.h"

namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

// This is slow O(num_nodes^3), but checks that the result is correct using a
// different algorithm.
void ExpectCorrectResult(absl::Span<const std::vector<int>> graph,
                         absl::Span<const std::vector<int>> scc) {
  int num_nodes = graph.size();
  std::vector<std::vector<bool>> is_reachable(
      num_nodes, std::vector<bool>(num_nodes, false));
  for (int i = 0; i < graph.size(); ++i) {
    is_reachable[i][i] = true;
    for (int j : graph[i]) {
      is_reachable[i][j] = true;
    }
  }

  // Floyd–Warshall algorithm.
  for (int k = 0; k < num_nodes; ++k) {
    for (int i = 0; i < num_nodes; ++i) {
      for (int j = 0; j < num_nodes; ++j) {
        if (!is_reachable[i][j]) {
          is_reachable[i][j] = is_reachable[i][k] && is_reachable[k][j];
        }
      }
    }
  }

  // Full checking of the solution.
  int num_seen = 0;
  std::vector<bool> is_seen(num_nodes, false);
  std::vector<bool> is_marked(num_nodes, false);
  for (int component = 0; component < scc.size(); ++component) {
    for (int node : scc[component]) {
      ASSERT_GE(node, 0);
      ASSERT_LT(node, num_nodes);
      EXPECT_FALSE(is_seen[node]);
      ++num_seen;
      is_seen[node] = true;

      // Verify that all other nodes in the component are reachable.
      // And mark them.
      for (int other : scc[component]) {
        EXPECT_TRUE(is_reachable[node][other]);
        EXPECT_TRUE(is_reachable[other][node]);
        is_marked[other] = true;
      }

      // Check that all the arcs go to already marked nodes. This tests the
      // topological order. It also implies that no two components are strongly
      // connected with each other.
      for (int head : graph[node]) {
        EXPECT_TRUE(is_marked[head]);
      }
    }
  }
  EXPECT_EQ(num_seen, num_nodes);
}

// Testing on a bunch of disconnected nodes, each node is an SCC.
TEST(FindStronglyConnectedComponentsTest, EmptyGraph) {
  std::vector<std::vector<int>> graph;
  std::vector<std::vector<int>> components;
  FindStronglyConnectedComponents(static_cast<int>(0), graph, &components);
  EXPECT_TRUE(components.empty());
  ExpectCorrectResult(graph, components);
}

// Testing on a bunch of disconnected nodes (no arcs), each node is an SCC.
TEST(FindStronglyConnectedComponentsTest, AllDisconnectedNoArcs) {
  const int kNodes = 10;
  std::vector<std::vector<int>> graph(kNodes);
  std::vector<std::vector<int>> components;
  FindStronglyConnectedComponents(kNodes, graph, &components);
  EXPECT_EQ(kNodes, components.size());
  for (int i = 0; i < kNodes; ++i) {
    EXPECT_THAT(components[i], ElementsAre(i));
  }
  ExpectCorrectResult(graph, components);
}

// Testing on a connected but not strongly connected graph:
// 0 --> 1 --> ... --> n
TEST(FindStronglyConnectedComponentsTest, SimplePath) {
  const int kNodes = 10;
  std::vector<std::vector<int>> graph(kNodes);
  for (int i = 0; i + 1 < kNodes; ++i) {
    graph[i].push_back(i + 1);
  }
  std::vector<std::vector<int>> components;
  FindStronglyConnectedComponents(kNodes, graph, &components);
  EXPECT_EQ(kNodes, components.size());
  for (int i = 0; i < kNodes; ++i) {
    EXPECT_THAT(components[i], ElementsAre(kNodes - i - 1));
  }
  ExpectCorrectResult(graph, components);
}

// Testing on a cycle: 0 --> 1 --> ... --> n --> 0
TEST(FindStronglyConnectedComponentsTest, Cycle) {
  const int kNodes = 5;
  std::vector<std::vector<int>> graph(kNodes);
  for (int i = 0; i < kNodes; ++i) {
    graph[i].push_back((i + 1) % kNodes);
  }
  std::vector<std::vector<int>> components;
  FindStronglyConnectedComponents(kNodes, graph, &components);
  EXPECT_EQ(1, components.size());
  EXPECT_THAT(components[0], ElementsAre(0, 1, 2, 3, 4));
  ExpectCorrectResult(graph, components);
}

// Testing on a simple permuted cycle.
TEST(FindStronglyConnectedComponentsTest, PermutedCycles) {
  const int kNodes = 61;
  std::vector<std::vector<int>> graph(kNodes);

  // The (2^j % kNodes) generate all the non-zero nodes when kNodes is 61.
  // Here for an element i = 2^j, we set its successor to 2^{j+1}.
  for (int i = 0; i < kNodes; ++i) {
    graph[i].push_back((i * 2) % kNodes);
  }

  // We have 2 components, {0} and all the other numbers.
  std::vector<std::vector<int>> components;
  FindStronglyConnectedComponents(kNodes, graph, &components);
  EXPECT_EQ(2, components.size());
  EXPECT_THAT(components[0], ElementsAre(0));
  EXPECT_EQ(kNodes - 1, components[1].size());
  ExpectCorrectResult(graph, components);
}

// Testing on a star graph: 0 -> {1, 2, 3, 4}.
TEST(FindStronglyConnectedComponentsTest, StarGraph) {
  const int kNodes = 5;
  std::vector<std::vector<int>> graph(kNodes);
  for (int i = 1; i < kNodes; ++i) {
    graph[0].push_back(i);
  }
  std::vector<std::vector<int>> components;
  FindStronglyConnectedComponents(kNodes, graph, &components);
  EXPECT_EQ(5, components.size());
  ExpectCorrectResult(graph, components);
}

// Testing on a singleton cycle: 0 <--> 0
TEST(FindStronglyConnectedComponentsTest, SingletonCycle) {
  const int kNodes = 1;
  std::vector<std::vector<int>> graph(kNodes);
  graph[0].push_back(0);
  std::vector<std::vector<int>> components;
  FindStronglyConnectedComponents(kNodes, graph, &components);
  EXPECT_EQ(1, components.size());
  EXPECT_THAT(components[0], ElementsAre(0));
  ExpectCorrectResult(graph, components);
}

// Testing SCC (strongly connected component) on the following graph:
//   ---> 0 ---
//  |          |
//  |          v
//  2 <------- 1 -----> 3
//             |        ^
//             |        |
//             |        V
//              ------> 4
// Leads to 2 SCCs: (0, 1, 2), (3, 4).
TEST(FindStronglyConnectedComponentsTest, SmallCase) {
  const int kNodes = 5;
  std::vector<std::vector<int>> graph{{1}, {2, 3}, {0}, {4}, {3}};
  std::vector<std::vector<int>> components;
  FindStronglyConnectedComponents(kNodes, graph, &components);
  EXPECT_EQ(2, components.size());
  EXPECT_THAT(components[0], ElementsAre(3, 4));
  EXPECT_THAT(components[1], ElementsAre(0, 1, 2));
  ExpectCorrectResult(graph, components);
}

// This test exercise the "reusability" of the class.
TEST(FindStronglyConnectedComponentsTest, ExtensiveRandomTest) {
  const int kRandomSeed = 123;
  std::mt19937 randomizer(kRandomSeed);
  std::vector<std::vector<int>> graph;
  std::vector<std::vector<int>> scc;

  std::vector<int> graph_sizes;
  std::vector<int> scc_sizes;
  StronglyConnectedComponentsFinder<int, std::vector<std::vector<int>>,
                                    std::vector<std::vector<int>>>
      finder;
  for (int test = 0; test < 20; ++test) {
    const uint8_t num_nodes = absl::Uniform(randomizer, 0, 256);
    graph_sizes.push_back(num_nodes);
    graph.clear();
    graph.resize(num_nodes);
    for (int i = 0; i < num_nodes; ++i) {
      graph[i].resize(absl::Bernoulli(randomizer, 1.0 / 5)
                          ? absl::Uniform(randomizer, 0, 20)
                          : absl::Uniform(randomizer, 0, 4));
      for (int j = 0; j < graph[i].size(); ++j) {
        graph[i][j] = absl::Uniform<int>(randomizer, 0, num_nodes);
      }
    }

    scc.clear();
    finder.FindStronglyConnectedComponents(num_nodes, graph, &scc);
    scc_sizes.push_back(scc.size());
    ExpectCorrectResult(graph, scc);
  }

  // This show that we get a few different cases.
  EXPECT_THAT(graph_sizes,
              testing::ElementsAreArray({254, 208, 130, 219, 54,  212, 33,
                                         80,  215, 175, 113, 161, 120, 152,
                                         133, 214, 75,  53,  80,  51}));
  EXPECT_THAT(scc_sizes, testing::ElementsAreArray({88, 63, 49, 45, 21, 91, 12,
                                                    22, 81, 53, 37, 54, 28, 69,
                                                    53, 80, 30, 12, 49, 23}));
}

TEST(FindStronglyConnectedComponentsTest, DifferentIndexType) {
  const int kNodes = (1 << 8) - 1;
  const int kArcs = 3 * kNodes;
  const int kRandomSeed = 0;

  // Use a random, but deterministic (and fixed) graph for this test.
  const int kNumExpectedComponents = 40;
  std::mt19937 randomizer(kRandomSeed);
  std::vector<std::vector<int>> graph(kNodes);
  for (int i = 0; i < kArcs; ++i) {
    graph[absl::Uniform(randomizer, 0, kNodes)].push_back(
        absl::Uniform(randomizer, 0, kNodes));
  }

  std::vector<std::vector<int>> scc;
  FindStronglyConnectedComponents(static_cast<int32_t>(kNodes), graph, &scc);
  EXPECT_EQ(kNumExpectedComponents, scc.size());

  scc.clear();
  FindStronglyConnectedComponents(static_cast<uint16_t>(kNodes), graph, &scc);
  EXPECT_EQ(kNumExpectedComponents, scc.size());

  scc.clear();
  FindStronglyConnectedComponents(static_cast<uint8_t>(kNodes), graph, &scc);
  EXPECT_EQ(kNumExpectedComponents, scc.size());
  ExpectCorrectResult(graph, scc);
}

// This special graph is designed such that in the algorithm, the last node get
// an index of kSettledNode even though it is not settled! However, since it
// has to be the last node for this to happen, the algorithm still works.
TEST(FindStronglyConnectedComponentsTest, LimitCase) {
  // We need num_nodes == std::numeric_limit<NodeIndex>::max() and that there is
  // one big connected component except the node (num_nodes - 1) which is on its
  // own.
  const int kNodes = (1 << 8) - 1;
  std::vector<std::vector<int>> graph(kNodes);
  for (int i = 0; i < kNodes - 1; ++i) {
    graph[i].push_back((i + 1) % (kNodes - 1));
  }
  // We also need that the last node of the big connected components point to
  // the last node.
  graph[kNodes - 2].push_back(kNodes - 1);

  SccCounterOutput<uint16_t> count1;
  FindStronglyConnectedComponents(static_cast<uint16_t>(kNodes), graph,
                                  &count1);
  EXPECT_EQ(2, count1.size());
  SccCounterOutput<uint8_t> count2;
  FindStronglyConnectedComponents(static_cast<uint8_t>(kNodes), graph, &count2);
  EXPECT_EQ(2, count2.size());
}

// An ordered cycle of maximum size is also another limit case. Here it works
// since the last node will not be the root of the connected component.
TEST(FindStronglyConnectedComponentsTest, CycleLimitCase) {
  const int kNodes = (1 << 8) - 1;
  std::vector<std::vector<int>> graph(kNodes);
  for (int i = 0; i < kNodes; ++i) {
    graph[i].push_back((i + 1) % kNodes);
  }
  SccCounterOutput<uint16_t> count1;
  FindStronglyConnectedComponents(static_cast<uint16_t>(kNodes), graph,
                                  &count1);
  EXPECT_EQ(1, count1.size());
  SccCounterOutput<uint8_t> count2;
  FindStronglyConnectedComponents(static_cast<uint8_t>(kNodes), graph, &count2);
  EXPECT_EQ(1, count2.size());
}

TEST(GetStronglyConnectedComponentsTest, SmallGraph) {
  // A-->B-->C-->D-->E<->F
  //     ^       |
  //     |       |
  //      `-----'
  EXPECT_THAT(
      GetStronglyConnectedComponents(
          util::graph::GenericGraph<std::string>::FromEdges({{"A", "B"},
                                                             {"B", "C"},
                                                             {"C", "D"},
                                                             {"D", "E"},
                                                             {"E", "F"},
                                                             {"D", "B"},
                                                             {"F", "E"}})),
      // SCCs are in reverse topological order.
      ElementsAre(UnorderedElementsAre("E", "F"),
                  UnorderedElementsAre("B", "C", "D"),
                  UnorderedElementsAre("A")));
}

TEST(FindStronglyConnectedComponentsIdsTest, SmallGraph) {
  // 0-->1-->2-->3-->4<->5
  //     ^       |
  //     |       |
  //      `-----'
  EXPECT_THAT(FindStronglyConnectedComponentsIds(std::vector<std::vector<int>>{
                  {1}, {2}, {3}, {1, 4}, {5}, {4}}),
              ElementsAre(2, 1, 1, 1, 0, 0));
}

TEST(FindStronglyConnectedComponentsIdsTest, OnStaticGraph) {
  util::StaticGraph<int>::Builder builder;
  builder.AddArc(0, 1);
  builder.AddArc(1, 2);
  builder.AddArc(2, 1);
  const auto graph = std::move(builder).Build(nullptr);
  EXPECT_THAT(FindStronglyConnectedComponentsIds(*graph), ElementsAre(1, 0, 0));
}

}  // namespace
