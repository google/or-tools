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

#include "ortools/graph/k_shortest_paths.h"

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
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/io.h"
#include "ortools/graph/shortest_paths.h"

namespace operations_research {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAreArray;
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

  const KShortestPaths paths = YenKShortestPaths(graph, lengths, /*source=*/0,
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

  const KShortestPaths paths = YenKShortestPaths(graph, lengths, /*source=*/0,
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

  const KShortestPaths paths = YenKShortestPaths(graph, lengths, /*source=*/0,
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

  const KShortestPaths paths = YenKShortestPaths(graph, lengths, /*source=*/0,
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

  const KShortestPaths paths = YenKShortestPaths(graph, lengths, /*source=*/0,
                                                 /*destination=*/2, /*k=*/2);
  EXPECT_THAT(paths.paths,
              ElementsAre(std::vector<int>{0, 2}, std::vector<int>{0, 1, 2}));
  EXPECT_THAT(paths.distances, ElementsAre(1, 2));
}

TEST(KShortestPathsYenTest, RandomTest) {
  std::mt19937 random(12345);
  const int kNumGraphs = 10;
  const int kNumQueriesPerGraph = 10;
  const int kNumNodes = 10;
  const int kNumArcs = 3 * kNumNodes;
  const int kMaxNumTrialsPerArc = 1'000;
  // TODO(user): when supported, also test negative weights.
  const int kMinLength = 0;
  const int kMaxLength = 1'000;

  const auto pick_one_node = [&random]() -> int {
    int node = absl::Uniform(random, 0, kNumNodes);
    CHECK_GE(node, 0);
    CHECK_LT(node, kNumNodes);
    return node;
  };
  const auto pick_two_nodes = [&pick_one_node]() -> std::pair<int, int> {
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

    // Build a random graph (and not multigraph).
    StaticGraph<> graph;
    std::vector<PathDistance> lengths;
    graph.AddNode(kNumNodes - 1);

    {
      std::set<std::pair<int, int>> arcs;
      for (int i = 0; i < kNumArcs; ++i) {
        // Pick an arc that is not yet known. Do up to some number of trials to
        // generate an arc before giving up.
        int src, dst;
        for (int count = 0; count < kMaxNumTrialsPerArc; ++count) {
          std::tie(src, dst) = pick_two_nodes();
          if (!arcs.contains({src, dst})) break;
          if (!arcs.contains({dst, src})) break;
        }

        // Double check that this arc is not yet known (or its reverse arc).
        if (arcs.contains({src, dst})) break;
        if (arcs.contains({dst, src})) break;

        arcs.insert({src, dst});
        graph.AddArc(src, dst);

        lengths.push_back(absl::Uniform(random, kMinLength, kMaxLength));
      }
    }

    std::vector<int> permutation;
    graph.Build(&permutation);
    util::Permute(permutation, &lengths);

    // Run random queries, with one source and one destination per query.
    for (int q = 0; q < kNumQueriesPerGraph; ++q) {
      int src, dst;
      std::tie(src, dst) = pick_two_nodes();

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

}  // namespace
}  // namespace operations_research
