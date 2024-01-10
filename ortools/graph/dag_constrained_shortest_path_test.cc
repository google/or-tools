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

#include "ortools/graph/dag_constrained_shortest_path.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/graph/dag_shortest_path.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/io.h"

namespace operations_research {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::IsEmpty;

TEST(ConstrainedShortestPathOnDagTest, SimpleGraph) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int b = 3;
  const int num_nodes = 4;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, a, 5.0, {6.0}},
       {source, b, 2.0, {4.0}},
       {a, destination, 3.0, {2.0}},
       {b, destination, 20.0, {3.0}}};

  EXPECT_THAT(ConstrainedShortestPathsOnDag(
                  num_nodes, arcs_with_length_and_resources, source,
                  destination, /*max_resources=*/{7.0}),
              FieldsAre(/*length=*/22.0, /*arc_path=*/ElementsAre(1, 3),
                        /*node_path=*/ElementsAre(source, b, destination)));
}

TEST(ConstrainedShortestPathOnDagTest, SimpleGraphTwoPaths) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int b = 3;
  const int num_nodes = 4;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, a, 5.0, {2.0}},
       {source, b, 2.0, {1.0}},
       {a, destination, 3.0, {1.0}},
       {b, destination, 20.0, {1.0}}};

  EXPECT_THAT(ConstrainedShortestPathsOnDag(
                  num_nodes, arcs_with_length_and_resources, source,
                  destination, /*max_resources=*/{6.0}),
              FieldsAre(/*length=*/8.0, /*arc_path=*/ElementsAre(0, 2),
                        /*node_path=*/ElementsAre(source, a, destination)));
}

TEST(ConstrainedShortestPathOnDagTest, LargerGraphWithNegativeCost) {
  const int source = 0;
  const int a = 3;
  const int b = 2;
  const int c = 1;
  const int destination = 4;
  const int num_nodes = 5;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{a, c, 5.0, {5.0}},           {source, b, 7.0, {4.0}},
       {a, b, 1.0, {3.0}},           {source, a, 3.0, {4.0}},
       {c, destination, 5.0, {1.0}}, {b, destination, -2.0, {1.0}}};

  EXPECT_THAT(ConstrainedShortestPathsOnDag(
                  num_nodes, arcs_with_length_and_resources, source,
                  destination, /*max_resources=*/{6.0}),
              FieldsAre(/*length=*/5.0, /*arc_path=*/ElementsAre(1, 5),
                        /*node_path=*/ElementsAre(source, b, destination)));
}

TEST(ConstrainedShortestPathOnDagTest, LargerGraphWithDominance) {
  const int source = 0;
  const int a = 3;
  const int b = 2;
  const int c = 1;
  const int destination = 4;
  const int num_nodes = 5;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{a, c, 5.0, {1.0}},           {source, b, 1.0, {3.0}},
       {a, b, 7.0, {4.0}},           {source, a, 3.0, {4.0}},
       {c, destination, 5.0, {1.0}}, {b, destination, -2.0, {1.0}}};

  EXPECT_THAT(ConstrainedShortestPathsOnDag(
                  num_nodes, arcs_with_length_and_resources, source,
                  destination, /*max_resources=*/{6.0}),
              FieldsAre(/*length=*/-1.0, /*arc_path=*/ElementsAre(1, 5),
                        /*node_path=*/ElementsAre(source, b, destination)));
}

TEST(ConstrainedShortestPathOnDagTest, LargerGraphNoMaximumDuration) {
  const int source = 0;
  const int a = 3;
  const int b = 2;
  const int c = 1;
  const int destination = 4;
  const int num_nodes = 5;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{a, c, 5.0, {1.0}},           {source, b, 7.0, {4.0}},
       {a, b, 1.0, {3.0}},           {source, a, 3.0, {4.0}},
       {c, destination, 5.0, {1.0}}, {b, destination, -2.0, {1.0}}};

  EXPECT_THAT(
      ConstrainedShortestPathsOnDag(
          num_nodes, arcs_with_length_and_resources, source, destination,
          /*max_resources=*/{std::numeric_limits<double>::max()}),
      FieldsAre(/*length=*/2.0, /*arc_path=*/ElementsAre(3, 2, 5),
                /*node_path=*/ElementsAre(source, a, b, destination)));
}

