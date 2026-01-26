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

#include "ortools/graph/bounded_dijkstra.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/graph_base/graph.h"
#include "ortools/graph_base/io.h"
#include "ortools/graph_base/test_util.h"
#include "ortools/util/flat_matrix.h"

namespace operations_research {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAreArray;
using ::util::ListGraph;

DEFINE_STRONG_INT_TYPE(NodeIndex, int32_t);
DEFINE_STRONG_INT_TYPE(ArcIndex, int64_t);

using TestGraph = ListGraph<NodeIndex, ArcIndex>;
template <typename DistanceType>
using DijkstraWrapper = BoundedDijkstraWrapper<TestGraph, DistanceType>;

TEST(BoundedDijkstraWrapperDeathTest, Accessors) {
  TestGraph graph;
  graph.AddArc(NodeIndex(1), NodeIndex(3));
  DijkstraWrapper<float>::ByArc<float> arc_lengths = {2.5};
  DijkstraWrapper<float> dijkstra(&graph, &arc_lengths);
  const std::is_same<float, decltype(dijkstra)::distance_type> same_type;
  ASSERT_TRUE(same_type.value);
  ASSERT_EQ(&dijkstra.graph(), &graph);
  ASSERT_EQ(dijkstra.GetArcLength(ArcIndex(0)), 2.5);
}

TEST(BoundedDijkstraWrapperDeathTest, WithArcLengthFunctor) {
  TestGraph graph;
  graph.AddArc(NodeIndex(1), NodeIndex(3));
  BoundedDijkstraWrapper<TestGraph, float, std::function<float(ArcIndex)>>
      dijkstra(&graph, [](ArcIndex) { return 2.34; });
  ASSERT_FLOAT_EQ(dijkstra.GetArcLength(ArcIndex(0)), 2.34f);
}

TEST(BoundedDijkstraWrapperDeathTest, ConstructorPreconditions) {
  TestGraph graph;
  for (int i = 0; i < 50; ++i) graph.AddArc(NodeIndex(i), NodeIndex(i + 1));

  typedef DijkstraWrapper<int> TestedClass;
  TestedClass::ByArc<int> arc_lengths(13, 0);
  EXPECT_DEATH(new TestedClass(&graph, &arc_lengths), "13");

  arc_lengths.resize(50, 0);
  arc_lengths[ArcIndex(20)] = -132;
  EXPECT_DEATH(new TestedClass(&graph, &arc_lengths), "-132");
}

TEST(BoundedDijkstraWrapper, ArcPathToAndSourceOfShortestPathToNode) {
  TestGraph graph;
  DijkstraWrapper<int>::ByArc<int> arc_lengths = {1, 2, 3, 4, 6, 5};
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  graph.AddArc(NodeIndex(1), NodeIndex(2));
  graph.AddArc(NodeIndex(1), NodeIndex(2));
  graph.AddArc(NodeIndex(2), NodeIndex(3));
  graph.AddArc(NodeIndex(2), NodeIndex(3));

  DijkstraWrapper<int> dijkstra(&graph, &arc_lengths);
  const auto reached = dijkstra.RunBoundedDijkstra(NodeIndex(0), 10);
  EXPECT_THAT(reached, ElementsAre(NodeIndex(0), NodeIndex(1), NodeIndex(2),
                                   NodeIndex(3)));
  EXPECT_EQ(9, dijkstra.distances()[NodeIndex(3)]);
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(3)),
              ElementsAre(ArcIndex(0), ArcIndex(2), ArcIndex(5)));
  EXPECT_THAT(
      dijkstra.NodePathTo(NodeIndex(3)),
      ElementsAre(NodeIndex(0), NodeIndex(1), NodeIndex(2), NodeIndex(3)));
  EXPECT_EQ(NodeIndex(0), dijkstra.SourceOfShortestPathToNode(NodeIndex(3)));
}

TEST(BoundedDijkstraWrapper, EmptyPath) {
  TestGraph graph;
  DijkstraWrapper<int>::ByArc<int> arc_lengths = {1, 2};
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  graph.AddArc(NodeIndex(2), NodeIndex(3));

  DijkstraWrapper<int> dijkstra(&graph, &arc_lengths);
  const auto reached = dijkstra.RunBoundedDijkstra(NodeIndex(0), 10);
  EXPECT_THAT(reached, ElementsAre(NodeIndex(0), NodeIndex(1)));

  EXPECT_EQ(0, dijkstra.distances()[NodeIndex(0)]);
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(0)), ElementsAre());
  EXPECT_THAT(dijkstra.NodePathTo(NodeIndex(0)), ElementsAre(NodeIndex(0)));
  EXPECT_EQ(NodeIndex(0), dijkstra.SourceOfShortestPathToNode(NodeIndex(0)));
}

