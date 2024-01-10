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

#include "ortools/graph/dag_shortest_path.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/io.h"
#include "ortools/util/flat_matrix.h"

namespace operations_research {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::status::StatusIs;

TEST(ShortestPathOnDagTest, SimpleGraph) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int b = 3;
  const int num_nodes = 4;
  const std::vector<ArcWithLength> arcs_with_length = {{source, a, 5.0},
                                                       {source, b, 2.0},
                                                       {a, destination, 3.0},
                                                       {b, destination, 20.0}};

  EXPECT_THAT(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      FieldsAre(/*length=*/8.0, /*arc_path=*/ElementsAre(0, 2),
                /*node_path=*/ElementsAre(source, a, destination)));
}

TEST(ShortestPathOnDagTest, LargerGraphWithNegativeCost) {
  const int source = 0;
  const int a = 3;
  const int b = 2;
  const int c = 1;
  const int destination = 4;
  const int num_nodes = 5;
  const std::vector<ArcWithLength> arcs_with_length = {
      {a, c, 5.0},      {source, b, 7.0},      {a, b, 1.0},
      {source, a, 3.0}, {c, destination, 5.0}, {b, destination, -2.0}};

  EXPECT_THAT(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      FieldsAre(/*length=*/2.0, /*arc_path=*/ElementsAre(3, 2, 5),
                /*node_path=*/ElementsAre(source, a, b, destination)));
}

TEST(ShortestPathOnDagTest, Cycle) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLength> arcs_with_length = {
      {source, destination, 1.0}, {destination, source, 2.0}};

  EXPECT_DEATH(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      "cycle");
}

TEST(ShortestPathOnDagTest, NaNWeight) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLength> arcs_with_length = {
      {source, destination, std::numeric_limits<double>::quiet_NaN()}};

  EXPECT_DEBUG_DEATH(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      "NaN");
}

TEST(ShortestPathOnDagTest, MinusInfWeight) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLength> arcs_with_length = {
      {source, destination, -kInf}};

  EXPECT_DEBUG_DEATH(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      "-inf");
}

TEST(ShortestPathOnDagTest, SetsNotConnected) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int num_nodes = 3;
  const std::vector<ArcWithLength> arcs_with_length = {{source, a, 1.0}};

  EXPECT_THAT(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      FieldsAre(/*length=*/kInf, /*arc_path=*/IsEmpty(),
                /*node_path=*/IsEmpty()));
}

TEST(ShortestPathOnDagTest, SetsNotConnectedDueToInfiniteCost) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int num_nodes = 3;
  const std::vector<ArcWithLength> arcs_with_length = {{a, destination, 1.0},
                                                       {source, a, kInf}};

  EXPECT_THAT(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      FieldsAre(/*length=*/kInf, /*arc_path=*/IsEmpty(),
                /*node_path=*/IsEmpty()));
}

TEST(ShortestPathOnDagTest, AvoidInfiniteCost) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int b = 3;
  const int num_nodes = 4;
  const std::vector<ArcWithLength> arcs_with_length = {{a, destination, 1.0},
                                                       {b, destination, 1.0},
                                                       {source, a, kInf},
                                                       {source, b, 3.0}};

  EXPECT_THAT(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      FieldsAre(/*length=*/4.0, /*arc_path=*/ElementsAre(3, 1),
                /*node_path=*/ElementsAre(source, b, destination)));
}

TEST(ShortestPathOnDagTest, SourceNotFirst) {
  const int destination = 0;
  const int source = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLength> arcs_with_length = {
      {source, destination, 1.0}};

  EXPECT_THAT(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      FieldsAre(/*length=*/1.0, /*arc_path=*/ElementsAre(0),
                /*node_path=*/ElementsAre(source, destination)));
}

