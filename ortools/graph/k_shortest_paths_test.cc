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

#include "ortools/graph/k_shortest_paths.h"

#include <algorithm>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/graph_io.h"
#include "ortools/graph/shortest_paths.h"

namespace operations_research {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAreArray;
using util::Permute;
using util::StaticGraph;

TEST(KShortestPathsYenDeathTest, EmptyGraph) {
  StaticGraph<> graph;
  std::vector<PathDistance> lengths;

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/0,
                                 /*destination=*/1, /*k=*/10),
               "graph.num_nodes\\(\\) > 0");
}

TEST(KShortestPathsYenDeathTest, NoArcGraph) {
  StaticGraph<> graph;
  graph.AddNode(1);
  (void)graph.Build();
  std::vector<PathDistance> lengths;

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/0,
                                 /*destination=*/1, /*k=*/10),
               "graph.num_arcs\\(\\) > 0");
}

TEST(KShortestPathsYenDeathTest, NonExistingSourceBecauseNegative) {
  StaticGraph<> graph;
  graph.AddNode(1);
  graph.AddArc(0, 1);
  (void)graph.Build();
  std::vector<PathDistance> lengths{0};

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/-1,
                                 /*destination=*/1, /*k=*/10),
               "source >= 0");
}

TEST(KShortestPathsYenDeathTest, NonExistingSourceBecauseTooLarge) {
  StaticGraph<> graph;
  graph.AddNode(1);
  graph.AddArc(0, 1);
  (void)graph.Build();
  std::vector<PathDistance> lengths{0};

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/1'000,
                                 /*destination=*/1, /*k=*/10),
               "source < graph.num_nodes()");
}

TEST(KShortestPathsYenDeathTest, NonExistingDestinationBecauseNegative) {
  StaticGraph<> graph;
  graph.AddNode(1);
  graph.AddArc(0, 1);
  (void)graph.Build();
  std::vector<PathDistance> lengths{0};

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/0,
                                 /*destination=*/-1, /*k=*/10),
               "destination >= 0");
}

TEST(KShortestPathsYenDeathTest, NonExistingDestinationBecauseTooLarge) {
  StaticGraph<> graph;
  graph.AddNode(1);
  graph.AddArc(0, 1);
  (void)graph.Build();
  std::vector<PathDistance> lengths{0};

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/0,
                                 /*destination=*/1'000, /*k=*/10),
               "destination < graph.num_nodes()");
}

TEST(KShortestPathsYenDeathTest, KEqualsZero) {
  StaticGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(1, 2);
  (void)graph.Build();
  std::vector<PathDistance> lengths{1, 1};

  EXPECT_DEATH(YenKShortestPaths(graph, lengths, /*source=*/0,
                                 /*destination=*/2, /*k=*/0),
               "k != 0");
}

TEST(KShortestPathsYenTest, ReducesToShortestPath) {
  StaticGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(1, 2);
  (void)graph.Build();
  std::vector<PathDistance> lengths{1, 1};

  const KShortestPaths<StaticGraph<>> paths =
      YenKShortestPaths(graph, lengths, /*source=*/0,
                        /*destination=*/2, /*k=*/1);
  EXPECT_THAT(paths.paths, ElementsAre(std::vector<int>{0, 1, 2}));
  EXPECT_THAT(paths.distances, ElementsAre(2));
}

TEST(KShortestPathsYenTest, OnlyHasOnePath) {
  StaticGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(1, 2);
  (void)graph.Build();
  std::vector<PathDistance> lengths{1, 1};

  const KShortestPaths<StaticGraph<>> paths =
      YenKShortestPaths(graph, lengths, /*source=*/0,
                        /*destination=*/2, /*k=*/10);
  EXPECT_THAT(paths.paths, ElementsAre(std::vector<int>{0, 1, 2}));
  EXPECT_THAT(paths.distances, ElementsAre(2));
}

TEST(KShortestPathsYenTest, HasTwoPaths) {
  StaticGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(0, 2);
  graph.AddArc(1, 2);
  (void)graph.Build();
  std::vector<PathDistance> lengths{1, 30, 1};

  const KShortestPaths<StaticGraph<>> paths =
      YenKShortestPaths(graph, lengths, /*source=*/0,
                        /*destination=*/2, /*k=*/10);
  EXPECT_THAT(paths.paths,
              ElementsAre(std::vector<int>{0, 1, 2}, std::vector<int>{0, 2}));
  EXPECT_THAT(paths.distances, ElementsAre(2, 30));
}