TEST(BoundedDijkstraWrapper, OverflowSafe) {
  TestGraph graph;
  const int64_t int_max = std::numeric_limits<int64_t>::max();
  DijkstraWrapper<int64_t>::ByArc<int64_t> arc_lengths = {int_max, int_max / 2,
                                                          int_max / 2, 1};
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  graph.AddArc(NodeIndex(1), NodeIndex(2));
  graph.AddArc(NodeIndex(2), NodeIndex(3));

  BoundedDijkstraWrapper<TestGraph, int64_t> dijkstra(&graph, &arc_lengths);
  const auto reached = dijkstra.RunBoundedDijkstra(NodeIndex(0), int_max);

  // This works because int_max is odd, i.e. 2 * (int_max / 2) = int_max - 1
  EXPECT_THAT(reached, ElementsAre(NodeIndex(0), NodeIndex(1), NodeIndex(2)));
  EXPECT_EQ(0, dijkstra.distances()[NodeIndex(0)]);
  EXPECT_EQ(int_max / 2, dijkstra.distances()[NodeIndex(1)]);
  EXPECT_EQ(int_max - 1, dijkstra.distances()[NodeIndex(2)]);
}

TEST(BoundedDijkstraWrapper,
     ArcPathToAndSourceOfShortestPathToNode_WithArcLengthFunction) {
  TestGraph graph;
  DijkstraWrapper<int>::ByArc<int> arc_lengths = {1, 2, 3, 4, 6, 5};
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  graph.AddArc(NodeIndex(1), NodeIndex(2));
  graph.AddArc(NodeIndex(1), NodeIndex(2));
  graph.AddArc(NodeIndex(2), NodeIndex(3));
  graph.AddArc(NodeIndex(2), NodeIndex(3));
  class MyArcLengthFunctor {
   public:
    explicit MyArcLengthFunctor(
        const DijkstraWrapper<int>::ByArc<int>& arc_lengths)
        : arc_lengths_(arc_lengths) {}

    int operator()(ArcIndex arc) const {
      return arc.value() % 2 == 1 ? arc_lengths_[arc] : 100;
    }

   private:
    const DijkstraWrapper<int>::ByArc<int>& arc_lengths_;
  };
  BoundedDijkstraWrapper<TestGraph, int, MyArcLengthFunctor> dijkstra(
      &graph, MyArcLengthFunctor(arc_lengths));

  const auto reached = dijkstra.RunBoundedDijkstra(NodeIndex(0), 20);
  EXPECT_THAT(reached, ElementsAre(NodeIndex(0), NodeIndex(1), NodeIndex(2),
                                   NodeIndex(3)));
  EXPECT_EQ(11, dijkstra.distances()[NodeIndex(3)]);
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(3)),
              ElementsAre(ArcIndex(1), ArcIndex(3), ArcIndex(5)));
  EXPECT_THAT(
      dijkstra.NodePathTo(NodeIndex(3)),
      ElementsAre(NodeIndex(0), NodeIndex(1), NodeIndex(2), NodeIndex(3)));
  EXPECT_EQ(NodeIndex(0), dijkstra.SourceOfShortestPathToNode(NodeIndex(3)));
}

TEST(BoundedDijkstraWrapperTest, RandomDenseGraph) {
  std::mt19937 random(12345);
  const int num_nodes = 50;
  std::vector<std::vector<int>> lengths(num_nodes, std::vector<int>(num_nodes));

  TestGraph graph;
  DijkstraWrapper<int>::ByArc<int> arc_lengths;
  for (int i = 0; i < num_nodes; ++i) {
    for (int j = 0; j < num_nodes; ++j) {
      lengths[i][j] = (i == j) ? 0 : absl::Uniform(random, 0, 1000);
      graph.AddArc(NodeIndex(i), NodeIndex(j));
      arc_lengths.push_back(lengths[i][j]);
    }
  }

  // Compute the shortest-path length using Floyd–Warshall algorithm.
  for (int k = 0; k < num_nodes; ++k) {
    for (int i = 0; i < num_nodes; ++i) {
      for (int j = 0; j < num_nodes; ++j) {
        lengths[i][j] = std::min(lengths[i][j], lengths[i][k] + lengths[k][j]);
      }
    }
  }

  // Test the bounded dijkstra code (from all sources).
  std::vector<int> reached_sizes;
  for (int source = 0; source < num_nodes; ++source) {
    const int limit = 100;
    DijkstraWrapper<int> dijkstra(&graph, &arc_lengths);
    const auto reached = dijkstra.RunBoundedDijkstra(NodeIndex(source), limit);
    for (const NodeIndex node : reached) {
      EXPECT_LT(dijkstra.distances()[node], limit);
      EXPECT_EQ(dijkstra.distances()[node], lengths[source][node.value()]);

      // Check that we never have the same node twice in the paths.
      std::vector<NodeIndex> path = {node};
      NodeIndex parent = node;
      while (dijkstra.parents()[parent] != parent) {
        parent = dijkstra.parents()[parent];
        path.push_back(parent);
      }
      std::sort(path.begin(), path.end());
      EXPECT_THAT(std::unique(path.begin(), path.end()), path.end());
    }

    int num_under_limit = 0;
    for (int i = 0; i < num_nodes; ++i) {
      if (lengths[source][i] < limit) ++num_under_limit;
    }
    EXPECT_EQ(num_under_limit, reached.size());
    reached_sizes.push_back(reached.size());
  }

  // With the given limit, we reach a good number of nodes.
  EXPECT_THAT(reached_sizes, Contains(15));
  EXPECT_THAT(reached_sizes, Contains(28));
  EXPECT_THAT(reached_sizes, Contains(41));
}

