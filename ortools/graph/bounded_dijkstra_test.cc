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
#include "ortools/graph/graph.h"
#include "ortools/graph/graph_io.h"
#include "ortools/graph/test_util.h"
#include "ortools/util/flat_matrix.h"

namespace operations_research {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAreArray;
using ::util::ListGraph;

TEST(BoundedDijkstraWrapperDeathTest, Accessors) {
  ListGraph<> graph;
  graph.AddArc(1, 3);
  std::vector<float> arc_lengths = {2.5};
  BoundedDijkstraWrapper<ListGraph<>, float> dijkstra(&graph, &arc_lengths);
  const std::is_same<float, decltype(dijkstra)::distance_type> same_type;
  ASSERT_TRUE(same_type.value);
  ASSERT_EQ(&dijkstra.graph(), &graph);
  ASSERT_EQ(dijkstra.GetArcLength(0), 2.5);
}

TEST(BoundedDijkstraWrapperDeathTest, WithArcLengthFunctor) {
  ListGraph<> graph;
  graph.AddArc(1, 3);
  BoundedDijkstraWrapper<ListGraph<>, float, std::function<float(int)>>
      dijkstra(&graph, [](int) { return 2.34; });
  ASSERT_FLOAT_EQ(dijkstra.GetArcLength(0), 2.34f);
}

TEST(BoundedDijkstraWrapperDeathTest, ConstructorPreconditions) {
  ListGraph<> graph;
  for (int i = 0; i < 50; ++i) graph.AddArc(i, i + 1);

  std::vector<int> arc_lengths(13, 0);
  typedef BoundedDijkstraWrapper<ListGraph<>, int> TestedClass;
  EXPECT_DEATH(new TestedClass(&graph, &arc_lengths), "13");

  arc_lengths.resize(50, 0);
  arc_lengths[20] = -132;
  EXPECT_DEATH(new TestedClass(&graph, &arc_lengths), "-132");
}

TEST(BoundedDijkstraWrapper, ArcPathToAndSourceOfShortestPathToNode) {
  ListGraph<> graph;
  std::vector<int> arc_lengths = {1, 2, 3, 4, 6, 5};
  graph.AddArc(0, 1);
  graph.AddArc(0, 1);
  graph.AddArc(1, 2);
  graph.AddArc(1, 2);
  graph.AddArc(2, 3);
  graph.AddArc(2, 3);

  BoundedDijkstraWrapper<ListGraph<>, int> dijkstra(&graph, &arc_lengths);
  const std::vector<int> reached = dijkstra.RunBoundedDijkstra(0, 10);
  EXPECT_THAT(reached, ElementsAre(0, 1, 2, 3));
  EXPECT_EQ(9, dijkstra.distances()[3]);
  EXPECT_THAT(dijkstra.ArcPathTo(3), ElementsAre(0, 2, 5));
  EXPECT_THAT(dijkstra.NodePathTo(3), ElementsAre(0, 1, 2, 3));
  EXPECT_EQ(0, dijkstra.SourceOfShortestPathToNode(3));
}

TEST(BoundedDijkstraWrapper, EmptyPath) {
  ListGraph<> graph;
  std::vector<int> arc_lengths = {1, 2};
  graph.AddArc(0, 1);
  graph.AddArc(2, 3);

  BoundedDijkstraWrapper<ListGraph<>, int> dijkstra(&graph, &arc_lengths);
  const std::vector<int> reached = dijkstra.RunBoundedDijkstra(0, 10);
  EXPECT_THAT(reached, ElementsAre(0, 1));

  EXPECT_EQ(0, dijkstra.distances()[0]);
  EXPECT_THAT(dijkstra.ArcPathTo(0), ElementsAre());
  EXPECT_THAT(dijkstra.NodePathTo(0), ElementsAre(0));
  EXPECT_EQ(0, dijkstra.SourceOfShortestPathToNode(0));
}

TEST(BoundedDijkstraWrapper, OverflowSafe) {
  ListGraph<> graph;
  const int64_t int_max = std::numeric_limits<int64_t>::max();
  std::vector<int64_t> arc_lengths = {int_max, int_max / 2, int_max / 2, 1};
  graph.AddArc(0, 1);
  graph.AddArc(0, 1);
  graph.AddArc(1, 2);
  graph.AddArc(2, 3);

  BoundedDijkstraWrapper<ListGraph<>, int64_t> dijkstra(&graph, &arc_lengths);
  const std::vector<int> reached = dijkstra.RunBoundedDijkstra(0, int_max);

  // This works because int_max is odd, i.e. 2 * (int_max / 2) = int_max - 1
  EXPECT_THAT(reached, ElementsAre(0, 1, 2));
  EXPECT_EQ(0, dijkstra.distances()[0]);
  EXPECT_EQ(int_max / 2, dijkstra.distances()[1]);
  EXPECT_EQ(int_max - 1, dijkstra.distances()[2]);
}

TEST(BoundedDijkstraWrapper,
     ArcPathToAndSourceOfShortestPathToNode_WithArcLengthFunction) {
  ListGraph<> graph;
  std::vector<int> arc_lengths = {1, 2, 3, 4, 6, 5};
  graph.AddArc(0, 1);
  graph.AddArc(0, 1);
  graph.AddArc(1, 2);
  graph.AddArc(1, 2);
  graph.AddArc(2, 3);
  graph.AddArc(2, 3);
  class MyArcLengthFunctor {
   public:
    explicit MyArcLengthFunctor(const std::vector<int>& arc_lengths)
        : arc_lengths_(arc_lengths) {}
    int operator()(int arc) const {
      return arc % 2 == 1 ? arc_lengths_[arc] : 100;
    }

   private:
    const std::vector<int>& arc_lengths_;
  };
  BoundedDijkstraWrapper<ListGraph<>, int, MyArcLengthFunctor> dijkstra(
      &graph, MyArcLengthFunctor(arc_lengths));

  const std::vector<int> reached = dijkstra.RunBoundedDijkstra(0, 20);
  EXPECT_THAT(reached, ElementsAre(0, 1, 2, 3));
  EXPECT_EQ(11, dijkstra.distances()[3]);
  EXPECT_THAT(dijkstra.ArcPathTo(3), ElementsAre(1, 3, 5));
  EXPECT_THAT(dijkstra.NodePathTo(3), ElementsAre(0, 1, 2, 3));
  EXPECT_EQ(0, dijkstra.SourceOfShortestPathToNode(3));
}

TEST(BoundedDijkstraWrapperTest, RandomDenseGraph) {
  std::mt19937 random(12345);
  const int num_nodes = 50;
  std::vector<std::vector<int>> lengths(num_nodes, std::vector<int>(num_nodes));

  ListGraph<> graph;
  std::vector<int> arc_lengths;
  for (int i = 0; i < num_nodes; ++i) {
    for (int j = 0; j < num_nodes; ++j) {
      lengths[i][j] = (i == j) ? 0 : absl::Uniform(random, 0, 1000);
      graph.AddArc(i, j);
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
    BoundedDijkstraWrapper<ListGraph<>, int> dijkstra(&graph, &arc_lengths);
    const std::vector<int> reached = dijkstra.RunBoundedDijkstra(source, limit);
    for (const int node : reached) {
      EXPECT_LT(dijkstra.distances()[node], limit);
      EXPECT_EQ(dijkstra.distances()[node], lengths[source][node]);

      // Check that we never have the same node twice in the paths.
      std::vector<int> path = {node};
      int parent = node;
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
        SimpleOneToOneShortestPath<int>(0, 3, tails, heads, lengths);
    EXPECT_EQ(distance, std::numeric_limits<int>::max());
    EXPECT_TRUE(path.empty());
  }

  {
    // from 0 to 2 work because 2 * big_length < int_max.
    const auto [distance, path] =
        SimpleOneToOneShortestPath<int>(0, 2, tails, heads, lengths);
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
  std::vector<int> arc_lengths;

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
      const auto [distance, path] =
          SimpleOneToOneShortestPath<int>(from, to, tails, heads, arc_lengths);
      EXPECT_EQ(distance, shortest_distance[from][to]);
      EXPECT_FALSE(path.empty());
      EXPECT_EQ(path.front(), from);
      EXPECT_EQ(path.back(), to);
    }

    // A limit of shortest_distance[from][to] + 1 works too.
    {
      const auto [distance, path] = SimpleOneToOneShortestPath<int>(
          from, to, tails, heads, arc_lengths, shortest_distance[from][to] + 1);
      EXPECT_EQ(distance, shortest_distance[from][to]);
      EXPECT_FALSE(path.empty());
      EXPECT_EQ(path.front(), from);
      EXPECT_EQ(path.back(), to);
    }

    // But a limit of shortest_distance[from][to] should fail.
    {
      const auto [distance, path] = SimpleOneToOneShortestPath<int>(
          from, to, tails, heads, arc_lengths, shortest_distance[from][to]);
      EXPECT_EQ(distance, shortest_distance[from][to]);
      EXPECT_TRUE(path.empty());
    }
  }
}

TEST(BoundedDijkstraWrapperTest, MultiRunsOverDynamicGraphAndLengths) {
  ListGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(0, 1);
  std::vector<int> arc_lengths = {4, 3};
  BoundedDijkstraWrapper<ListGraph<>, int> dijkstra(&graph, &arc_lengths);

  EXPECT_THAT(dijkstra.RunBoundedDijkstra(0, 5), ElementsAre(0, 1));
  EXPECT_EQ(0, dijkstra.SourceOfShortestPathToNode(1));
  EXPECT_THAT(dijkstra.ArcPathTo(1), ElementsAre(1));

  EXPECT_THAT(dijkstra.RunBoundedDijkstra(0, 2), ElementsAre(0));
  EXPECT_EQ(0, dijkstra.SourceOfShortestPathToNode(0));
  EXPECT_THAT(dijkstra.ArcPathTo(0), IsEmpty());

  EXPECT_THAT(dijkstra.RunBoundedDijkstra(1, 99), ElementsAre(1));
  EXPECT_EQ(1, dijkstra.SourceOfShortestPathToNode(1));
  EXPECT_THAT(dijkstra.ArcPathTo(1), IsEmpty());

  // Add some arcs and nodes...
  graph.AddArc(0, 2);
  arc_lengths.push_back(1);
  graph.AddArc(1, 2);
  arc_lengths.push_back(0);
  graph.AddArc(2, 1);
  arc_lengths.push_back(1);
  graph.AddArc(1, 3);
  arc_lengths.push_back(5);

  EXPECT_THAT(dijkstra.RunBoundedDijkstra(0, 10), ElementsAre(0, 2, 1, 3));
  EXPECT_EQ(0, dijkstra.SourceOfShortestPathToNode(3));
  EXPECT_THAT(dijkstra.ArcPathTo(3), ElementsAre(2, 4, 5));

  EXPECT_THAT(dijkstra.RunBoundedDijkstra(0, 6), ElementsAre(0, 2, 1));
  EXPECT_EQ(0, dijkstra.SourceOfShortestPathToNode(1));
  EXPECT_THAT(dijkstra.ArcPathTo(1), ElementsAre(2, 4));
}

TEST(BoundedDijkstraWrapperTest, MultipleSources) {
  // Use this graph. Source nodes have their initial distance in [ ].
  //
  // N1[0] --(2)--> N0[4] --(1)--> N2 --(5)--> N3 <--(4)-- N4[3] --(5)--> N5
  ListGraph<> graph;
  std::vector<int> arc_lengths;
  graph.AddArc(1, 0);
  arc_lengths.push_back(2);
  graph.AddArc(0, 2);
  arc_lengths.push_back(1);
  graph.AddArc(2, 3);
  arc_lengths.push_back(5);
  graph.AddArc(4, 3);
  arc_lengths.push_back(4);
  graph.AddArc(4, 5);
  arc_lengths.push_back(5);
  BoundedDijkstraWrapper<ListGraph<>, int> dijkstra(&graph, &arc_lengths);
  // The distance limit is exclusive, so we can't reach Node 5.
  ASSERT_THAT(dijkstra.RunBoundedDijkstraFromMultipleSources(
                  {{1, 0}, {0, 4}, {4, 3}}, 8),
              // The order is deterministic: node 4 comes before node 2, despite
              // having equal distance and higher index, because it's a source.
              ElementsAre(1, 0, 4, 2, 3));
  EXPECT_EQ(2, dijkstra.distances()[0]);
  EXPECT_EQ(1, dijkstra.SourceOfShortestPathToNode(0));
  EXPECT_THAT(dijkstra.ArcPathTo(0), ElementsAre(0));
  EXPECT_EQ(0, dijkstra.distances()[1]);
  EXPECT_EQ(1, dijkstra.SourceOfShortestPathToNode(1));
  EXPECT_THAT(dijkstra.ArcPathTo(1), IsEmpty());
  EXPECT_EQ(3, dijkstra.distances()[2]);
  EXPECT_EQ(1, dijkstra.SourceOfShortestPathToNode(2));
  EXPECT_THAT(dijkstra.ArcPathTo(2), ElementsAre(0, 1));
  EXPECT_EQ(7, dijkstra.distances()[3]);
  EXPECT_EQ(4, dijkstra.SourceOfShortestPathToNode(3));
  EXPECT_THAT(dijkstra.ArcPathTo(3), ElementsAre(3));
  EXPECT_EQ(3, dijkstra.distances()[4]);
  EXPECT_EQ(4, dijkstra.SourceOfShortestPathToNode(4));
  EXPECT_THAT(dijkstra.ArcPathTo(4), IsEmpty());
}

TEST(BoundedDijkstraWrapperTest, SourcesAtOrBeyondDistanceLimitAreNotReached) {
  ListGraph<> graph(/*num_nodes=*/5, /*arc_capacity=*/0);
  std::vector<int> arc_lengths;  // No arcs.
  BoundedDijkstraWrapper<ListGraph<>, int> dijkstra(&graph, &arc_lengths);
  EXPECT_THAT(dijkstra.RunBoundedDijkstraFromMultipleSources(
                  {{0, 10}, {1, 11}, {2, 12}, {3, 13}}, 12),
              ElementsAre(0, 1));
}

TEST(BoundedDijkstraWrapperTest, SourcesListedMultipleTimesKeepsMinDistance) {
  ListGraph<> graph(/*num_nodes=*/5, /*arc_capacity=*/1);
  graph.AddArc(1, 3);
  std::vector<int> arc_lengths = {20};
  BoundedDijkstraWrapper<ListGraph<>, int> dijkstra(&graph, &arc_lengths);
  EXPECT_THAT(dijkstra.RunBoundedDijkstraFromMultipleSources(
                  {{1, 12}, {1, 10}, {1, 14}}, 31),
              ElementsAre(1, 3));
  EXPECT_EQ(dijkstra.distances()[3], 30);
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
  ListGraph<> graph;
  std::vector<int> arc_lengths;
  graph.AddArc(0, 1);
  arc_lengths.push_back(3);
  graph.AddArc(2, 3);
  arc_lengths.push_back(3);
  graph.AddArc(1, 5);
  arc_lengths.push_back(1);
  graph.AddArc(3, 5);
  arc_lengths.push_back(0);
  graph.AddArc(5, 3);
  arc_lengths.push_back(0);
  graph.AddArc(5, 4);
  arc_lengths.push_back(1);
  BoundedDijkstraWrapper<ListGraph<>, int> dijkstra(&graph, &arc_lengths);

  // Repeat the same source and destination multiple times, to verify that
  // it's supported.
  std::vector<std::pair<int, int>> sources = {{0, 5}, {2, 4}, {0, 2}, {0, 9}};
  std::vector<std::pair<int, int>> destinations = {
      {1, 7}, {4, 5}, {3, 3}, {4, 1}, {4, 3}};
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, destinations, /*num_destinations_to_reach=*/1,
          /*distance_limit=*/1000),
      Contains(4));
  EXPECT_EQ(2 + 3 + 1 + 1, dijkstra.distances()[4]);
  EXPECT_EQ(0, dijkstra.SourceOfShortestPathToNode(4));
  EXPECT_THAT(dijkstra.ArcPathTo(4),
              ElementsAre(/*0->1*/ 0, /*1->5*/ 2, /*5->4*/ 5));
  EXPECT_EQ(2, dijkstra.GetSourceIndex(0));
  EXPECT_EQ(3, dijkstra.GetDestinationIndex(4));

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
      ElementsAre(4));

  // Slightly modify the graph and try again. We want a case where the best
  // destination isn't the one with the smallest distance offset.
  destinations.push_back({1, 2});  // D1 will be the closest destination now.
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, destinations, /*num_destinations_to_reach=*/1,
          /*distance_limit=*/8),  // Limit is exclusive.
      ElementsAre(1));
  EXPECT_EQ(0, dijkstra.SourceOfShortestPathToNode(1));
  EXPECT_THAT(dijkstra.ArcPathTo(1), ElementsAre(/*0->1*/ 0));

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
  dijkstra.GetDestinationIndex(4);
  dijkstra.GetSourceIndex(1);

  // Setting num_reached_destinations=1 now should make '1' the only reachable
  // destination, even if the limit is infinite.
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, destinations, /*num_destinations_to_reach=*/1,
          /*distance_limit=*/1000),
      ElementsAre(1));

  // Verify that if we set the number of destinations to infinity, they're all
  // explored, and the search still stops before exploring the whole graph. To
  // do that, we add one extra arc that's beyond the farthest destination's
  // distance (including its destination offset), i.e. 1 (distance 2+3+7 = 12).
  graph.AddArc(5, 6);
  arc_lengths.push_back(2);
  graph.AddArc(6, 7);
  arc_lengths.push_back(0);
  EXPECT_THAT(
      dijkstra.RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
          sources, destinations, /*num_destinations_to_reach=*/1000,
          /*distance_limit=*/1000),
      ElementsAre(1, 4, 3));
  EXPECT_GE(dijkstra.distances()[1], 5);
  EXPECT_GE(dijkstra.distances()[4], 7);
  EXPECT_GE(dijkstra.distances()[3], 6);

  // To verify that node #7 isn't reached, we can check its distance, which will
  // still be set to the initialized "distance_limit - min_destination_offset".
  EXPECT_GE(dijkstra.distances()[7], 1000 - 1);
}

