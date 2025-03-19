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

#include "ortools/graph/dag_constrained_shortest_path.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/graph_io.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::IsEmpty;

TEST(GetInversePermutationTest, EmptyPermutation) {
  EXPECT_THAT(internal::GetInversePermutation<int>({}), IsEmpty());
}

TEST(GetInversePermutationTest, SingleElementPermutation) {
  EXPECT_THAT(internal::GetInversePermutation<int>({0}), ElementsAre(0));
}

TEST(GetInversePermutationTest, ThreeElementPermutation) {
  EXPECT_THAT(internal::GetInversePermutation<int>({1, 2, 0}),
              ElementsAre(2, 0, 1));
}

TEST(GetInversePermutationTest, RandomPermutation) {
  const int num_tests = 100;
  const int max_size = 1000;
  absl::BitGen bitgen;
  for (int unused = 0; unused < num_tests; ++unused) {
    const int num_elements = absl::Uniform<int>(bitgen, 0, max_size);
    std::vector<int> permutation(num_elements);
    absl::c_iota(permutation, 0);
    absl::c_shuffle(permutation, bitgen);
    const std::vector<int> inverse_permutation =
        internal::GetInversePermutation<int>(permutation);
    for (int i = 0; i < num_elements; ++i) {
      EXPECT_EQ(inverse_permutation[permutation[i]], i);
    }
  }
}

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

TEST(ConstrainedShortestPathOnDagTest, GraphWithNoArcs) {
  EXPECT_THAT(ConstrainedShortestPathsOnDag(
                  /*num_nodes=*/1, /*arcs_with_length_and_resources=*/{},
                  /*source=*/0, /*destination=*/0, /*max_resources=*/{7.0}),
              FieldsAre(/*length=*/0, /*arc_path=*/IsEmpty(),
                        /*node_path=*/ElementsAre(0)));
  EXPECT_THAT(ConstrainedShortestPathsOnDag(
                  /*num_nodes=*/2, /*arcs_with_length_and_resources=*/{},
                  /*source=*/0, /*destination=*/1, /*max_resources=*/{7.0}),
              FieldsAre(/*length=*/kInf, /*arc_path=*/IsEmpty(),
                        /*node_path=*/IsEmpty()));
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

TEST(ConstrainedShortestPathOnDagTest, NoResources) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int num_nodes = 3;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, a, 5.0, {}}, {a, destination, 3.0, {}}};

  EXPECT_DEATH(
      ConstrainedShortestPathsOnDag(num_nodes, arcs_with_length_and_resources,
                                    source, destination,
                                    /*max_resources=*/{}),
      "ortools/graph/dag_shortest_path.h");
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

TEST(ConstrainedShortestPathsOnDagWrapperTest,
     ShortestPathGoesThroughMultipleSources) {
  const int source_1 = 0;
  const int source_2 = 1;
  const int destination = 2;
  const int num_nodes = 3;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/2);
  std::vector<double> arc_lengths;
  std::vector<std::vector<double>> arc_resources(1);
  graph.AddArc(source_1, source_2);
  arc_lengths.push_back(-7.0);
  arc_resources[0].push_back(2.0);
  graph.AddArc(source_2, destination);
  arc_lengths.push_back(3.0);
  arc_resources[0].push_back(3.0);
  const std::vector<int> topological_order = {source_1, source_2, destination};
  const std::vector<int> sources = {source_1, source_2};
  const std::vector<int> destinations = {destination};
  const std::vector<double> max_resources = {6.0};
  ConstrainedShortestPathsOnDagWrapper<util::ListGraph<>>
      constrained_shortest_path_on_dag(&graph, &arc_lengths, &arc_resources,
                                       topological_order, sources, destinations,
                                       &max_resources);

  EXPECT_THAT(
      constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag(),
      FieldsAre(/*length=*/-4.0, /*arc_path=*/ElementsAre(0, 1),
                /*node_path=*/ElementsAre(source_1, source_2, destination)));
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
                                       topological_order, sources, destinations,
                                       &max_resources);

  EXPECT_THAT(
      constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag(),
      FieldsAre(/*length=*/2.0, /*arc_path=*/ElementsAre(2),
                /*node_path=*/ElementsAre(source, destination_3)));
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
                                       topological_order, sources, destinations,
                                       &max_resources);

  EXPECT_THAT(
      constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag(),
      FieldsAre(/*length=*/8.0, /*arc_path=*/ElementsAre(0, 2),
                /*node_path=*/ElementsAre(source, a, destination)));

  // Update the length of arc b -> destination from 20.0 to -1.0.
  arc_lengths[3] = -1.0;

  EXPECT_THAT(
      constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag(),
      FieldsAre(/*length=*/1.0, /*arc_path=*/ElementsAre(1, 3),
                /*node_path=*/ElementsAre(source, b, destination)));
}