TEST(SimpleOneToOneShortestPathTest, PathTooLong) {
  const int big_length = std::numeric_limits<int>::max() / 2;
  std::vector<int> tails = {0, 1, 2};
  std::vector<int> heads = {1, 2, 3};
  std::vector<int> lengths = {big_length, big_length, big_length};

  {
    const auto [distance, path] =
        SimpleOneToOneShortestPath<int, int>(0, 3, tails, heads, lengths);
    EXPECT_EQ(distance, std::numeric_limits<int>::max());
    EXPECT_TRUE(path.empty());
  }

  {
    // from 0 to 2 work because 2 * big_length < int_max.
    const auto [distance, path] =
        SimpleOneToOneShortestPath<int, int>(0, 2, tails, heads, lengths);
    EXPECT_EQ(distance, std::numeric_limits<int>::max() - 1);
    EXPECT_THAT(path, ElementsAre(0, 1, 2));
  }
}

TEST(SimpleOneToOneShortestPathTest, Random) {
  absl::BitGen random;

  // We will generate a random dense graph.
  const int num_nodes = 50;
  std::vector<std::vector<int>> lengths(num_nodes, std::vector<int>(num_nodes));
  std::vector<std::vector<int>> shortest_distance(num_nodes,
                                                  std::vector<int>(num_nodes));

  // This will be the "sparse" representation.
  std::vector<int> tails;
  std::vector<int> heads;
  DijkstraWrapper<int>::ByArc<int> arc_lengths;

  // We permutes the arc order to properly test that it do not matter.
  std::vector<int> nodes(num_nodes);
  for (int i = 0; i < num_nodes; ++i) nodes[i] = i;
  std::shuffle(nodes.begin(), nodes.end(), random);

  // Generate random data.
  for (const int tail : nodes) {
    for (const int head : nodes) {
      lengths[tail][head] = (tail == head) ? 0 : absl::Uniform(random, 0, 1000);
      shortest_distance[tail][head] = lengths[tail][head];
      tails.push_back(tail);
      heads.push_back(head);
      arc_lengths.push_back(lengths[tail][head]);
    }
  }

  // Compute the shortest-path length using Floyd–Warshall algorithm.
  for (int k = 0; k < num_nodes; ++k) {
    for (int i = 0; i < num_nodes; ++i) {
      for (int j = 0; j < num_nodes; ++j) {
        shortest_distance[i][j] =
            std::min(shortest_distance[i][j],
                     shortest_distance[i][k] + shortest_distance[k][j]);
      }
    }
  }

  // Test the code from a bunch of random (from, to) pair.
  for (int runs = 0; runs < 100; ++runs) {
    const int from = absl::Uniform(random, 0, num_nodes);
    const int to = absl::Uniform(random, 0, num_nodes);

    // No limit. There should always be a path with our generated data.
    {
      const auto [distance, path] = SimpleOneToOneShortestPath<int, int>(
          from, to, tails, heads, arc_lengths);
      EXPECT_EQ(distance, shortest_distance[from][to]);
      EXPECT_FALSE(path.empty());
      EXPECT_EQ(path.front(), from);
      EXPECT_EQ(path.back(), to);
    }

    // A limit of shortest_distance[from][to] + 1 works too.
    {
      const auto [distance, path] = SimpleOneToOneShortestPath<int, int>(
          from, to, tails, heads, arc_lengths, shortest_distance[from][to] + 1);
      EXPECT_EQ(distance, shortest_distance[from][to]);
      EXPECT_FALSE(path.empty());
      EXPECT_EQ(path.front(), from);
      EXPECT_EQ(path.back(), to);
    }

    // But a limit of shortest_distance[from][to] should fail.
    {
      const auto [distance, path] = SimpleOneToOneShortestPath<int, int>(
          from, to, tails, heads, arc_lengths, shortest_distance[from][to]);
      EXPECT_EQ(distance, shortest_distance[from][to]);
      EXPECT_TRUE(path.empty());
    }
  }
}