TEST(ShortestPathOnDagTest, MultipleArcs) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLength> arcs_with_length = {
      {source, destination, 4.0}, {source, destination, 2.0}};

  EXPECT_THAT(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      FieldsAre(/*length=*/2.0, /*arc_path=*/ElementsAre(1),
                /*node_path=*/ElementsAre(source, destination)));
}

TEST(ShortestPathOnDagTest, UpdateCost) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int b = 3;
  const int num_nodes = 4;
  std::vector<ArcWithLength> arcs_with_length = {{source, a, 5.0},
                                                 {source, b, 2.0},
                                                 {a, destination, 3.0},
                                                 {b, destination, 20.0}};

  EXPECT_THAT(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      FieldsAre(/*length=*/8.0, /*arc_path=*/ElementsAre(0, 2),
                /*node_path=*/ElementsAre(source, a, destination)));

  // Update the length of arc b -> destination from 20.0 to -1.0.
  arcs_with_length[3].length = -1.0;

  EXPECT_THAT(
      ShortestPathsOnDag(num_nodes, arcs_with_length, source, destination),
      FieldsAre(/*length=*/1.0, /*arc_path=*/ElementsAre(1, 3),
                /*node_path=*/ElementsAre(source, b, destination)));
}

TEST(ShortestPathsOnDagWrapperTest, ValidateTopologicalOrder) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/1);
  std::vector<double> arc_lengths;
  graph.AddArc(source, destination);
  arc_lengths.push_back(1.0);
  const std::vector<int> topological_order = {source};

  EXPECT_DEBUG_DEATH(ShortestPathsOnDagWrapper<util::ListGraph<>>(
                         &graph, &arc_lengths, topological_order),
                     "Invalid topological order");
}

TEST(ShortestPathsOnDagWrapperTest, MultipleSources) {
  const int source_1 = 0;
  const int source_2 = 1;
  const int destination = 2;
  const int num_nodes = 3;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/2);
  std::vector<double> arc_lengths;
  graph.AddArc(source_1, destination);
  arc_lengths.push_back(-6.0);
  graph.AddArc(source_2, destination);
  arc_lengths.push_back(3.0);
  const std::vector<int> topological_order = {source_2, source_1, destination};
  ShortestPathsOnDagWrapper<util::ListGraph<>> shortest_path_on_dag(
      &graph, &arc_lengths, topological_order);
  shortest_path_on_dag.RunShortestPathOnDag({source_1, source_2});

  EXPECT_TRUE(shortest_path_on_dag.IsReachable(destination));
  EXPECT_THAT(shortest_path_on_dag.LengthTo(destination), -6.0);
  EXPECT_THAT(shortest_path_on_dag.ArcPathTo(destination), ElementsAre(0));
  EXPECT_THAT(shortest_path_on_dag.NodePathTo(destination),
              ElementsAre(source_1, destination));
}

TEST(ShortestPathOnDagTest, ShortestPathGoesThroughMultipleSources) {
  const int source_1 = 0;
  const int source_2 = 1;
  const int destination = 2;
  const int num_nodes = 3;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/2);
  std::vector<double> arc_lengths;
  graph.AddArc(source_1, source_2);
  arc_lengths.push_back(-7.0);
  graph.AddArc(source_2, destination);
  arc_lengths.push_back(3.0);
  const std::vector<int> topological_order = {source_1, source_2, destination};
  ShortestPathsOnDagWrapper<util::ListGraph<>> shortest_path_on_dag(
      &graph, &arc_lengths, topological_order);
  shortest_path_on_dag.RunShortestPathOnDag({source_1, source_2});

  EXPECT_TRUE(shortest_path_on_dag.IsReachable(destination));
  EXPECT_THAT(shortest_path_on_dag.LengthTo(destination), -4.0);
  EXPECT_THAT(shortest_path_on_dag.ArcPathTo(destination), ElementsAre(0, 1));
  EXPECT_THAT(shortest_path_on_dag.NodePathTo(destination),
              ElementsAre(source_1, source_2, destination));
}