TEST(BoundedDijkstraWrapperTest, OneToOneShortestPath) {
  // Since we already tested the multiple sources - multiple destinations
  // variant, we only need to test the "plumbing" here.
  ListGraph<> graph;
  std::vector<int> arc_lengths;
  graph.AddArc(0, 1);
  arc_lengths.push_back(3);
  graph.AddArc(1, 2);
  arc_lengths.push_back(2);
  BoundedDijkstraWrapper<ListGraph<>, int> dijkstra(&graph, &arc_lengths);

  EXPECT_TRUE(dijkstra.OneToOneShortestPath(0, 2, 6));
  EXPECT_THAT(dijkstra.ArcPathTo(2), ElementsAre(0, 1));

  EXPECT_TRUE(dijkstra.OneToOneShortestPath(0, 0, 1));
  EXPECT_THAT(dijkstra.ArcPathTo(0), ElementsAre());

  EXPECT_TRUE(dijkstra.OneToOneShortestPath(1, 2, 3));
  EXPECT_THAT(dijkstra.ArcPathTo(2), ElementsAre(1));

  EXPECT_FALSE(dijkstra.OneToOneShortestPath(0, 2, 5));
  EXPECT_FALSE(dijkstra.OneToOneShortestPath(0, 0, 0));
  EXPECT_FALSE(dijkstra.OneToOneShortestPath(1, 2, 2));
  EXPECT_FALSE(dijkstra.OneToOneShortestPath(2, 1, 1000));
}