TEST(BoundedDijkstraWrapperTest, MultiRunsOverDynamicGraphAndLengths) {
  TestGraph graph;
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  DijkstraWrapper<int>::ByArc<int> arc_lengths = {4, 3};
  DijkstraWrapper<int> dijkstra(&graph, &arc_lengths);

  EXPECT_THAT(dijkstra.RunBoundedDijkstra(NodeIndex(0), 5),
              ElementsAre(NodeIndex(0), NodeIndex(1)));
  EXPECT_EQ(NodeIndex(0), dijkstra.SourceOfShortestPathToNode(NodeIndex(1)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(1)), ElementsAre(ArcIndex(1)));

  EXPECT_THAT(dijkstra.RunBoundedDijkstra(NodeIndex(0), 2),
              ElementsAre(NodeIndex(0)));
  EXPECT_EQ(NodeIndex(0), dijkstra.SourceOfShortestPathToNode(NodeIndex(0)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(0)), IsEmpty());

  EXPECT_THAT(dijkstra.RunBoundedDijkstra(NodeIndex(1), 99),
              ElementsAre(NodeIndex(1)));
  EXPECT_EQ(NodeIndex(1), dijkstra.SourceOfShortestPathToNode(NodeIndex(1)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(1)), IsEmpty());

  // Add some arcs and nodes...
  graph.AddArc(NodeIndex(0), NodeIndex(2));
  arc_lengths.push_back(1);
  graph.AddArc(NodeIndex(1), NodeIndex(2));
  arc_lengths.push_back(0);
  graph.AddArc(NodeIndex(2), NodeIndex(1));
  arc_lengths.push_back(1);
  graph.AddArc(NodeIndex(1), NodeIndex(3));
  arc_lengths.push_back(5);

  EXPECT_THAT(
      dijkstra.RunBoundedDijkstra(NodeIndex(0), 10),
      ElementsAre(NodeIndex(0), NodeIndex(2), NodeIndex(1), NodeIndex(3)));
  EXPECT_EQ(NodeIndex(0), dijkstra.SourceOfShortestPathToNode(NodeIndex(3)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(3)),
              ElementsAre(ArcIndex(2), ArcIndex(4), ArcIndex(5)));

  EXPECT_THAT(dijkstra.RunBoundedDijkstra(NodeIndex(0), 6),
              ElementsAre(NodeIndex(0), NodeIndex(2), NodeIndex(1)));
  EXPECT_EQ(NodeIndex(0), dijkstra.SourceOfShortestPathToNode(NodeIndex(1)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(1)),
              ElementsAre(ArcIndex(2), ArcIndex(4)));
}

TEST(BoundedDijkstraWrapperTest, MultipleSources) {
  // Use this graph. Source nodes have their initial distance in [ ].
  //
  // N1[0] --(2)--> N0[4] --(1)--> N2 --(5)--> N3 <--(4)-- N4[3] --(5)--> N5
  TestGraph graph;
  DijkstraWrapper<int>::ByArc<int> arc_lengths;
  graph.AddArc(NodeIndex(1), NodeIndex(0));
  arc_lengths.push_back(2);
  graph.AddArc(NodeIndex(0), NodeIndex(2));
  arc_lengths.push_back(1);
  graph.AddArc(NodeIndex(2), NodeIndex(3));
  arc_lengths.push_back(5);
  graph.AddArc(NodeIndex(4), NodeIndex(3));
  arc_lengths.push_back(4);
  graph.AddArc(NodeIndex(4), NodeIndex(5));
  arc_lengths.push_back(5);
  DijkstraWrapper<int> dijkstra(&graph, &arc_lengths);
  // The distance limit is exclusive, so we can't reach Node 5.
  ASSERT_THAT(dijkstra.RunBoundedDijkstraFromMultipleSources(
                  {{NodeIndex(1), 0}, {NodeIndex(0), 4}, {NodeIndex(4), 3}}, 8),
              // The order is deterministic: node 4 comes before node 2, despite
              // having equal distance and higher index, because it's a source.
              ElementsAre(NodeIndex(1), NodeIndex(0), NodeIndex(4),
                          NodeIndex(2), NodeIndex(3)));
  EXPECT_EQ(2, dijkstra.distances()[NodeIndex(0)]);
  EXPECT_EQ(NodeIndex(1), dijkstra.SourceOfShortestPathToNode(NodeIndex(0)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(0)), ElementsAre(ArcIndex(0)));
  EXPECT_EQ(0, dijkstra.distances()[NodeIndex(1)]);
  EXPECT_EQ(NodeIndex(1), dijkstra.SourceOfShortestPathToNode(NodeIndex(1)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(1)), IsEmpty());
  EXPECT_EQ(3, dijkstra.distances()[NodeIndex(2)]);
  EXPECT_EQ(NodeIndex(1), dijkstra.SourceOfShortestPathToNode(NodeIndex(2)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(2)),
              ElementsAre(ArcIndex(0), ArcIndex(1)));
  EXPECT_EQ(7, dijkstra.distances()[NodeIndex(3)]);
  EXPECT_EQ(NodeIndex(4), dijkstra.SourceOfShortestPathToNode(NodeIndex(3)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(3)), ElementsAre(ArcIndex(3)));
  EXPECT_EQ(3, dijkstra.distances()[NodeIndex(4)]);
  EXPECT_EQ(NodeIndex(4), dijkstra.SourceOfShortestPathToNode(NodeIndex(4)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(4)), IsEmpty());
}

