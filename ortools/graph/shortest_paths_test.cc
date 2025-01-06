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

#include "ortools/graph/shortest_paths.h"

#include <algorithm>
#include <random>
#include <vector>

#include "absl/base/macros.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/graph/ebert_graph.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/strongly_connected_components.h"

namespace operations_research {

template <class GraphType>
void CheckPathDataPair(
    const GenericPathContainer<GraphType>& container,
    const GenericPathContainer<GraphType>& distance_container,
    PathDistance expected_distance, NodeIndex expected_predecessor,
    NodeIndex tail, NodeIndex head) {
  EXPECT_EQ(expected_distance, container.GetDistance(tail, head));
  EXPECT_EQ(expected_distance, distance_container.GetDistance(tail, head));
  EXPECT_EQ(expected_predecessor,
            container.GetPenultimateNodeInPath(tail, head));
  EXPECT_DEATH(distance_container.GetPenultimateNodeInPath(tail, head),
               "Path not stored.");
  // Checking path between tail and head.
  std::vector<NodeIndex> paths;
  container.GetPath(tail, head, &paths);
  if (tail == head) {
    EXPECT_GE(1, paths.size());
    if (!paths.empty()) {
      EXPECT_EQ(tail, paths.back());
    }
  } else if (!paths.empty()) {
    EXPECT_EQ(tail, paths[0]);
    NodeIndex current = head;
    for (int i = paths.size() - 1; i >= 0; --i) {
      EXPECT_EQ(current, paths[i]);
      current = container.GetPenultimateNodeInPath(tail, current);
    }
  }
  EXPECT_DEATH(distance_container.GetPath(tail, head, &paths),
               "Path not stored.");
}

template <class GraphType>
void CheckPathDataRow(const GraphType& graph,
                      const GenericPathContainer<GraphType>& container,
                      const GenericPathContainer<GraphType>& distance_container,
                      const NodeIndex expected_paths[],
                      const PathDistance expected_distances[], NodeIndex tail) {
  int index = tail * graph.num_nodes();
  for (typename GraphType::NodeIterator iterator(graph); iterator.Ok();
       iterator.Next()) {
    const NodeIndex head(iterator.Index());
    CheckPathDataPair(container, distance_container, expected_distances[index],
                      expected_paths[index], tail, head);
    ++index;
  }
}

template <class GraphType>
void CheckPathDataRowFromGraph(
    const GraphType& graph, const GenericPathContainer<GraphType>& container,
    const GenericPathContainer<GraphType>& distance_container,
    const NodeIndex expected_paths[], const PathDistance expected_distances[],
    NodeIndex tail) {
  int index = tail * graph.num_nodes();
  for (typename GraphType::NodeIndex head : graph.AllNodes()) {
    CheckPathDataPair(container, distance_container, expected_distances[index],
                      expected_paths[index], tail, head);
    ++index;
  }
}

template <class GraphType>
void CheckPathData(const GraphType& graph,
                   const GenericPathContainer<GraphType>& container,
                   const GenericPathContainer<GraphType>& distance_container,
                   const NodeIndex expected_paths[],
                   const PathDistance expected_distances[]) {
  for (typename GraphType::NodeIterator iterator(graph); iterator.Ok();
       iterator.Next()) {
    const NodeIndex tail(iterator.Index());
    CheckPathDataRow(graph, container, distance_container, expected_paths,
                     expected_distances, tail);
  }
}

template <class GraphType>
void CheckPathDataFromGraph(
    const GraphType& graph, const GenericPathContainer<GraphType>& container,
    const GenericPathContainer<GraphType>& distance_container,
    const NodeIndex expected_paths[], const PathDistance expected_distances[]) {
  for (typename GraphType::NodeIndex tail : graph.AllNodes()) {
    CheckPathDataRowFromGraph(graph, container, distance_container,
                              expected_paths, expected_distances, tail);
  }
}

#define BUILD_CONTAINERS()                                                  \
  auto container =                                                          \
      GenericPathContainer<GraphType>::BuildInMemoryCompactPathContainer(); \
  auto distance_container =                                                 \
      GenericPathContainer<GraphType>::BuildPathDistanceContainer()

template <class GraphType>
void TestShortestPathsFromGraph(const GraphType& graph,
                                const std::vector<PathDistance>& lengths,
                                const NodeIndex expected_paths[],
                                const PathDistance expected_distances[]) {
  const int kThreads = 32;
  const typename GraphType::NodeIndex source = 0;
  std::vector<typename GraphType::NodeIndex> some_nodes;
  int index = 0;
  std::mt19937 randomizer(12345);
  for (const typename GraphType::NodeIndex node : graph.AllNodes()) {
    if (absl::Bernoulli(randomizer, 1.0 / 2)) {
      some_nodes.push_back(node);
    }
    ++index;
  }
  // All-pair shortest paths.
  {
    BUILD_CONTAINERS();
    ComputeAllToAllShortestPathsWithMultipleThreads(graph, lengths, kThreads,
                                                    &container);
    ComputeAllToAllShortestPathsWithMultipleThreads(graph, lengths, kThreads,
                                                    &distance_container);
    CheckPathDataFromGraph(graph, container, distance_container, expected_paths,
                           expected_distances);
  }
  // One-to-all shortest paths.
  {
    BUILD_CONTAINERS();
    ComputeOneToAllShortestPaths(graph, lengths, source, &container);
    ComputeOneToAllShortestPaths(graph, lengths, source, &distance_container);
    CheckPathDataRowFromGraph(graph, container, distance_container,
                              expected_paths, expected_distances, source);
  }
  // Many-to-all shortest paths.
  {
    BUILD_CONTAINERS();
    ComputeManyToAllShortestPathsWithMultipleThreads(graph, lengths, some_nodes,
                                                     kThreads, &container);
    ComputeManyToAllShortestPathsWithMultipleThreads(
        graph, lengths, some_nodes, kThreads, &distance_container);
    for (int i = 0; i < some_nodes.size(); ++i) {
      CheckPathDataRowFromGraph(graph, container, distance_container,
                                expected_paths, expected_distances,
                                some_nodes[i]);
    }
  }
  // Many-to-all shortest paths with duplicates.
  {
    BUILD_CONTAINERS();
    std::vector<NodeIndex> sources(3, source);
    ComputeManyToAllShortestPathsWithMultipleThreads(graph, lengths, sources,
                                                     kThreads, &container);
    ComputeManyToAllShortestPathsWithMultipleThreads(
        graph, lengths, sources, kThreads, &distance_container);
    for (int i = 0; i < sources.size(); ++i) {
      CheckPathDataRowFromGraph(graph, container, distance_container,
                                expected_paths, expected_distances, sources[i]);
    }
  }
  // One-to-many shortest paths.
  {
    BUILD_CONTAINERS();
    ComputeOneToManyShortestPaths(graph, lengths, source, some_nodes,
                                  &container);
    ComputeOneToManyShortestPaths(graph, lengths, source, some_nodes,
                                  &distance_container);
    index = source * graph.num_nodes();
    for (int i = 0; i < some_nodes.size(); ++i) {
      CheckPathDataPair(container, distance_container,
                        expected_distances[index + some_nodes[i]],
                        expected_paths[index + some_nodes[i]], source,
                        some_nodes[i]);
    }
  }  // Many-to-many shortest paths.
  {
    BUILD_CONTAINERS();
    ComputeManyToManyShortestPathsWithMultipleThreads(
        graph, lengths, some_nodes, some_nodes, kThreads, &container);
    ComputeManyToManyShortestPathsWithMultipleThreads(
        graph, lengths, some_nodes, some_nodes, kThreads, &distance_container);
    for (int i = 0; i < some_nodes.size(); ++i) {
      index = some_nodes[i] * graph.num_nodes();
      for (int j = 0; j < some_nodes.size(); ++j) {
        CheckPathDataPair(container, distance_container,
                          expected_distances[index + some_nodes[j]],
                          expected_paths[index + some_nodes[j]], some_nodes[i],
                          some_nodes[j]);
      }
    }
  }
}

#undef BUILD_CONTAINERS

template <class GraphType>
void TestShortestPathsFromGraph(
    int num_nodes, int num_arcs, const typename GraphType::NodeIndex arcs[][2],
    const PathDistance arc_lengths[],
    const typename GraphType::NodeIndex expected_paths[],
    const PathDistance expected_distances[]) {
  GraphType graph(num_nodes, num_arcs);
  std::vector<PathDistance> lengths(num_arcs);
  for (int i = 0; i < num_arcs; ++i) {
    lengths[graph.AddArc(arcs[i][0], arcs[i][1])] = arc_lengths[i];
  }
  std::vector<typename GraphType::ArcIndex> permutation;
  graph.Build(&permutation);
  util::Permute(permutation, &lengths);
  TestShortestPathsFromGraph(graph, lengths, expected_paths,
                             expected_distances);
}

// Series of shortest paths tests on small graphs.

// Empty fixture templates to collect the types of graphs on which
// we want to base the shortest paths template instances that we
// test.
template <typename GraphType>
class ShortestPathsDeathTest : public testing::Test {};
template <typename GraphType>
class ShortestPathsTest : public testing::Test {};

typedef testing::Types<StarGraph, ForwardStarGraph>
    EbertGraphTypesForShortestPathsTesting;

TYPED_TEST_SUITE(ShortestPathsDeathTest,
                 EbertGraphTypesForShortestPathsTesting);
TYPED_TEST_SUITE(ShortestPathsTest, EbertGraphTypesForShortestPathsTesting);

template <typename GraphType>
class GraphShortestPathsDeathTest : public testing::Test {};
template <typename GraphType>
class GraphShortestPathsTest : public testing::Test {};

typedef testing::Types<
    ::util::ListGraph<>, ::util::StaticGraph<>, ::util::ReverseArcListGraph<>,
    ::util::ReverseArcStaticGraph<>, ::util::ReverseArcMixedGraph<>>
    GraphTypesForShortestPathsTesting;

TYPED_TEST_SUITE(GraphShortestPathsDeathTest,
                 GraphTypesForShortestPathsTesting);
TYPED_TEST_SUITE(GraphShortestPathsTest, GraphTypesForShortestPathsTesting);

// Test on an empty graph.
TYPED_TEST(GraphShortestPathsDeathTest, ShortestPathsEmptyGraph) {
  const auto kExpectedPaths = nullptr;
  const auto kExpectedDistances = nullptr;
  TypeParam graph;
  std::vector<PathDistance> lengths;
  TestShortestPathsFromGraph(graph, lengths, kExpectedPaths,
                             kExpectedDistances);
}

// Test on a disconnected graph (set of nodes pointing to themselves).
TYPED_TEST(GraphShortestPathsDeathTest, ShortestPathsAllDisconnected) {
  const typename TypeParam::NodeIndex kUnconnected = TypeParam::kNilNode;
  const int kNodes = 3;
  const typename TypeParam::NodeIndex kArcs[][2] = {{0, 0}, {1, 1}, {2, 2}};
  const PathDistance kArcLengths[] = {0, 0, 0};
  const int kExpectedPaths[] = {0, kUnconnected, kUnconnected, kUnconnected,
                                1, kUnconnected, kUnconnected, kUnconnected,
                                2};
  const PathDistance kExpectedDistances[] = {0,
                                             kDisconnectedPathDistance,
                                             kDisconnectedPathDistance,
                                             kDisconnectedPathDistance,
                                             0,
                                             kDisconnectedPathDistance,
                                             kDisconnectedPathDistance,
                                             kDisconnectedPathDistance,
                                             0};
  TestShortestPathsFromGraph<TypeParam>(kNodes, ABSL_ARRAYSIZE(kArcLengths),
                                        kArcs, kArcLengths, kExpectedPaths,
                                        kExpectedDistances);
}

//       1        1        1
//  -> 0 ---> 2 ------> 4 <--- 1
// |   |                |      |
// |   |4              1|      |
// |   |        1       |     3|
// |1   ---> 3 ---> 5 <-       |
// |               ||          |
//  ---------------  ----------
//
TYPED_TEST(GraphShortestPathsDeathTest, ShortestPaths1) {
  const int kNodes = 6;
  const typename TypeParam::NodeIndex kArcs[][2] = {
      {0, 2}, {0, 3}, {1, 4}, {2, 4}, {3, 5}, {4, 5}, {5, 0}, {5, 1}};
  const PathDistance kArcLengths[] = {1, 4, 1, 1, 1, 1, 1, 3};
  const int kExpectedPaths[] = {5, 5, 0, 0, 2, 4, 5, 5, 0, 0, 1, 4,
                                5, 5, 0, 0, 2, 4, 5, 5, 0, 0, 2, 3,
                                5, 5, 0, 0, 2, 4, 5, 5, 0, 0, 2, 4};
  const PathDistance kExpectedDistances[] = {
      4, 6, 1, 4, 2, 3, 3, 5, 4, 7, 1, 2, 3, 5, 4, 7, 1, 2,
      2, 4, 3, 6, 4, 1, 2, 4, 3, 6, 4, 1, 1, 3, 2, 5, 3, 4};
  TestShortestPathsFromGraph<TypeParam>(kNodes, ABSL_ARRAYSIZE(kArcLengths),
                                        kArcs, kArcLengths, kExpectedPaths,
                                        kExpectedDistances);
}

//   0
//  ---
// |   |   1        4
//  -> 0 -----> 1 -----> 4 --
//    ||        |        ^   |
//    ||3       |1      1|   |
//    ||        |        |   |
//    | ------> 2 -------    |
//    |                      |
//    |          1           |
//     ----------------------
TYPED_TEST(GraphShortestPathsDeathTest, ShortestPaths2) {
  const int kNodes = 4;
  const typename TypeParam::NodeIndex kArcs[][2] = {
      {0, 1}, {0, 0}, {0, 2}, {1, 2}, {1, 3}, {2, 3}, {3, 0}};
  const PathDistance kArcLengths[] = {1, 0, 3, 1, 4, 1, 1};
  const int kExpectedPaths[] = {0, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2};
  const PathDistance kExpectedDistances[] = {0, 1, 2, 3, 3, 4, 1, 2,
                                             2, 3, 4, 1, 1, 2, 3, 4};
  TestShortestPathsFromGraph<TypeParam>(kNodes, ABSL_ARRAYSIZE(kArcLengths),
                                        kArcs, kArcLengths, kExpectedPaths,
                                        kExpectedDistances);
}

TYPED_TEST(GraphShortestPathsDeathTest, MismatchedData) {
  TypeParam graph(2, 2);
  graph.AddArc(0, 1);
  graph.AddArc(1, 0);
  std::vector<PathDistance> lengths = {0};
  auto container =
      GenericPathContainer<TypeParam>::BuildInMemoryCompactPathContainer();
  EXPECT_DEATH(ComputeAllToAllShortestPathsWithMultipleThreads(graph, lengths,
                                                               1, &container),
               "Number of arcs in graph must match arc length vector size");
}

// Test the case where some sources are not strongly connected to themselves.
TYPED_TEST(GraphShortestPathsDeathTest, SourceNotConnectedToItself) {
  const typename TypeParam::NodeIndex kUnconnected = TypeParam::kNilNode;
  const int kNodes = 3;
  const typename TypeParam::NodeIndex kArcs[][2] = {{1, 2}, {2, 2}};
  const PathDistance kArcLengths[] = {1, 0};
  const int kExpectedPaths[] = {kUnconnected, kUnconnected, kUnconnected,
                                kUnconnected, kUnconnected, 1,
                                kUnconnected, kUnconnected, 2};
  const PathDistance kExpectedDistances[] = {kDisconnectedPathDistance,
                                             kDisconnectedPathDistance,
                                             kDisconnectedPathDistance,
                                             kDisconnectedPathDistance,
                                             kDisconnectedPathDistance,
                                             1,
                                             kDisconnectedPathDistance,
                                             kDisconnectedPathDistance,
                                             0};
  TestShortestPathsFromGraph<TypeParam>(kNodes, ABSL_ARRAYSIZE(kArcLengths),
                                        kArcs, kArcLengths, kExpectedPaths,
                                        kExpectedDistances);
}

// Test the case where the graph is a multigraph, a graph with parallel arcs
// (arcs which have the same end nodes).
TYPED_TEST(GraphShortestPathsDeathTest, Multigraph) {
  const int kNodes = 4;
  const typename TypeParam::NodeIndex kArcs[][2] = {
      {0, 1}, {0, 1}, {0, 2}, {0, 2}, {1, 3}, {2, 3}, {1, 3}, {2, 3}, {3, 0}};
  const PathDistance kArcLengths[] = {2, 3, 1, 2, 2, 2, 1, 1, 1};
  const int kExpectedPaths[] = {3, 0, 0, 2, 3, 0, 0, 1, 3, 0, 0, 2, 3, 0, 0, 2};
  const PathDistance kExpectedDistances[] = {3, 2, 1, 2, 2, 4, 3, 1,
                                             2, 4, 3, 1, 1, 3, 2, 3};
  TestShortestPathsFromGraph<TypeParam>(kNodes, ABSL_ARRAYSIZE(kArcLengths),
                                        kArcs, kArcLengths, kExpectedPaths,
                                        kExpectedDistances);
}

// Large test on a random strongly connected graph with 10,000,000 nodes and
// 50,000,000 arcs.
// Shortest paths are computed between 10 randomly chosen nodes.
TYPED_TEST(GraphShortestPathsTest, DISABLED_LargeRandomShortestPaths) {
  const int kSize = 10000000;
  const int kDegree = 4;
  const int max_distance = 50;
  const PathDistance kConnectionArcLength = 300;
  std::mt19937 randomizer(12345);
  TypeParam graph(kSize, kSize + kSize * kDegree);
  std::vector<PathDistance> lengths;
  for (int i = 0; i < kSize; ++i) {
    const typename TypeParam::NodeIndex tail(
        absl::Uniform(randomizer, 0, kSize));
    for (int j = 0; j < kDegree; ++j) {
      const typename TypeParam::NodeIndex head(
          absl::Uniform(randomizer, 0, kSize));
      const PathDistance length =
          1 + absl::Uniform(randomizer, 0, max_distance);
      graph.AddArc(tail, head);
      lengths.push_back(length);
    }
  }
  typename TypeParam::NodeIndex prev_index = TypeParam::kNilNode;
  typename TypeParam::NodeIndex first_index = TypeParam::kNilNode;
  for (const typename TypeParam::NodeIndex node_index : graph.AllNodes()) {
    if (prev_index != TypeParam::kNilNode) {
      graph.AddArc(prev_index, node_index);
      lengths.push_back(kConnectionArcLength);
    } else {
      first_index = node_index;
    }
    prev_index = node_index;
  }
  graph.AddArc(prev_index, first_index);
  lengths.push_back(kConnectionArcLength);
  std::vector<typename TypeParam::ArcIndex> permutation;
  graph.Build(&permutation);
  util::Permute(permutation, &lengths);
  std::vector<std::vector<typename TypeParam::NodeIndex>> components;
  ::FindStronglyConnectedComponents(graph.num_nodes(), graph, &components);
  CHECK_EQ(1, components.size());
  CHECK_EQ(kSize, components[0].size());
  const int kSourceSize = 10;
  const int source_size = std::min(graph.num_nodes(), kSourceSize);
  std::vector<typename TypeParam::NodeIndex> sources(source_size, 0);
  for (int i = 0; i < source_size; ++i) {
    sources[i] = absl::Uniform(randomizer, 0, graph.num_nodes());
  }
  const int kThreads = 10;
  auto container =
      GenericPathContainer<TypeParam>::BuildInMemoryCompactPathContainer();
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, lengths, sources, sources, kThreads, &container);
  auto distance_container =
      GenericPathContainer<TypeParam>::BuildPathDistanceContainer();
  ComputeManyToManyShortestPathsWithMultipleThreads(
      graph, lengths, sources, sources, kThreads, &distance_container);
  for (int tail = 0; tail < sources.size(); ++tail) {
    for (int head = 0; head < sources.size(); ++head) {
      EXPECT_NE(TypeParam::kNilNode, container.GetPenultimateNodeInPath(
                                         sources[tail], sources[head]));
      EXPECT_NE(kDisconnectedPathDistance,
                container.GetDistance(sources[tail], sources[head]));
      EXPECT_NE(kDisconnectedPathDistance,
                distance_container.GetDistance(sources[tail], sources[head]));
    }
  }
}
}  // namespace operations_research