TEST(BoundedDijkstraWrapperTest, CustomSettledNodeCallback) {
  // A small chain: 8 --[3]--> 1 --[2]--> 42 --[3]--> 3 --[2]--> 4.
  ListGraph<> graph;
  std::vector<int> arc_lengths;
  graph.AddArc(8, 1);
  arc_lengths.push_back(3);
  graph.AddArc(1, 42);
  arc_lengths.push_back(2);
  graph.AddArc(42, 3);
  arc_lengths.push_back(3);
  graph.AddArc(3, 4);
  arc_lengths.push_back(2);
  typedef BoundedDijkstraWrapper<ListGraph<>, int> DijkstraType;
  DijkstraType dijkstra(&graph, &arc_lengths);

  // Tracks each NodeDistance it's called on, and sets the distance limit
  // to 10 if it gets called on node 42.
  std::vector<std::pair<int, int>> settled_node_dists;
  auto callback = [&settled_node_dists](int node, int distance,
                                        int* distance_limit) {
    settled_node_dists.push_back({node, distance});
    if (node == 42) *distance_limit = 10;
  };

  EXPECT_THAT(dijkstra.RunBoundedDijkstraWithSettledNodeCallback({{8, 0}},
                                                                 callback, 999),
              ElementsAre(8, 1, 42, 3));
  EXPECT_THAT(settled_node_dists,
              ElementsAre(Pair(8, 0), Pair(1, 3), Pair(42, 5), Pair(3, 8)));
}