TEST(BoundedDijkstraWrapperTest, SourcesAtOrBeyondDistanceLimitAreNotReached) {
  TestGraph graph(/*num_nodes=*/NodeIndex(5), /*arc_capacity=*/ArcIndex(0));
  DijkstraWrapper<int>::ByArc<int> arc_lengths;  // No arcs.
  DijkstraWrapper<int> dijkstra(&graph, &arc_lengths);
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSources({{NodeIndex(0), 10},
                                                      {NodeIndex(1), 11},
                                                      {NodeIndex(2), 12},
                                                      {NodeIndex(3), 13}},
                                                     12),
      ElementsAre(NodeIndex(0), NodeIndex(1)));
}

TEST(BoundedDijkstraWrapperTest, SourcesListedMultipleTimesKeepsMinDistance) {
  TestGraph graph(/*num_nodes=*/NodeIndex(5), /*arc_capacity=*/ArcIndex(1));
  graph.AddArc(NodeIndex(1), NodeIndex(3));
  DijkstraWrapper<int>::ByArc<int> arc_lengths = {20};
  DijkstraWrapper<int> dijkstra(&graph, &arc_lengths);
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSources(
          {{NodeIndex(1), 12}, {NodeIndex(1), 10}, {NodeIndex(1), 14}}, 31),
      ElementsAre(NodeIndex(1), NodeIndex(3)));
  EXPECT_EQ(dijkstra.distances()[NodeIndex(3)], 30);
}