TEST(ConstrainedShortestPathsOnDagWrapperTest, LimitMaximumNumberOfLabels) {
  const int source = 0;
  const int destination = 1;
  const int a = 2;
  const int num_nodes = 3;
  util::ListGraph<> graph(num_nodes, /*arc_capacity=*/2);
  std::vector<double> arc_lengths;
  std::vector<std::vector<double>> arc_resources(1);
  graph.AddArc(source, a);
  arc_lengths.push_back(5.0);
  arc_resources[0].push_back(2.0);
  graph.AddArc(a, destination);
  arc_lengths.push_back(-2.0);
  arc_resources[0].push_back(1.0);
  const std::vector<int> topological_order = {source, a, destination};
  const std::vector<int> sources = {source};
  const std::vector<int> destinations = {destination};
  const std::vector<double> max_resources = {6.0};
  ConstrainedShortestPathsOnDagWrapper<util::ListGraph<>>
      constrained_shortest_path_on_dag_with_one_label(
          &graph, &arc_lengths, &arc_resources, topological_order, sources,
          destinations, &max_resources, /*max_num_created_labels=*/1);
  ConstrainedShortestPathsOnDagWrapper<util::ListGraph<>>
      constrained_shortest_path_on_dag_with_four_labels(
          &graph, &arc_lengths, &arc_resources, topological_order, sources,
          destinations, &max_resources, /*max_num_created_labels=*/4);

  EXPECT_THAT(constrained_shortest_path_on_dag_with_one_label
                  .RunConstrainedShortestPathOnDag(),
              FieldsAre(/*length=*/kInf,
                        /*arc_path=*/IsEmpty(),
                        /*node_path=*/IsEmpty()));
  EXPECT_THAT(constrained_shortest_path_on_dag_with_four_labels
                  .RunConstrainedShortestPathOnDag(),
              FieldsAre(/*length=*/3.0, /*arc_path=*/ElementsAre(0, 1),
                        /*node_path=*/ElementsAre(source, a, destination)));
}

// Builds a random DAG with a given number of nodes and arcs where 0 is always
// the first and num_nodes-1 the last element in the topological order. Note
// that the graph always include at least one arc from 0 to num_nodes-1.
template <typename NodeIndex = int32_t, typename ArcIndex = int32_t>
std::pair<util::StaticGraph<NodeIndex, ArcIndex>, std::vector<NodeIndex>>
BuildRandomDag(const NodeIndex num_nodes, const ArcIndex num_arcs) {
  absl::BitGen bit_gen;
  CHECK_GE(num_nodes, 2);
  CHECK_GE(num_arcs, 1);
  CHECK_LE(num_arcs, (num_nodes * (num_nodes - 1)) / 2);
  std::vector<NodeIndex> topological_order(num_nodes);
  topological_order.back() = num_nodes - 1;
  absl::Span<NodeIndex> non_start_end =
      absl::MakeSpan(topological_order).subspan(1, num_nodes - 2);
  absl::c_iota(non_start_end, 1);
  absl::c_shuffle(non_start_end, bit_gen);
  ArcIndex edges_added = 0;
  util::StaticGraph<NodeIndex, ArcIndex> graph(num_nodes, num_arcs);
  graph.AddArc(0, num_nodes - 1);
  while (edges_added < num_arcs - 1) {
    NodeIndex start_index = absl::Uniform(bit_gen, 0, num_nodes - 1);
    NodeIndex end_index = absl::Uniform(bit_gen, start_index + 1, num_nodes);
    graph.AddArc(topological_order[start_index], topological_order[end_index]);
    edges_added++;
  }
  graph.Build();
  return {graph, topological_order};
}