TEST(BoundedDisjktraTest, RandomizedStressTest) {
  std::mt19937 random;
  const int kNumTests = 10'000;
  constexpr int kint32max = std::numeric_limits<int>::max();
  for (int test = 0; test < kNumTests; ++test) {
    // Generate a random graph with random weights.
    const int num_nodes = absl::Uniform(random, 1, 12);
    const int num_arcs =
        absl::Uniform(absl::IntervalClosed, random, 0,
                      std::min(num_nodes * (num_nodes - 1), 15));
    ListGraph<> graph(num_nodes, num_arcs);
    for (int a = 0; a < num_arcs; ++a) {
      graph.AddArc(absl::Uniform(random, 0, num_nodes),
                   absl::Uniform(random, 0, num_nodes));
    }
    std::vector<int> lengths(num_arcs);
    for (int& w : lengths) w = absl::Uniform(random, 0, 5);

    // Run Floyd-Warshall as a 'reference' shortest path algorithm.
    FlatMatrix<int> ref_dist(num_nodes, num_nodes, kint32max);
    for (int a = 0; a < num_arcs; ++a) {
      int& d = ref_dist[graph.Tail(a)][graph.Head(a)];
      if (lengths[a] < d) d = lengths[a];
    }
    for (int node = 0; node < num_nodes; ++node) {
      ref_dist[node][node] = 0;
    }
    for (int k = 0; k < num_nodes; ++k) {
      for (int i = 0; i < num_nodes; ++i) {
        for (int j = 0; j < num_nodes; ++j) {
          const int64_t dist_through_k =
              static_cast<int64_t>(ref_dist[i][k]) + ref_dist[k][j];
          if (dist_through_k < ref_dist[i][j]) ref_dist[i][j] = dist_through_k;
        }
      }
    }

    // Compute the graph's largest distance below kint32max.
    int max_distance = 0;
    for (int i = 0; i < num_nodes; ++i) {
      for (int j = 0; j < num_nodes; ++j) {
        const int d = ref_dist[i][j];
        if (d != kint32max && d > max_distance) max_distance = d;
      }
    }

    // Now, run some Dijkstras and verify that they match. To balance out the
    // FW (Floyd-Warshall) which is O(N³), we run more than one Dijkstra per FW.
    BoundedDijkstraWrapper<ListGraph<>, int> dijkstra(&graph, &lengths);
    for (int num_dijkstra = 0; num_dijkstra < 20; ++num_dijkstra) {
      // Draw the distance limit.
      const int limit =
          absl::Bernoulli(random, 0.2)
              ? kint32max
              : absl::Uniform(absl::IntervalClosed, random, 0, max_distance);
      // Draw sources (*with* repetition) with initial distances.
      const int num_sources = absl::Uniform(random, 1, 5);
      std::vector<std::pair<int, int>> sources(num_sources);
      for (auto& [s, dist] : sources) {
        s = absl::Uniform(random, 0, num_nodes);
        dist = absl::Uniform(absl::IntervalClosed, random, 0, max_distance + 1);
      }
      // Precompute the reference minimum distance to each node (using any of
      // the sources), and the expected reached nodes: any node whose distance
      // is < limit. That includes the sources: if a source's initial distance
      // is ≥ limit, it won't be reached.That includes the source themselves.
      std::vector<int> node_min_dist(num_nodes, kint32max);
      std::vector<int> expected_reached_nodes;
      for (int node = 0; node < num_nodes; ++node) {
        int min_dist = kint32max;
        for (const auto& [src, dist] : sources) {
          // Cast to int64_t to avoid overflows.
          min_dist = std::min<int64_t>(
              min_dist, static_cast<int64_t>(ref_dist[src][node]) + dist);
        }
        node_min_dist[node] = min_dist;
        if (min_dist < limit) expected_reached_nodes.push_back(node);
      }

      const std::vector<int> reached_nodes =
          dijkstra.RunBoundedDijkstraFromMultipleSources(sources, limit);
      EXPECT_THAT(reached_nodes,
                  UnorderedElementsAreArray(expected_reached_nodes));
      for (const int node : reached_nodes) {
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
  std::vector<int64_t> arc_lengths(graph->num_arcs(), 0);
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