TEST(BoundedDijkstraWrapperTest, MultipleSourcesMultipleDestinations) {
  // Source nodes are "S", destination nodes are "D", and the rest are "N".
  // Source and destination nodes have their distance offset in [ ].
  //
  //  S0[2] --(3)--> D1[7] --(1)--.
  //                               >--> N5 --(1)--> D4[1]
  //  S2[4] --(3)--> D3[3] --(0)--'      |
  //                  ^                  |
  //                   \                /
  //                    `------(0)-----'
  //
  //  The shortest path is S0->D1->N5->D4, of distance 2 + 3 + 1 + 1 + 1 = 8.
  TestGraph graph;
  DijkstraWrapper<int>::ByArc<int> arc_lengths;
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  arc_lengths.push_back(3);
  graph.AddArc(NodeIndex(2), NodeIndex(3));
  arc_lengths.push_back(3);
  graph.AddArc(NodeIndex(1), NodeIndex(5));
  arc_lengths.push_back(1);
  graph.AddArc(NodeIndex(3), NodeIndex(5));
  arc_lengths.push_back(0);
  graph.AddArc(NodeIndex(5), NodeIndex(3));
  arc_lengths.push_back(0);
  graph.AddArc(NodeIndex(5), NodeIndex(4));
  arc_lengths.push_back(1);
  DijkstraWrapper<int> dijkstra(&graph, &arc_lengths);

  // Repeat the same source and destination multiple times, to verify that
  // it's supported.
  std::vector<std::pair<NodeIndex, int>> sources = {{NodeIndex(0), 5},
                                                    {NodeIndex(2), 4},
                                                    {NodeIndex(0), 2},
                                                    {NodeIndex(0), 9}};
  std::vector<std::pair<NodeIndex, int>> destinations = {{NodeIndex(1), 7},
                                                         {NodeIndex(4), 5},
                                                         {NodeIndex(3), 3},
                                                         {NodeIndex(4), 1},
                                                         {NodeIndex(4), 3}};
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, destinations, /*num_destinations_to_reach=*/1,
          /*distance_limit=*/1000),
      Contains(NodeIndex(4)));
  EXPECT_EQ(2 + 3 + 1 + 1, dijkstra.distances()[NodeIndex(4)]);
  EXPECT_EQ(NodeIndex(0), dijkstra.SourceOfShortestPathToNode(NodeIndex(4)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(4)),
              ElementsAre(/*0->1*/ ArcIndex(0), /*1->5*/ ArcIndex(2),
                          /*5->4*/ ArcIndex(5)));
  EXPECT_EQ(2, dijkstra.GetSourceIndex(NodeIndex(0)));
  EXPECT_EQ(3, dijkstra.GetDestinationIndex(NodeIndex(4)));

  // Run it with a limit too small: it'll fail to discover any destination.
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, destinations, /*num_destinations_to_reach=*/2,
          /*distance_limit=*/8),  // Limit is exclusive.
      IsEmpty());

  // .. And with a limit that's just big enough for "4". It'll be ok.
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, destinations, /*num_destinations_to_reach=*/2,
          /*distance_limit=*/9),  // Limit is exclusive.
      ElementsAre(NodeIndex(4)));

  // Slightly modify the graph and try again. We want a case where the best
  // destination isn't the one with the smallest distance offset.
  destinations.push_back(
      {NodeIndex(1), 2});  // D1 will be the closest destination now.
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, destinations, /*num_destinations_to_reach=*/1,
          /*distance_limit=*/8),  // Limit is exclusive.
      ElementsAre(NodeIndex(1)));
  EXPECT_EQ(NodeIndex(0), dijkstra.SourceOfShortestPathToNode(NodeIndex(1)));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(1)),
              ElementsAre(/*0->1*/ ArcIndex(0)));

  // Corner case: run with no destinations.
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, {}, /*num_destinations_to_reach=*/99,
          /*distance_limit=*/1000),
      IsEmpty());

  // Corner case: run with num_destinations_to_reach = 0.
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, destinations, /*num_destinations_to_reach=*/0,
          /*distance_limit=*/1000),
      IsEmpty());

  // Call Get{Source,Destination}Index() on nodes that aren't sources or
  // destinations. This returns junk; so we don't check the returned values,
  // but we do check that it doesn't crash.
  dijkstra.GetDestinationIndex(NodeIndex(4));
  dijkstra.GetSourceIndex(NodeIndex(1));

  // Setting num_reached_destinations=1 now should make '1' the only reachable
  // destination, even if the limit is infinite.
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, destinations, /*num_destinations_to_reach=*/1,
          /*distance_limit=*/1000),
      ElementsAre(NodeIndex(1)));

  // Verify that if we set the number of destinations to infinity, they're all
  // explored, and the search still stops before exploring the whole graph. To
  // do that, we add one extra arc that's beyond the farthest destination's
  // distance (including its destination offset), i.e. 1 (distance 2+3+7 = 12).
  graph.AddArc(NodeIndex(5), NodeIndex(6));
  arc_lengths.push_back(2);
  graph.AddArc(NodeIndex(6), NodeIndex(7));
  arc_lengths.push_back(0);
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, destinations, /*num_destinations_to_reach=*/1000,
          /*distance_limit=*/1000),
      ElementsAre(NodeIndex(1), NodeIndex(4), NodeIndex(3)));
  EXPECT_GE(dijkstra.distances()[NodeIndex(1)], 5);
  EXPECT_GE(dijkstra.distances()[NodeIndex(4)], 7);
  EXPECT_GE(dijkstra.distances()[NodeIndex(3)], 6);

  // To verify that node #7 isn't reached, we can check its distance, which will
  // still be set to the initialized "distance_limit - min_destination_offset".
  EXPECT_GE(dijkstra.distances()[NodeIndex(7)], 1000 - 1);
}

TEST(BoundedDijkstraWrapperTest, OneToOneShortestPath) {
  // Since we already tested the multiple sources - multiple destinations
  // variant, we only need to test the "plumbing" here.
  TestGraph graph;
  DijkstraWrapper<int>::ByArc<int> arc_lengths;
  graph.AddArc(NodeIndex(0), NodeIndex(1));
  arc_lengths.push_back(3);
  graph.AddArc(NodeIndex(1), NodeIndex(2));
  arc_lengths.push_back(2);
  DijkstraWrapper<int> dijkstra(&graph, &arc_lengths);

  EXPECT_TRUE(dijkstra.OneToOneShortestPath(NodeIndex(0), NodeIndex(2), 6));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(2)),
              ElementsAre(ArcIndex(0), ArcIndex(1)));

  EXPECT_TRUE(dijkstra.OneToOneShortestPath(NodeIndex(0), NodeIndex(0), 1));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(0)), ElementsAre());

  EXPECT_TRUE(dijkstra.OneToOneShortestPath(NodeIndex(1), NodeIndex(2), 3));
  EXPECT_THAT(dijkstra.ArcPathTo(NodeIndex(2)), ElementsAre(ArcIndex(1)));

  EXPECT_FALSE(dijkstra.OneToOneShortestPath(NodeIndex(0), NodeIndex(2), 5));
  EXPECT_FALSE(dijkstra.OneToOneShortestPath(NodeIndex(0), NodeIndex(0), 0));
  EXPECT_FALSE(dijkstra.OneToOneShortestPath(NodeIndex(1), NodeIndex(2), 2));
  EXPECT_FALSE(dijkstra.OneToOneShortestPath(NodeIndex(2), NodeIndex(0), 1000));
}