TEST(ShortestPathOnDagTest, MultipleDestinations) {
  const int source = 0;
  const int destination_1 = 1;
  const int destination_2 = 2;
  const int destination_3 = 3;
  const int num_nodes = 4;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/3);
  std::vector<double> arc_lengths;
  graph.AddArc(source, destination_1);
  arc_lengths.push_back(3.0);
  graph.AddArc(source, destination_2);
  arc_lengths.push_back(1.0);
  graph.AddArc(source, destination_3);
  arc_lengths.push_back(2.0);
  const std::vector<int> topological_order = {source, destination_3,
                                              destination_1, destination_2};
  ShortestPathsOnDagWrapper<util::ListGraph<>> shortest_path_on_dag(
      &graph, &arc_lengths, topological_order);
  shortest_path_on_dag.RunShortestPathOnDag({source});

  EXPECT_TRUE(shortest_path_on_dag.IsReachable(destination_1));
  EXPECT_THAT(shortest_path_on_dag.LengthTo(destination_1), 3.0);
  EXPECT_THAT(shortest_path_on_dag.ArcPathTo(destination_1), ElementsAre(0));
  EXPECT_THAT(shortest_path_on_dag.NodePathTo(destination_1),
              ElementsAre(source, destination_1));

  EXPECT_TRUE(shortest_path_on_dag.IsReachable(destination_2));
  EXPECT_THAT(shortest_path_on_dag.LengthTo(destination_2), 1.0);
  EXPECT_THAT(shortest_path_on_dag.ArcPathTo(destination_2), ElementsAre(1));
  EXPECT_THAT(shortest_path_on_dag.NodePathTo(destination_2),
              ElementsAre(source, destination_2));

  EXPECT_TRUE(shortest_path_on_dag.IsReachable(destination_3));
  EXPECT_THAT(shortest_path_on_dag.LengthTo(destination_3), 2.0);
  EXPECT_THAT(shortest_path_on_dag.ArcPathTo(destination_3), ElementsAre(2));
  EXPECT_THAT(shortest_path_on_dag.NodePathTo(destination_3),
              ElementsAre(source, destination_3));
}

TEST(ShortestPathsOnDagWrapperTest, UpdateCost) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int b = 3;
  const int num_nodes = 4;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/4);
  std::vector<double> arc_lengths;
  graph.AddArc(source, a);
  arc_lengths.push_back(5.0);
  graph.AddArc(source, b);
  arc_lengths.push_back(2.0);
  graph.AddArc(a, destination);
  arc_lengths.push_back(3.0);
  graph.AddArc(b, destination);
  arc_lengths.push_back(20.0);
  const std::vector<int> topological_order = {source, a, b, destination};
  ShortestPathsOnDagWrapper<util::ListGraph<>> shortest_path_on_dag(
      &graph, &arc_lengths, topological_order);
  shortest_path_on_dag.RunShortestPathOnDag({source});

  EXPECT_TRUE(shortest_path_on_dag.IsReachable(destination));
  EXPECT_THAT(shortest_path_on_dag.LengthTo(destination), 8.0);
  EXPECT_THAT(shortest_path_on_dag.ArcPathTo(destination), ElementsAre(0, 2));
  EXPECT_THAT(shortest_path_on_dag.NodePathTo(destination),
              ElementsAre(source, a, destination));

  // Update the length of arc b -> destination from 20.0 to -1.0.
  arc_lengths[3] = -1.0;
  shortest_path_on_dag.RunShortestPathOnDag({source});

  EXPECT_TRUE(shortest_path_on_dag.IsReachable(destination));
  EXPECT_THAT(shortest_path_on_dag.LengthTo(destination), 1.0);
  EXPECT_THAT(shortest_path_on_dag.ArcPathTo(destination), ElementsAre(1, 3));
  EXPECT_THAT(shortest_path_on_dag.NodePathTo(destination),
              ElementsAre(source, b, destination));
}