TEST(ConstrainedShortestPathOnDagTest, GraphWithInefficientEdge) {
  const int source = 0;
  const int a = 1;
  const int destination = 2;
  const int num_nodes = 3;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, a, 3.0, {4.0}},
       {source, destination, 9.0, {6.0}},
       {a, destination, 5.0, {1.0}}};

  EXPECT_THAT(
      ConstrainedShortestPathsOnDag(num_nodes, arcs_with_length_and_resources,
                                    source, destination,
                                    /*max_resources=*/{6.0}),
      FieldsAre(/*length=*/8.0, /*arc_path=*/ElementsAre(0, 2),
                /*node_path=*/ElementsAre(source, a, destination)));
}

TEST(ConstrainedShortestPathOnDagTest, Cycle) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, destination, 1.0, {0.0}}, {destination, source, 2.0, {1.0}}};

  EXPECT_DEATH(
      ConstrainedShortestPathsOnDag(num_nodes, arcs_with_length_and_resources,
                                    source, destination,
                                    /*max_resources=*/{0.0}),
      "cycle");
}

TEST(ConstrainedShortestPathOnDagTest, MinusInfWeight) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, destination, -kInf, {0.0}}};

  EXPECT_DEBUG_DEATH(ConstrainedShortestPathsOnDag(
                         num_nodes, arcs_with_length_and_resources, source,
                         destination, /*max_resources=*/{0.0}),
                     "-inf");
}

TEST(ConstrainedShortestPathOnDagTest, SetsNotConnected) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int num_nodes = 3;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, a, 1.0, {0.0}}};

  EXPECT_THAT(ConstrainedShortestPathsOnDag(
                  num_nodes, arcs_with_length_and_resources, source,
                  destination, /*max_resources=*/{0.0}),
              FieldsAre(/*length=*/kInf,
                        /*arc_path=*/IsEmpty(),
                        /*node_path=*/IsEmpty()));
}

TEST(ConstrainedShortestPathOnDagTest, SetsNotConnectedDueToInfiniteCost) {
  const int source = 2;
  const int destination = 0;
  const int a = 1;
  const int num_nodes = 3;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{a, destination, 1.0, {0.0}}, {source, a, kInf, {0.0}}};

  EXPECT_THAT(ConstrainedShortestPathsOnDag(
                  num_nodes, arcs_with_length_and_resources, source,
                  destination, /*max_resources=*/{0.0}),
              FieldsAre(/*length=*/kInf,
                        /*arc_path=*/IsEmpty(),
                        /*node_path=*/IsEmpty()));
}

TEST(ConstrainedShortestPathOnDagTest, SetsNotConnectedDueToLackOfResources) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, destination, 1.0, {1.0, 8.0}},
       {source, destination, 2.0, {7.0, 2.0}},
       {source, destination, 3.0, {6.0, 3.0}}};

  EXPECT_THAT(ConstrainedShortestPathsOnDag(
                  num_nodes, arcs_with_length_and_resources, source,
                  destination, /*max_resources=*/{4.0, 4.0}),
              FieldsAre(/*length=*/kInf,
                        /*arc_path=*/IsEmpty(),
                        /*node_path=*/IsEmpty()));
}

TEST(ConstrainedShortestPathOnDagTest, MultipleArcs) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, destination, 4.0, {2.0}}, {source, destination, 2.0, {4.0}}};

  EXPECT_THAT(ConstrainedShortestPathsOnDag(
                  num_nodes, arcs_with_length_and_resources, source,
                  destination, /*max_resources=*/{3.0}),
              FieldsAre(/*length=*/4.0, /*arc_path=*/ElementsAre(0),
                        /*node_path=*/ElementsAre(source, destination)));
}