TEST(BoundedDijkstraWrapperTest, CustomSettledNodeCallback) {
  // A small chain: 8 --[3]--> 1 --[2]--> 42 --[3]--> 3 --[2]--> 4.
  TestGraph graph;
  DijkstraWrapper<int>::ByArc<int> arc_lengths;
  graph.AddArc(NodeIndex(8), NodeIndex(1));
  arc_lengths.push_back(3);
  graph.AddArc(NodeIndex(1), NodeIndex(42));
  arc_lengths.push_back(2);
  graph.AddArc(NodeIndex(42), NodeIndex(3));
  arc_lengths.push_back(3);
  graph.AddArc(NodeIndex(3), NodeIndex(4));
  arc_lengths.push_back(2);
  typedef DijkstraWrapper<int> DijkstraType;
  DijkstraType dijkstra(&graph, &arc_lengths);

  // Tracks each NodeDistance it's called on, and sets the distance limit
  // to 10 if it gets called on node 42.
  std::vector<std::pair<NodeIndex, int>> settled_node_dists;
  auto callback = [&settled_node_dists](NodeIndex node, int distance,
                                        int* distance_limit) {
    settled_node_dists.push_back({node, distance});
    if (node == NodeIndex(42)) *distance_limit = 10;
  };

  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraWithSettledNodeCallback({{NodeIndex(8), 0}},
                                                         callback, 999),
      ElementsAre(NodeIndex(8), NodeIndex(1), NodeIndex(42), NodeIndex(3)));
  EXPECT_THAT(settled_node_dists,
              ElementsAre(Pair(NodeIndex(8), 0), Pair(NodeIndex(1), 3),
                          Pair(NodeIndex(42), 5), Pair(NodeIndex(3), 8)));
}

TEST(BoundedDisjktraTest, RandomizedStressTest) {
  std::mt19937 random;
  const int kNumTests = 10'000;
  constexpr int kint32max = std::numeric_limits<int>::max();
  for (int test = 0; test < kNumTests; ++test) {
    // Generate a random graph with random weights.
    const NodeIndex num_nodes(absl::Uniform(random, 1, 12));
    const ArcIndex num_arcs(absl::Uniform(
        absl::IntervalClosed, random, 0,
        std::min(num_nodes.value() * (num_nodes.value() - 1), 15)));
    TestGraph graph(num_nodes, num_arcs);
    for (ArcIndex a(0); a < num_arcs; ++a) {
      graph.AddArc(NodeIndex(absl::Uniform(random, 0, num_nodes.value())),
                   NodeIndex(absl::Uniform(random, 0, num_nodes.value())));
    }
    DijkstraWrapper<int>::ByArc<int> lengths(num_arcs);
    for (int& w : lengths) w = absl::Uniform(random, 0, 5);

    // Run Floyd-Warshall as a 'reference' shortest path algorithm.
    FlatMatrix<int> ref_dist(num_nodes.value(), num_nodes.value(), kint32max);
    for (ArcIndex a(0); a < num_arcs; ++a) {
      int& d = ref_dist[graph.Tail(a).value()][graph.Head(a).value()];
      if (lengths[a] < d) d = lengths[a];
    }
    for (NodeIndex node(0); node < num_nodes; ++node) {
      ref_dist[node.value()][node.value()] = 0;
    }
    for (NodeIndex k(0); k < num_nodes; ++k) {
      for (NodeIndex i(0); i < num_nodes; ++i) {
        for (NodeIndex j(0); j < num_nodes; ++j) {
          const int64_t dist_through_k =
              static_cast<int64_t>(ref_dist[i.value()][k.value()]) +
              ref_dist[k.value()][j.value()];
          if (dist_through_k < ref_dist[i.value()][j.value()])
            ref_dist[i.value()][j.value()] = dist_through_k;
        }
      }
    }

    // Compute the graph's largest distance below kint32max.
    int max_distance = 0;
    for (NodeIndex i(0); i < num_nodes; ++i) {
      for (NodeIndex j(0); j < num_nodes; ++j) {
        const int d = ref_dist[i.value()][j.value()];
        if (d != kint32max && d > max_distance) max_distance = d;
      }
    }

    // Now, run some Dijkstras and verify that they match. To balance out the
    // FW (Floyd-Warshall) which is O(N³), we run more than one Dijkstra per FW.
    DijkstraWrapper<int> dijkstra(&graph, &lengths);
    for (int num_dijkstra = 0; num_dijkstra < 20; ++num_dijkstra) {
      // Draw the distance limit.
      const int limit =
          absl::Bernoulli(random, 0.2)
              ? kint32max
              : absl::Uniform(absl::IntervalClosed, random, 0, max_distance);
      // Draw sources (*with* repetition) with initial distances.
      const int num_sources = absl::Uniform(random, 1, 5);
      std::vector<std::pair<NodeIndex, int>> sources(num_sources);
      for (auto& [s, dist] : sources) {
        s = NodeIndex(absl::Uniform(random, 0, num_nodes.value()));
        dist = absl::Uniform(absl::IntervalClosed, random, 0, max_distance + 1);
      }
      // Precompute the reference minimum distance to each node (using any of
      // the sources), and the expected reached nodes: any node whose distance
      // is < limit. That includes the sources: if a source's initial distance
      // is ≥ limit, it won't be reached.That includes the source themselves.
      DijkstraWrapper<int>::ByNode<int> node_min_dist(num_nodes, kint32max);
      DijkstraWrapper<int>::ByNode<NodeIndex> expected_reached_nodes;
      for (NodeIndex node(0); node < num_nodes; ++node) {
        int min_dist = kint32max;
        for (const auto& [src, dist] : sources) {
          // Cast to int64_t to avoid overflows.
          min_dist = std::min<int64_t>(
              min_dist,
              static_cast<int64_t>(ref_dist[src.value()][node.value()]) + dist);
        }
        node_min_dist[node] = min_dist;
        if (min_dist < limit) expected_reached_nodes.push_back(node);
      }

      const auto reached_nodes =
          dijkstra.RunBoundedDijkstraFromMultipleSources(sources, limit);
      EXPECT_THAT(reached_nodes,
                  UnorderedElementsAreArray(expected_reached_nodes));
      for (const NodeIndex node : reached_nodes) {
        EXPECT_EQ(dijkstra.distances()[node], node_min_dist[node]) << node;
      }
      ASSERT_FALSE(HasFailure())
          << DUMP_VARS(num_nodes, num_arcs, num_sources, limit, lengths)
          << "\n With graph:\n"
          << util::GraphToString(graph, util::PRINT_GRAPH_ARCS);
    }
  }
}