// The length of each arc is drawn uniformly at random within a given interval
// except if the first arc from 0 to num_nodes-1 where it is set to
// `start_to_end_value`.
template <class GraphType>
std::vector<double> GenerateRandomIntegerValues(
    const GraphType& graph, const double min_value = 0.0,
    const double max_value = 10.0, const double start_to_end_value = 10000.0) {
  absl::BitGen bit_gen;
  std::vector<double> arc_values;
  arc_values.reserve(graph.num_arcs());
  bool start_to_end_value_set = false;
  for (typename GraphType::ArcIndex arc = 0; arc < graph.num_arcs(); ++arc) {
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

double SolveConstrainedShortestPathUsingIntegerProgramming(
    const util::StaticGraph<>& graph, absl::Span<const double> arc_lengths,
    absl::Span<const std::vector<double>> arc_resources,
    absl::Span<const double> max_resources,
    absl::Span<const util::StaticGraph<>::NodeIndex> sources,
    absl::Span<const util::StaticGraph<>::NodeIndex> destinations) {
  using NodeIndex = util::StaticGraph<>::NodeIndex;
  using ArcIndex = util::StaticGraph<>::ArcIndex;

  math_opt::Model model;
  std::vector<math_opt::Variable> arc_variables;
  std::vector<math_opt::LinearExpression> flow_conservation(graph.num_nodes(),
                                                            0.0);
  for (ArcIndex arc_index = 0; arc_index < graph.num_arcs(); ++arc_index) {
    arc_variables.push_back(model.AddBinaryVariable(absl::StrCat(
        arc_index, "_", graph.Tail(arc_index), "->", graph.Head(arc_index))));
    model.set_objective_coefficient(arc_variables[arc_index],
                                    arc_lengths[arc_index]);
    flow_conservation[graph.Head(arc_index)] -= arc_variables[arc_index];
    flow_conservation[graph.Tail(arc_index)] += arc_variables[arc_index];
  }

  math_opt::LinearExpression all_sources;
  math_opt::LinearExpression all_destinations;
  for (NodeIndex node_index = 0; node_index < graph.num_nodes(); ++node_index) {
    math_opt::LinearExpression net_flow = 0;
    if (absl::c_linear_search(sources, node_index)) {
      const math_opt::Variable s = model.AddBinaryVariable();
      all_sources += s;
      net_flow += s;
    }
    if (absl::c_linear_search(destinations, node_index)) {
      const math_opt::Variable t = model.AddBinaryVariable();
      all_destinations += t;
      net_flow -= t;
    }
    model.AddLinearConstraint(flow_conservation[node_index] == net_flow);
  }
  model.AddLinearConstraint(all_sources == 1);
  model.AddLinearConstraint(all_destinations == 1);
  for (int r = 0; r < max_resources.size(); ++r) {
    math_opt::LinearExpression variable_resources;
    for (ArcIndex arc_index = 0; arc_index < graph.num_arcs(); ++arc_index) {
      variable_resources +=
          arc_resources[r][arc_index] * arc_variables[arc_index];
    }
    model.AddLinearConstraint(variable_resources <= max_resources[r]);
  }
  const absl::StatusOr<math_opt::SolveResult> result =
      math_opt::Solve(model, math_opt::SolverType::kCpSat, {});
  CHECK_OK(result.status())
      << util::GraphToString(graph, util::PRINT_GRAPH_ARCS);
  CHECK_OK(result->termination.EnsureIsOptimal())
      << util::GraphToString(graph, util::PRINT_GRAPH_ARCS);
  return result->objective_value();
}

TEST(ConstrainedShortestPathsOnDagWrapperTest,
     RandomizedStressTestSingleResource) {
  absl::BitGen bit_gen;
  const int kNumTests = 50;
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
    const std::vector<int> destinations = {num_nodes - 1};
    const std::vector<double> max_resources = {15.0};
    ConstrainedShortestPathsOnDagWrapper<util::StaticGraph<>>
        constrained_shortest_path_on_dag(&graph, &arc_lengths, &arc_resources,
                                         topological_order, sources,
                                         destinations, &max_resources);
    const int kNumSamples = 5;
    for (int _ = 0; _ < kNumSamples; ++_) {
      arc_lengths = GenerateRandomIntegerValues(graph);
      const GraphPathWithLength<util::StaticGraph<>> path_with_length =
          constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();

      EXPECT_NEAR(path_with_length.length,
                  SolveConstrainedShortestPathUsingIntegerProgramming(
                      graph, arc_lengths, arc_resources, max_resources, sources,
                      destinations),
                  1e-5);

      ASSERT_FALSE(HasFailure())
          << DUMP_VARS(num_nodes, num_arcs, arc_lengths) << "\n With graph :\n "
          << util::GraphToString(graph, util::PRINT_GRAPH_ARCS);
    }
  }
}

// -----------------------------------------------------------------------------
// Benchmark.
// -----------------------------------------------------------------------------
template <typename NodeIndex = int32_t, typename ArcIndex = int32_t>
void BM_RandomDag(benchmark::State& state) {
  absl::BitGen bit_gen;
  // Generate a fixed random DAG.
  const NodeIndex num_nodes = state.range(0);
  const ArcIndex num_arcs = num_nodes * state.range(1);
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
  const std::vector<NodeIndex> sources = {0};
  const std::vector<NodeIndex> destinations = {num_nodes - 1};
  const std::vector<double> max_resources = {num_nodes * 0.2};
  ConstrainedShortestPathsOnDagWrapper<util::StaticGraph<NodeIndex, ArcIndex>>
      constrained_shortest_path_on_dag(&graph, &arc_lengths, &arc_resources,
                                       topological_order, sources, destinations,
                                       &max_resources);

  int total_label_count = 0;
  for (auto _ : state) {
    // Pick a arc lengths scenario at random.
    arc_lengths =
        arc_lengths_scenarios[absl::Uniform(bit_gen, 0, num_scenarios)];
    const GraphPathWithLength<util::StaticGraph<NodeIndex, ArcIndex>>
        path_with_length =
            constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();
    total_label_count += constrained_shortest_path_on_dag.label_count();
    CHECK_GE(path_with_length.length, 0.0);
    CHECK_LE(path_with_length.length, 10000.0);
  }
  state.SetItemsProcessed(std::max(1, total_label_count));
}

BENCHMARK(BM_RandomDag<int32_t, int32_t>)
    ->ArgPair(1 << 10, 16)
    ->ArgPair(1 << 16, 4)
    ->ArgPair(1 << 16, 16)
    ->ArgPair(1 << 19, 4);
BENCHMARK(BM_RandomDag<int64_t, int64_t>)
    ->ArgPair(1 << 19, 16)
    ->ArgPair(1 << 22, 4);

// Generate a 2-dimensional grid DAG.
// Eg. for width=3, height=2, it generates this:
// 0 ----> 1 ----> 2
// |       |       |
// |       |       |
// v       v       v
// 3 ----> 4 ----> 5
void BM_GridDAG(benchmark::State& state) {
  const int64_t width = state.range(0);
  const int64_t height = state.range(1);
  const int num_resources = state.range(2);
  const int num_nodes = width * height;
  const int num_arcs = 2 * num_nodes - width - height;
  util::StaticGraph<> graph(num_nodes, num_arcs);
  // Add horizontal edges.
  for (int i = 0; i < height; ++i) {
    for (int j = 1; j < width; ++j) {
      const int left = i * width + (j - 1);
      const int right = i * width + j;
      graph.AddArc(left, right);
    }
  }
  // Add vertical edges.
  for (int i = 1; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const int up = (i - 1) * width + j;
      const int down = i * width + j;
      graph.AddArc(up, down);
    }
  }
  graph.Build();
  std::vector<int> topological_order(num_nodes);
  absl::c_iota(topological_order, 0);

  // Generate 20 scenarios of random arc lengths.
  absl::BitGen bit_gen;
  const int kNumScenarios = 20;
  std::vector<std::vector<double>> arc_lengths_scenarios;
  for (int unused = 0; unused < kNumScenarios; ++unused) {
    std::vector<double> arc_lengths(graph.num_arcs());
    for (int i = 0; i < graph.num_arcs(); ++i) {
      arc_lengths[i] = absl::Uniform<double>(bit_gen, 0, 1);
    }
    arc_lengths_scenarios.push_back(arc_lengths);
  }

  std::vector<std::vector<double>> arc_resources(num_resources);
  for (int r = 0; r < num_resources; ++r) {
    arc_resources[r].resize(graph.num_arcs());
    for (int i = 0; i < graph.num_arcs(); ++i) {
      arc_resources[r][i] = absl::Uniform<double>(bit_gen, 0, 1);
    }
  }

  std::vector<double> arc_lengths(num_arcs);
  const std::vector<int> sources = {0};
  const std::vector<int> destinations = {num_nodes - 1};
  std::vector<double> max_resources(num_resources);
  // Each path from source to destination has `(width + height - 2)` arcs. Each
  // arc has mean resource(s) 0.5. We want to consider paths with half (0.5) the
  // mean resource(s).
  const double max_resource = (width + height - 2) * 0.5 * 0.5;
  for (int r = 0; r < num_resources; ++r) {
    max_resources[r] = max_resource;
  }

  ConstrainedShortestPathsOnDagWrapper<util::StaticGraph<>>
      constrained_shortest_path_on_dag(&graph, &arc_lengths, &arc_resources,
                                       topological_order, sources, destinations,
                                       &max_resources);

  int total_label_count = 0;
  for (auto _ : state) {
    // Pick a arc lengths scenario at random.
    arc_lengths =
        arc_lengths_scenarios[absl::Uniform<int>(bit_gen, 0, kNumScenarios)];
    const GraphPathWithLength<util::StaticGraph<>> path_with_length =
        constrained_shortest_path_on_dag.RunConstrainedShortestPathOnDag();
    total_label_count += constrained_shortest_path_on_dag.label_count();
    CHECK_GE(path_with_length.length, 0.0);
  }
  state.SetItemsProcessed(std::max(1, total_label_count));
}