TEST(ConstrainedShortestPathOnDagTest, UpdateArcsLengthAndResources) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int b = 3;
  const int num_nodes = 4;
  std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources = {
      {source, a, 5.0, {1.0, 3.0}},
      {source, b, 2.0, {4.0, 10.0}},
      {a, destination, 3.0, {5.0, 9.0}},
      {b, destination, 20.0, {2.0, 2.0}}};
  const std::vector<double> max_resources = {6.0, 12.0};

  EXPECT_THAT(
      ConstrainedShortestPathsOnDag(num_nodes, arcs_with_length_and_resources,
                                    source, destination, max_resources),
      FieldsAre(/*length=*/8.0, /*arc_path=*/ElementsAre(0, 2),
                /*node_path=*/ElementsAre(source, a, destination)));

  // Update the length of arc b -> destination from 20.0 to -1.0.
  arcs_with_length_and_resources[3].length = -1.0;

  EXPECT_THAT(
      ConstrainedShortestPathsOnDag(num_nodes, arcs_with_length_and_resources,
                                    source, destination, max_resources),
      FieldsAre(/*length=*/1.0, /*arc_path=*/ElementsAre(1, 3),
                /*node_path=*/ElementsAre(source, b, destination)));

  // Update the first resource of arc source -> b from 4.0 to 5.0 making
  // the path source -> b -> destination infeasible.
  arcs_with_length_and_resources[1].resources[0] = 5.0;

  EXPECT_THAT(
      ConstrainedShortestPathsOnDag(num_nodes, arcs_with_length_and_resources,
                                    source, destination, max_resources),
      FieldsAre(/*length=*/8.0, /*arc_path=*/ElementsAre(0, 2),
                /*node_path=*/ElementsAre(source, a, destination)));
}

TEST(ConstrainedShortestPathsOnDagWrapperTest, ValidateTopologicalOrder) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/1);
  std::vector<double> arc_lengths;
  std::vector<std::vector<double>> arc_resources(1);
  graph.AddArc(source, destination);
  arc_lengths.push_back(1.0);
  arc_resources[0].push_back({1.0});
  const std::vector<int> topological_order = {source};
  const std::vector<int> sources = {source};
  const std::vector<int> destinations = {destination};
  const std::vector<double> max_resources = {0.0};

  EXPECT_DEATH(ConstrainedShortestPathsOnDagWrapper<util::ListGraph<>>(
                   &graph, &arc_lengths, &arc_resources, &topological_order,
                   &sources, &destinations, &max_resources),
               "");
}

TEST(ConstrainedShortestPathsOnDagWrapperTest,
     ShortestPathGoesThroughMultipleSources) {
  const int source_1 = 0;
  const int source_2 = 1;
  const int destination = 2;
  const int num_nodes = 3;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/2);
  std::vector<double> arc_lengths;
  std::vector<std::vector<double>> arc_resources;
  graph.AddArc(source_1, source_2);
  arc_lengths.push_back(-7.0);
  graph.AddArc(source_2, destination);
  arc_lengths.push_back(3.0);
  const std::vector<int> topological_order = {source_1, source_2, destination};
  const std::vector<int> sources = {source_1, source_2};
  const std::vector<int> destinations = {destination};
  const std::vector<double> max_resources = {};
  ConstrainedShortestPathsOnDagWrapper<util::ListGraph<>>
      constrained_shortest_path_on_dag(&graph, &arc_lengths, &arc_resources,
                                       &topological_order, &sources,
                                       &destinations, &max_resources);
  constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();

  EXPECT_TRUE(constrained_shortest_path_on_dag.IsReachable(destination));
  EXPECT_THAT(constrained_shortest_path_on_dag.LengthTo(destination), -4.0);
  EXPECT_THAT(constrained_shortest_path_on_dag.ArcPathTo(destination),
              ElementsAre(0, 1));
  EXPECT_THAT(constrained_shortest_path_on_dag.NodePathTo(destination),
              ElementsAre(source_1, source_2, destination));
}