template <bool arc_lengths_are_discrete>
void BM_GridGraph(benchmark::State& state) {
  typedef util::StaticGraph<int> Graph;
  const int64_t kWidth = 100;
  const int64_t kHeight = 10000;
  const int kSourceNode = static_cast<int>(kWidth * kHeight / 2);
  std::unique_ptr<Graph> graph =
      util::Create2DGridGraph<Graph>(/*width=*/kWidth, /*height=*/kHeight);
  BoundedDijkstraWrapper<Graph, int64_t>::ByArc<int64_t> arc_lengths(
      graph->num_arcs(), 0);
  const int64_t min_length = arc_lengths_are_discrete ? 0 : 1;
  const int64_t max_length = arc_lengths_are_discrete ? 2 : 1000000000000000L;
  std::mt19937 random(12345);
  for (int64_t& length : arc_lengths) {
    length = absl::Uniform(random, min_length, max_length + 1);
  }
  BoundedDijkstraWrapper<Graph, int64_t> dijkstra(graph.get(), &arc_lengths);
  const int64_t kSearchRadius = kWidth * (min_length + max_length) / 2;
  // NOTE(user): The expected number of nodes visited is in ϴ(kWidth²),
  // since the search radius is ϴ(kWidth). The exact constant is hard to
  // derive mathematically as a function of the radius formula, so I measured
  // it empirically and it was close to 0.5, which seemed about right.
  const int64_t kExpectedApproximateSearchSize = kWidth * kWidth / 2;
  int64_t total_nodes_visited = 0;
  for (auto _ : state) {
    const int num_nodes_visited =
        dijkstra
            .RunBoundedDijkstra(/*source_node=*/kSourceNode,
                                /*distance_limit=*/kSearchRadius)
            .size();
    // We verify that the Dijkstra search size is in the expected range, to
    // make sure that we're measuring what we want in this benchmark.
    CHECK_GE(num_nodes_visited, kExpectedApproximateSearchSize / 2);
    CHECK_GE(num_nodes_visited, kExpectedApproximateSearchSize * 2);
    total_nodes_visited += num_nodes_visited;
  }
  state.SetItemsProcessed(total_nodes_visited);
}

BENCHMARK(BM_GridGraph<true>);
BENCHMARK(BM_GridGraph<false>);

}  // namespace
}  // namespace operations_research