BENCHMARK(BM_GridDAG)
    ->Args({100, 100, 1})
    ->Args({100, 100, 2})
    ->Args({1000, 100, 1})
    ->Args({1000, 100, 2});

// -----------------------------------------------------------------------------
// Debug tests.
// -----------------------------------------------------------------------------
#ifndef NDEBUG
TEST(ConstrainedShortestPathOnDagTest, MinusInfWeight) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, destination, -kInf, {0.0}}};

  EXPECT_DEATH(ConstrainedShortestPathsOnDag(
                   num_nodes, arcs_with_length_and_resources, source,
                   destination, /*max_resources=*/{0.0}),
               "-inf");
}

TEST(ConstrainedShortestPathOnDagTest, NaNWeight) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, destination, std::numeric_limits<double>::quiet_NaN(), {0.0}}};

  EXPECT_DEATH(ConstrainedShortestPathsOnDag(
                   num_nodes, arcs_with_length_and_resources, source,
                   destination, /*max_resources=*/{0.0}),
               "NaN");
}

TEST(ConstrainedShortestPathOnDagTest, InfResource) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, destination, 0.0, {kInf}}};

  EXPECT_DEATH(ConstrainedShortestPathsOnDag(
                   num_nodes, arcs_with_length_and_resources, source,
                   destination, /*max_resources=*/{0.0}),
               "inf");
}

TEST(ConstrainedShortestPathOnDagTest, NegativeMaxResource) {
  const int source = 0;
  const int destination = 1;
  const int num_nodes = 2;
  const std::vector<ArcWithLengthAndResources> arcs_with_length_and_resources =
      {{source, destination, 0.0, {0.0}}};

  EXPECT_DEATH(ConstrainedShortestPathsOnDag(
                   num_nodes, arcs_with_length_and_resources, source,
                   destination, /*max_resources=*/{-1.0}),
               "negative");
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
                   &graph, &arc_lengths, &arc_resources, topological_order,
                   sources, destinations, &max_resources),
               "Invalid topological order");
}
#endif  // NDEBUG

}  // namespace
}  // namespace operations_research