TEST(TopologicalOrderIsValidTest, ValidateTopologicalOrder) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/2);
  graph.AddArc(source, destination);

  EXPECT_OK(TopologicalOrderIsValid(graph, {source, destination}));
  EXPECT_THAT(TopologicalOrderIsValid(graph, {source}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("topological_order.size() = 1")));
  EXPECT_THAT(TopologicalOrderIsValid(graph, {source, source}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("0 appears twice")));
  EXPECT_THAT(TopologicalOrderIsValid(graph, {destination, source}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("arc (0, 1) is inconsistent")));

  graph.AddArc(source, source);

  EXPECT_THAT(TopologicalOrderIsValid(graph, {source, destination}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("arc (0, 0) is inconsistent")));
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
// except if the first arc from 0 to num_nodes-1 where it is set to a large
// length.
std::vector<double> GenerateRandomLengths(const util::StaticGraph<>& graph,
                                          const double min_length = 0.0,
                                          const double max_length = 10.0,
                                          const double large_length = 10000.0) {
  absl::BitGen bit_gen;
  std::vector<double> arc_lengths;
  arc_lengths.reserve(graph.num_arcs());
  bool large_length_set = false;
  for (util::StaticGraph<>::ArcIndex arc = 0; arc < graph.num_arcs(); ++arc) {
    if (!large_length_set && graph.Tail(arc) == 0 &&
        graph.Head(arc) == graph.num_nodes() - 1) {
      arc_lengths.push_back(large_length);
      large_length_set = true;
      continue;
    }
    arc_lengths.push_back(static_cast<double>(
        absl::Uniform<int>(bit_gen, min_length, max_length)));
  }
  return arc_lengths;
}