TEST(KShortestPathsYenTest, HasTwoPathsWithLongerPath) {
  StaticGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(0, 4);
  graph.AddArc(1, 2);
  graph.AddArc(2, 3);
  graph.AddArc(3, 4);
  (void)graph.Build();
  std::vector<PathDistance> lengths{1, 30, 1, 1, 1};

  const KShortestPaths<StaticGraph<>> paths =
      YenKShortestPaths(graph, lengths, /*source=*/0,
                        /*destination=*/4, /*k=*/10);
  EXPECT_THAT(paths.paths, ElementsAre(std::vector<int>{0, 1, 2, 3, 4},
                                       std::vector<int>{0, 4}));
  EXPECT_THAT(paths.distances, ElementsAre(4, 30));
}

TEST(KShortestPathsYenTest, ReturnsTheRightNumberOfPaths) {
  StaticGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(0, 2);
  graph.AddArc(0, 3);
  graph.AddArc(1, 2);
  graph.AddArc(3, 2);

  (void)graph.Build();
  std::vector<PathDistance> lengths{1, 1, 1, 1, 1};

  const KShortestPaths<StaticGraph<>> paths =
      YenKShortestPaths(graph, lengths, /*source=*/0,
                        /*destination=*/2, /*k=*/2);
  EXPECT_THAT(paths.paths,
              ElementsAre(std::vector<int>{0, 2}, std::vector<int>{0, 1, 2}));
  EXPECT_THAT(paths.distances, ElementsAre(1, 2));
}

// This test verifies that the algorithm returns the shortest path from the
// candidate paths produced at each spur.
TEST(KShortestPathsYenTest, ShortestPathSelectedFromCandidates) {
  // Topology:
  //
  //    0 ---- 3 ---- 6     Arcs        length
  //    | \  / | \  / |     horizontal  100
  //    |  \/  |  \/  |     diagonal    100
  //    |  /\  |  /\  |     vertical    1000
  //    | /  \ | /  \ |
  //    1 ---- 2 ---- 7
  //    | \  / | \  / |
  //    |  \/  |  \/  |
  //    |  /\  |  /\  |
  //    | /  \ | /  \ |
  //    4 ---- 5 ---- 8
  StaticGraph<> graph;

  using TailHeadCost = std::tuple<int, int, int>;
  std::vector<TailHeadCost> arcs = {
      {0, 1, 1000}, {0, 2, 100},  {0, 3, 100},                 //
      {1, 0, 1000}, {1, 2, 100},  {1, 3, 100},  {1, 4, 1000},  //
      {2, 0, 100},  {2, 1, 100},  {2, 3, 1000}, {2, 4, 100},  {2, 5, 1000},
      {2, 6, 100},  {2, 7, 100},  {2, 8, 100},                               //
      {3, 0, 100},  {3, 1, 100},  {3, 2, 1000}, {3, 6, 100},  {3, 7, 100},   //
      {4, 1, 1000}, {4, 2, 100},  {4, 5, 100},                               //
      {5, 1, 100},  {5, 2, 1000}, {5, 4, 100},  {5, 7, 100},  {5, 8, 100},   //
      {6, 2, 100},  {6, 3, 100},  {6, 7, 1000},                              //
      {7, 2, 100},  {7, 3, 100},  {7, 5, 100},  {7, 6, 1000}, {7, 8, 1000},  //
      {8, 2, 100},  {8, 5, 100},  {8, 7, 1000},
  };
  std::vector<PathDistance> lengths;
  lengths.reserve(arcs.size());
  for (const auto& [tail, head, cost] : arcs) {
    graph.AddArc(tail, head);
    lengths.push_back(cost);
  }

  std::vector<int> permutation;
  graph.Build(&permutation);
  Permute(permutation, &lengths);

  const KShortestPaths<StaticGraph<>> paths =
      YenKShortestPaths(graph, lengths, /*source=*/0,
                        /*destination=*/6, /*k=*/14);

  EXPECT_THAT(paths.distances,
              ElementsAre(200, 200,            //
                          400, 400, 400, 400,  //
                          600, 600, 600, 600, 600, 600, 600, 600));

  EXPECT_THAT(
      paths.paths,
      testing::UnorderedElementsAre(
          // 200
          std::vector<int>{0, 2, 6}, std::vector<int>{0, 3, 6},
          // 400
          std::vector<int>{0, 2, 1, 3, 6}, std::vector<int>{0, 3, 1, 2, 6},
          std::vector<int>{0, 2, 7, 3, 6}, std::vector<int>{0, 3, 7, 2, 6},
          // 600
          std::vector<int>{0, 2, 7, 5, 1, 3, 6},
          std::vector<int>{0, 3, 7, 5, 1, 2, 6},
          std::vector<int>{0, 2, 4, 5, 1, 3, 6},
          std::vector<int>{0, 3, 7, 5, 4, 2, 6},
          std::vector<int>{0, 2, 4, 5, 7, 3, 6},
          std::vector<int>{0, 2, 8, 5, 1, 3, 6},
          std::vector<int>{0, 3, 7, 5, 8, 2, 6},
          std::vector<int>{0, 2, 8, 5, 7, 3, 6}));
}