TEST(ConstrainedShortestPathsOnDagWrapperTest, MultipleDestinations) {
  const int source = 0;
  const int destination_1 = 1;
  const int destination_2 = 2;
  const int destination_3 = 3;
  const int num_nodes = 4;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/3);
  std::vector<double> arc_lengths;
  std::vector<std::vector<double>> arc_resources(1);
  graph.AddArc(source, destination_1);
  arc_lengths.push_back(3.0);
  arc_resources[0].push_back(5.0);
  graph.AddArc(source, destination_2);
  arc_lengths.push_back(1.0);
  arc_resources[0].push_back(7.0);
  graph.AddArc(source, destination_3);
  arc_lengths.push_back(2.0);
  arc_resources[0].push_back(6.0);
  const std::vector<int> sources = {source};
  const std::vector<int> destinations = {destination_1, destination_2,
                                         destination_3};
  const std::vector<int> topological_order = {source, destination_3,
                                              destination_1, destination_2};
  const std::vector<double> max_resources = {6.0};
  ConstrainedShortestPathsOnDagWrapper<util::ListGraph<>>
      constrained_shortest_path_on_dag(&graph, &arc_lengths, &arc_resources,
                                       &topological_order, &sources,
                                       &destinations, &max_resources);
  constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();

  EXPECT_TRUE(constrained_shortest_path_on_dag.IsReachable(destination_1));
  EXPECT_THAT(constrained_shortest_path_on_dag.LengthTo(destination_1), 3.0);
  EXPECT_THAT(constrained_shortest_path_on_dag.ArcPathTo(destination_1),
              ElementsAre(0));
  EXPECT_THAT(constrained_shortest_path_on_dag.NodePathTo(destination_1),
              ElementsAre(source, destination_1));

  EXPECT_FALSE(constrained_shortest_path_on_dag.IsReachable(destination_2));
  EXPECT_DEATH(constrained_shortest_path_on_dag.LengthTo(destination_2), "");
  EXPECT_DEATH(constrained_shortest_path_on_dag.ArcPathTo(destination_2), "");
  EXPECT_DEATH(constrained_shortest_path_on_dag.NodePathTo(destination_2), "");

  EXPECT_TRUE(constrained_shortest_path_on_dag.IsReachable(destination_3));
  EXPECT_THAT(constrained_shortest_path_on_dag.LengthTo(destination_3), 2.0);
  EXPECT_THAT(constrained_shortest_path_on_dag.ArcPathTo(destination_3),
              ElementsAre(2));
  EXPECT_THAT(constrained_shortest_path_on_dag.NodePathTo(destination_3),
              ElementsAre(source, destination_3));
}

TEST(ConstrainedShortestPathsOnDagWrapperTest, UpdateArcsLength) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int b = 3;
  const int num_nodes = 4;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/4);
  std::vector<double> arc_lengths;
  std::vector<std::vector<double>> arc_resources(2);
  graph.AddArc(source, a);
  arc_lengths.push_back(5.0);
  arc_resources[0].push_back(1.0);
  arc_resources[1].push_back(3.0);
  graph.AddArc(source, b);
  arc_lengths.push_back(2.0);
  arc_resources[0].push_back(4.0);
  arc_resources[1].push_back(10.0);
  graph.AddArc(a, destination);
  arc_lengths.push_back(3.0);
  arc_resources[0].push_back(5.0);
  arc_resources[1].push_back(9.0);
  graph.AddArc(b, destination);
  arc_lengths.push_back(20.0);
  arc_resources[0].push_back(2.0);
  arc_resources[1].push_back(2.0);
  const std::vector<int> topological_order = {source, a, b, destination};
  const std::vector<int> sources = {source};
  const std::vector<int> destinations = {destination};
  const std::vector<double> max_resources = {6.0, 12.0};
  ConstrainedShortestPathsOnDagWrapper<util::ListGraph<>>
      constrained_shortest_path_on_dag(&graph, &arc_lengths, &arc_resources,
                                       &topological_order, &sources,
                                       &destinations, &max_resources);
  constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();

  EXPECT_TRUE(constrained_shortest_path_on_dag.IsReachable(destination));
  EXPECT_THAT(constrained_shortest_path_on_dag.LengthTo(destination), 8.0);
  EXPECT_THAT(constrained_shortest_path_on_dag.ArcPathTo(destination),
              ElementsAre(0, 2));
  EXPECT_THAT(constrained_shortest_path_on_dag.NodePathTo(destination),
              ElementsAre(source, a, destination));

  // Update the length of arc b -> destination from 20.0 to -1.0.
  arc_lengths[3] = -1.0;
  constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();

  EXPECT_TRUE(constrained_shortest_path_on_dag.IsReachable(destination));
  EXPECT_THAT(constrained_shortest_path_on_dag.LengthTo(destination), 1.0);
  EXPECT_THAT(constrained_shortest_path_on_dag.ArcPathTo(destination),
              ElementsAre(1, 3));
  EXPECT_THAT(constrained_shortest_path_on_dag.NodePathTo(destination),
              ElementsAre(source, b, destination));
}