TEST(ShortestPathOnDagTest, RandomizedStressTest) {
  absl::BitGen bit_gen;
  const int kNumTests = 10000;
  for (int test = 0; test < kNumTests; ++test) {
    const int num_nodes = absl::Uniform(bit_gen, 2, 12);
    const int num_arcs = absl::Uniform(
        bit_gen, 1, std::min(num_nodes * (num_nodes - 1) / 2, 15));
    // Generate a random DAG with random lengths.
    const auto [graph, topological_order] = BuildRandomDag(num_nodes, num_arcs);
    const std::vector<double> arc_lengths = GenerateRandomLengths(graph);

    // Run Floyd-Warshall as a 'reference' shortest path algorithm.
    FlatMatrix<double> ref_dist(num_nodes, num_nodes, kInf);
    for (int a = 0; a < num_arcs; ++a) {
      double& d = ref_dist[graph.Tail(a)][graph.Head(a)];
      if (arc_lengths[a] < d) d = arc_lengths[a];
    }
    for (int node = 0; node < num_nodes; ++node) {
      ref_dist[node][node] = 0;
    }
    for (int k = 0; k < num_nodes; ++k) {
      for (int i = 0; i < num_nodes; ++i) {
        for (int j = 0; j < num_nodes; ++j) {
          const double dist_through_k = ref_dist[i][k] + ref_dist[k][j];
          if (dist_through_k < ref_dist[i][j]) ref_dist[i][j] = dist_through_k;
        }
      }
    }

    // Now, run some shortest paths and verify that they match. To balance out
    // the FW (Floyd-Warshall) which is O(N³), we run more than one shortest
    // path per FW.
    ShortestPathsOnDagWrapper<util::StaticGraph<>> shortest_path_on_dag(
        &graph, &arc_lengths, topological_order);
    for (int _ = 0; _ < 20; ++_) {
      // Draw sources (*with* repetition) with initial distances.
      const int num_sources = absl::Uniform(bit_gen, 1, 5);
      std::vector<int> sources(num_sources);
      for (int& source : sources) {
        source = absl::Uniform(bit_gen, 0, num_nodes);
      }
      // Precompute the reference minimum distance to each node (using any of
      // the sources), and the expected reached nodes: any node whose distance
      // is < kInf.
      std::vector<double> node_min_dist(num_nodes, kInf);
      std::vector<int> expected_reached_nodes;
      for (int node = 0; node < num_nodes; ++node) {
        double min_dist = kInf;
        for (const int source : sources) {
          min_dist = std::min(min_dist, ref_dist[source][node]);
        }
        node_min_dist[node] = min_dist;
        if (min_dist < kInf) expected_reached_nodes.push_back(node);
      }
      shortest_path_on_dag.RunShortestPathOnDag(sources);
      for (const int node : expected_reached_nodes) {
        EXPECT_TRUE(shortest_path_on_dag.IsReachable(node));
        EXPECT_EQ(shortest_path_on_dag.LengthTo(node), node_min_dist[node])
            << node;
      }
      ASSERT_FALSE(HasFailure())
          << DUMP_VARS(num_nodes, num_arcs, num_sources, sources, arc_lengths)
          << "\n With graph:\n"
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
  // Generate at most 20 scenarios of random arc lengths.
  const int num_scenarios = std::min(20, (int)state.iterations());
  std::vector<std::vector<double>> arc_lengths_scenarios;
  for (int _ = 0; _ < num_scenarios; ++_) {
    arc_lengths_scenarios.push_back(GenerateRandomLengths(graph));
  }
  std::vector<double>* arc_lengths = &arc_lengths_scenarios.front();
  ShortestPathsOnDagWrapper<util::StaticGraph<>> shortest_path_on_dag(
      &graph, arc_lengths, topological_order);
  for (auto _ : state) {
    // Pick a arc lengths scenario at random.
    arc_lengths =
        &(arc_lengths_scenarios[absl::Uniform(bit_gen, 0, num_scenarios)]);
    shortest_path_on_dag.RunShortestPathOnDag({0});
    CHECK(shortest_path_on_dag.IsReachable(num_nodes - 1));
    const double minimum_length = shortest_path_on_dag.LengthTo(num_nodes - 1);
    CHECK_GE(minimum_length, 0.0);
    CHECK_LE(minimum_length, 10000.0);
  }
  state.SetItemsProcessed(state.iterations() * (num_nodes + num_arcs));
}

BENCHMARK(BM_RandomDag)
    ->ArgPair(1000, 10)
    ->ArgPair(1 << 16, 4)
    ->ArgPair(1 << 16, 16)
    ->ArgPair(1 << 22, 4)
    ->ArgPair(1 << 22, 16);

void BM_LineDag(benchmark::State& state) {
  const int num_nodes = state.range(0);
  const int num_edges = num_nodes - 1;
  std::vector<int> topological_order(num_nodes);
  util::StaticGraph<> graph(num_nodes, num_edges);
  absl::c_iota(topological_order, 0);
  for (int i = 0; i < num_nodes - 1; ++i) {
    graph.AddArc(i, i + 1);
  }
  graph.Build();
  std::vector<double> arc_lengths(num_edges, 1);
  ShortestPathsOnDagWrapper<util::StaticGraph<>> shortest_path_on_dag(
      &graph, &arc_lengths, topological_order);
  for (auto _ : state) {
    shortest_path_on_dag.RunShortestPathOnDag({0});
    CHECK(shortest_path_on_dag.IsReachable(num_nodes - 1));
    CHECK_EQ(shortest_path_on_dag.LengthTo(num_nodes - 1), num_nodes - 1);
    CHECK_EQ(shortest_path_on_dag.ArcPathTo(num_nodes - 1).size(),
             num_nodes - 1);
    CHECK_EQ(shortest_path_on_dag.NodePathTo(num_nodes - 1).size(), num_nodes);
  }
  state.SetItemsProcessed(state.iterations() * num_nodes);
}

BENCHMARK(BM_LineDag)->Arg(1 << 16)->Arg(1 << 22)->Arg(1 << 24);

}  // namespace
}  // namespace operations_research