namespace internal {

template <typename Graph, typename NodeIndexType, typename ArcIndexType,
          typename URBG, bool IsDirected>
Graph GenerateUniformGraph(URBG&& urbg, const NodeIndexType num_nodes,
                           const ArcIndexType num_edges) {
  // TODO(user): make these utility functions so they can be reused.
  const auto pick_one_node = [&urbg, num_nodes]() -> NodeIndexType {
    const NodeIndexType node = absl::Uniform(urbg, 0, num_nodes);
    CHECK_GE(node, 0);
    CHECK_LT(node, num_nodes);
    return node;
  };
  const auto pick_two_distinct_nodes =
      [&pick_one_node]() -> std::pair<NodeIndexType, NodeIndexType> {
    const NodeIndexType src = pick_one_node();
    NodeIndexType dst;
    do {
      dst = pick_one_node();
    } while (src == dst);
    CHECK_NE(src, dst);
    return {src, dst};
  };

  // Determine the maximum number of arcs in the graph.
  const ArcIndexType max_num_arcs = IsDirected
                                        ? (num_nodes * (num_nodes - 1))
                                        : (num_nodes * (num_nodes - 1)) / 2;

  // Build a random graph (and not multigraph) with `num_arcs` or `max_num_arcs`
  // arcs, whichever is lower. The set is useful to ensure the graph does not
  // contain the same arc more than once (the result would be a multigraph).
  // TODO(user): this is an awful way to generate a complete graph.
  StaticGraph<> graph;
  graph.AddNode(num_nodes - 1);

  std::set<std::pair<NodeIndexType, NodeIndexType>> arcs;
  for (ArcIndexType i = 0; i < std::min(num_edges, max_num_arcs); ++i) {
    NodeIndexType src, dst;
    std::tie(src, dst) = pick_two_distinct_nodes();
    if (arcs.find({src, dst}) != arcs.end()) continue;
    if (IsDirected && (arcs.find({dst, src}) != arcs.end())) continue;

    arcs.insert({src, dst});
    graph.AddArc(src, dst);

    if (IsDirected) {
      arcs.insert({dst, src});
      graph.AddArc(dst, src);
    }
  }

  // No need to keep the permutation when building, as there are no associated
  // attributes such as lengths in this function.
  graph.Build(nullptr);

  return graph;
}

}  // namespace internal

// Generates a random (un)directed graph with `num_nodes` nodes and up to
// `num_arcs` arcs / `num_edges` edges, following a uniform probability
// distribution. `urbg` is a source of randomness, such as an `std::mt19937`
// object.
//
// If the number of arcs that is requested is too large compared to the number
// of nodes (i.e. greater than the maximum number of arcs for a directed or
// undirected graph with the specified number of node), this function returns a
// complete graph.
template <typename NodeIndexType, typename ArcIndexType,
          typename Graph = StaticGraph<NodeIndexType, ArcIndexType>,
          typename URBG>
Graph GenerateUniformGraph(URBG&& urbg, const NodeIndexType num_nodes,
                           const ArcIndexType num_edges) {
  return internal::GenerateUniformGraph<Graph, NodeIndexType, ArcIndexType,
                                        URBG, /*IsDirected=*/false>(
      urbg, num_nodes, num_edges);
}
template <typename NodeIndexType, typename ArcIndexType,
          typename Graph = StaticGraph<NodeIndexType, ArcIndexType>,
          typename URBG>
Graph GenerateUniformDirectedGraph(URBG&& urbg, const NodeIndexType num_nodes,
                                   const ArcIndexType num_arcs) {
  return internal::GenerateUniformGraph<Graph, NodeIndexType, ArcIndexType,
                                        URBG, /*IsDirected=*/true>(
      urbg, num_nodes, num_arcs);
}