// Builds a random DAG with a given number of nodes and arcs where 0 is always
// the first and num_nodes-1 the last element in the topological order. Note
// that the graph always include at least one arc from 0 to num_nodes-1.
std::pair<util::StaticGraph<>, std::vector<int>> BuildRandomDag(
    const int64_t num_nodes, const int64_t num_arcs) {
  absl::BitGen bit_gen;
  CHECK_GE(num_nodes, 2);
  CHECK_GE(num_arcs, 1);
  CHECK_LE(num_arcs, (num_nodes * (num_nodes - 1)) / 2);
  std::vector<int> topological_order(num_nodes);
  topological_order.back() = num_nodes - 1;
  absl::Span<int> non_start_end =
      absl::MakeSpan(topological_order).subspan(1, num_nodes - 2);
  absl::c_iota(non_start_end, 1);
  absl::c_shuffle(non_start_end, bit_gen);
  int edges_added = 0;
  util::StaticGraph<> graph(num_nodes, num_arcs);
  graph.AddArc(0, num_nodes - 1);
  while (edges_added < num_arcs - 1) {
    int start_index = absl::Uniform(bit_gen, 0, num_nodes - 1);
    int end_index = absl::Uniform(bit_gen, start_index + 1, num_nodes);
    graph.AddArc(topological_order[start_index], topological_order[end_index]);
    edges_added++;
  }
  graph.Build();
  return {graph, topological_order};
}

// The length of each arc is drawn uniformly at random within a given interval
// except if the first arc from 0 to num_nodes-1 where it is set to
// `start_to_end_value`.
std::vector<double> GenerateRandomIntegerValues(
    const util::StaticGraph<>& graph, const double min_value = 0.0,
    const double max_value = 10.0, const double start_to_end_value = 10000.0) {
  absl::BitGen bit_gen;
  std::vector<double> arc_values;
  arc_values.reserve(graph.num_arcs());
  bool start_to_end_value_set = false;
  for (util::StaticGraph<>::ArcIndex arc = 0; arc < graph.num_arcs(); ++arc) {
    if (!start_to_end_value_set && graph.Tail(arc) == 0 &&
        graph.Head(arc) == graph.num_nodes() - 1) {
      arc_values.push_back(start_to_end_value);
      start_to_end_value_set = true;
      continue;
    }
    arc_values.push_back(
        static_cast<double>(absl::Uniform<int>(bit_gen, min_value, max_value)));
  }
  return arc_values;
}