TEST(KShortestPathsYenTest, RandomTest) {
  std::mt19937 random(12345);
  constexpr int kNumGraphs = 10;
  constexpr int kNumQueriesPerGraph = 10;
  constexpr int kNumNodes = 10;
  constexpr int kNumArcs = 3 * kNumNodes;
  // TODO(user): when supported, also test negative weights.
  constexpr int kMinLength = 0;
  constexpr int kMaxLength = 1'000;

  const auto pick_one_node = [&random]() -> int {
    int node = absl::Uniform(random, 0, kNumNodes);
    CHECK_GE(node, 0);
    CHECK_LT(node, kNumNodes);
    return node;
  };
  const auto pick_two_distinct_nodes =
      [&pick_one_node]() -> std::pair<int, int> {
    int src = pick_one_node();
    int dst;
    do {
      dst = pick_one_node();
    } while (src == dst);
    CHECK_NE(src, dst);
    return {src, dst};
  };

  const auto format_path = [](std::string* out, const std::vector<int>& path) {
    absl::StrAppend(out, absl::StrJoin(path, " - "));
  };

  for (int graph_iter = 0; graph_iter < kNumGraphs; ++graph_iter) {
    (void)graph_iter;

    StaticGraph<> graph =
        GenerateUniformDirectedGraph(random, kNumNodes, kNumArcs);
    std::vector<PathDistance> lengths;
    for (int i = 0; i < graph.num_arcs(); ++i) {
      lengths.push_back(absl::Uniform(random, kMinLength, kMaxLength));
    }

    // Run random queries, with one source and one destination per query.
    for (int q = 0; q < kNumQueriesPerGraph; ++q) {
      int src, dst;
      std::tie(src, dst) = pick_two_distinct_nodes();

      // Determine the set of simple paths between these nodes by brute force.
      // (Simple in the sense that the path does not contain loops.)
      //
      // Basic idea: graph traversal from the source node until the destination
      // node, not stopping until the whole graph is searched.
      //
      // This loop always finishes, even if the two nodes are not connected:
      // at some point, there will be no tentative path left. In case of a loop
      // in the graph, the tested paths will not contain loops.
      std::set<std::vector<int>> brute_force_paths;
      std::vector<std::vector<int>> tentative_paths{{src}};
      while (!tentative_paths.empty()) {
        std::vector<int> partial_path = tentative_paths.front();
        tentative_paths.erase(tentative_paths.begin());

        const int last_node = partial_path.back();
        for (const int next_arc : graph.OutgoingArcs(last_node)) {
          const int next_node = graph.Head(next_arc);
          ASSERT_NE(last_node, next_node);

          if (absl::c_find(partial_path, next_node) != partial_path.end()) {
            // To avoid loops (both in the path and at run time), ensure that
            // the path does not go through `next_node`. Otherwise, there would
            // be a loop in path, going at least twice through `next_node`.
            continue;
          }

          std::vector<int> new_path = partial_path;
          new_path.push_back(next_node);

          if (next_node == dst) {
            brute_force_paths.emplace(std::move(new_path));
          } else {
            tentative_paths.emplace_back(std::move(new_path));
          }
        }
      }
      ASSERT_THAT(tentative_paths, IsEmpty());

      // Maybe the procedure fails to find paths because none exist, which is
      // possible with random graphs (i.e. the graph is disconnected, with `src`
      // and `dst` in distinct connected components).
      if (brute_force_paths.empty()) continue;

      // Use the algorithm-under-test to generate as many paths as possible.
      const KShortestPaths yen_paths =
          YenKShortestPaths(graph, lengths, src, dst,
                            /*k=*/brute_force_paths.size());

      // The two sets of paths must correspond.
      EXPECT_THAT(brute_force_paths, UnorderedElementsAreArray(yen_paths.paths))
          << "[" << util::GraphToString(graph, util::PRINT_GRAPH_ARCS)
          << "] Brute-force paths: ["
          << absl::StrJoin(brute_force_paths, ", ", format_path)
          << "] Yen paths: ["
          << absl::StrJoin(yen_paths.paths, ", ", format_path) << "]";
    }
  }
}

void BM_Yen(benchmark::State& state) {
  const int num_nodes = state.range(0);
  // Use half the maximum number of arcs, so that the graph is a bit sparse.
  const int num_arcs = num_nodes * (num_nodes - 1) / 4;
  // TODO(user): when supported, also benchmark negative weights
  // (separately?).
  constexpr int kMinLength = 0;
  constexpr int kMaxLength = 1'000;

  std::mt19937 random(12345);
  const auto pick_one_node = [&random, num_nodes]() -> int {
    int node = absl::Uniform(random, 0, num_nodes);
    CHECK_GE(node, 0);
    CHECK_LT(node, num_nodes);
    return node;
  };
  const auto pick_two_distinct_nodes =
      [&pick_one_node]() -> std::pair<int, int> {
    int src = pick_one_node();
    int dst;
    do {
      dst = pick_one_node();
    } while (src == dst);
    CHECK_NE(src, dst);
    return {src, dst};
  };

  StaticGraph<> graph =
      GenerateUniformDirectedGraph(random, num_nodes, num_arcs);
  std::vector<PathDistance> lengths;
  for (int i = 0; i < graph.num_arcs(); ++i) {
    lengths.push_back(absl::Uniform(random, kMinLength, kMaxLength));
  }

  for (auto unused : state) {
    int src, dst;
    std::tie(src, dst) = pick_two_distinct_nodes();
    YenKShortestPaths(graph, lengths, src, dst, /*k=*/10);
  }
}

BENCHMARK(BM_Yen)->Range(10, 1'000);

}  // namespace
}  // namespace operations_research