// TODO(b/316203403): Remplace bounds with correct value computed using MIP.
TEST(ConstrainedShortestPathsOnDagWrapperTest,
     RandomizedStressTestSingleResource) {
  absl::BitGen bit_gen;
  const int kNumTests = 10000;
  for (int test = 0; test < kNumTests; ++test) {
    const int num_nodes = absl::Uniform(bit_gen, 2, 12);
    const int num_arcs = absl::Uniform(
        bit_gen, 1, std::min(num_nodes * (num_nodes - 1) / 2, 15));
    // Generate a random DAG with random resources
    const auto [graph, topological_order] = BuildRandomDag(num_nodes, num_arcs);
    std::vector<double> arc_lengths(num_arcs);
    std::vector<std::vector<double>> arc_resources(1);
    arc_resources[0] = GenerateRandomIntegerValues(graph, /*min_value=*/1.0,
                                                   /*max_value=*/10.0,
                                                   /*start_to_end_value=*/1.0);
    const std::vector<int> sources = {0};
    std::vector<int> destinations(num_nodes);
    absl::c_iota(destinations, 0);
    const std::vector<double> max_resources = {15.0};
    ConstrainedShortestPathsOnDagWrapper<util::StaticGraph<>>
        constrained_shortest_path_on_dag(&graph, &arc_lengths, &arc_resources,
                                         &topological_order, &sources,
                                         &destinations, &max_resources);

    // Run DAG shortest path on `arc_length` as a LB.
    ShortestPathsOnDagWrapper<util::StaticGraph<>> shortest_path_length(
        &graph, &arc_lengths, topological_order);

    // Run DAG shortest path on `arc_resources` as a UB.
    ShortestPathsOnDagWrapper<util::StaticGraph<>> shortest_path_resources(
        &graph, &(arc_resources[0]), topological_order);
    shortest_path_resources.RunShortestPathOnDag(sources);

    // Precompute the expected reached nodes: any node whose resource is <=
    // max_resources[0].
    std::vector<int> expected_reached_nodes;
    for (int node = 0; node < num_nodes; ++node) {
      if (shortest_path_resources.LengthTo(node) <= max_resources[0]) {
        expected_reached_nodes.push_back(node);
      }
    }

    for (int _ = 0; _ < 20; ++_) {
      // Draw random lengths and recompute *un*constrained shortest paths.
      arc_lengths = GenerateRandomIntegerValues(graph);
      shortest_path_length.RunShortestPathOnDag(sources);

      constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();

      for (const int node : expected_reached_nodes) {
        const std::vector<int> ub_arc_path =
            shortest_path_resources.ArcPathTo(node);
        double ub_length = 0.0;
        for (const int arc : ub_arc_path) {
          ub_length += arc_lengths[arc];
        }
        EXPECT_TRUE(constrained_shortest_path_on_dag.IsReachable(node));
        EXPECT_GE(constrained_shortest_path_on_dag.LengthTo(node),
                  shortest_path_length.LengthTo(node))
            << node;
        EXPECT_LE(constrained_shortest_path_on_dag.LengthTo(node), ub_length)
            << node;
      }
      ASSERT_FALSE(HasFailure())
          << DUMP_VARS(num_nodes, num_arcs, arc_lengths) << "\n With graph:\n"
          << util::GraphToString(graph, util::PRINT_GRAPH_ARCS);
    }
  }
}

// -----------------------------------------------------------------------------
// Benchmark.
// -----------------------------------------------------------------------------
void BM_RandomDag(benchmark::State& state) {
  absl::BitGen bit_gen;
  // Generate a fixed random DAG.
  const int num_nodes = state.range(0);
  const int num_arcs = num_nodes * state.range(1);
  const auto [graph, topological_order] = BuildRandomDag(num_nodes, num_arcs);
  // Generate 20 scenarios of random arc lengths.
  const int num_scenarios = 20;
  std::vector<std::vector<double>> arc_lengths_scenarios;
  for (int _ = 0; _ < num_scenarios; ++_) {
    arc_lengths_scenarios.push_back(GenerateRandomIntegerValues(graph));
  }
  std::vector<double> arc_lengths(num_arcs);
  std::vector<std::vector<double>> arc_resources(1);
  arc_resources[0] =
      GenerateRandomIntegerValues(graph, /*min_value=*/1.0,
                                  /*max_value=*/10.0,
                                  /*start_to_end_value=*/num_nodes * 0.2);
  const std::vector<int> sources = {0};
  const std::vector<int> destinations = {num_nodes - 1};
  const std::vector<double> max_resources = {num_nodes * 0.2};
  ConstrainedShortestPathsOnDagWrapper<util::StaticGraph<>>
      constrained_shortest_path_on_dag(&graph, &arc_lengths, &arc_resources,
                                       &topological_order, &sources,
                                       &destinations, &max_resources);

  for (auto _ : state) {
    // Pick a arc lengths scenario at random.
    arc_lengths =
        arc_lengths_scenarios[absl::Uniform(bit_gen, 0, num_scenarios)];
    constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();
    CHECK(constrained_shortest_path_on_dag.IsReachable(num_nodes - 1));
    const double minimum_length =
        constrained_shortest_path_on_dag.LengthTo(num_nodes - 1);
    CHECK_GE(minimum_length, 0.0);
    CHECK_LE(minimum_length, 10000.0);
  }
  state.SetItemsProcessed(state.iterations() * (num_nodes + num_arcs));
}

BENCHMARK(BM_RandomDag)
    ->ArgPair(1 << 6, 4)
    ->ArgPair(1 << 6, 16)
    ->ArgPair(1 << 9, 4);

}  // namespace
}  // namespace operations_research
