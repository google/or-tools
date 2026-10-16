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

#include "ortools/graph/util.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/io.h"
#include "ortools/graph/random_graph.h"

namespace util {
namespace {

using testing::AnyOf;
using testing::ElementsAre;
using testing::UnorderedElementsAre;
using util::PRINT_GRAPH_ADJACENCY_LISTS_SORTED;
using util::PRINT_GRAPH_ARCS;

TEST(GraphDiagnosisTest, Empty) {
  ListGraph<> graph;
  EXPECT_FALSE(GraphHasSelfArcs(graph));
  EXPECT_FALSE(GraphHasDuplicateArcs(graph));
  EXPECT_TRUE(GraphIsSymmetric(graph));
  EXPECT_TRUE(GraphIsWeaklyConnected(graph));
}

TEST(GraphDiagnosisTest, NoArcs) {
  const auto graph =
      StaticGraph<>::Builder(/*num_nodes=*/1047, /*arc_capacity=*/0)
          .Build(nullptr);
  EXPECT_FALSE(GraphHasSelfArcs(*graph));
  EXPECT_FALSE(GraphHasDuplicateArcs(*graph));
  EXPECT_TRUE(GraphIsSymmetric(*graph));
  EXPECT_FALSE(GraphIsWeaklyConnected(*graph));
}

TEST(GraphDiagnosisTest, ZeroZero) {
  ListGraph<> graph;
  graph.AddArc(0, 0);
  EXPECT_TRUE(GraphHasSelfArcs(graph));
  EXPECT_FALSE(GraphHasDuplicateArcs(graph));
  EXPECT_TRUE(GraphIsSymmetric(graph));
  EXPECT_TRUE(GraphIsWeaklyConnected(graph));

  graph.AddArc(0, 0);
  EXPECT_TRUE(GraphHasSelfArcs(graph));
  EXPECT_TRUE(GraphHasDuplicateArcs(graph));
  EXPECT_TRUE(GraphIsSymmetric(graph));
  EXPECT_TRUE(GraphIsWeaklyConnected(graph));
}

TEST(GraphDiagnosisTest, SomeSimpleCases) {
  ListGraph<> graph;

  graph.AddArc(1, 2);
  graph.AddArc(2, 1);
  EXPECT_FALSE(GraphHasSelfArcs(graph));
  EXPECT_FALSE(GraphHasDuplicateArcs(graph));
  EXPECT_TRUE(GraphIsSymmetric(graph));
  EXPECT_FALSE(GraphIsWeaklyConnected(graph));  // '0' is disconnected.

  graph.AddArc(1, 1);
  graph.AddArc(1, 2);
  graph.AddArc(2, 1);
  EXPECT_TRUE(GraphHasSelfArcs(graph));
  EXPECT_TRUE(GraphHasDuplicateArcs(graph));
  EXPECT_TRUE(GraphIsSymmetric(graph));
  EXPECT_FALSE(GraphIsWeaklyConnected(graph));
}

TEST(GraphDiagnosisTest, RandomizedStressTestForSelfAndDuplicateArcs) {
  auto random = std::mt19937(1234);
  const int num_tests = DEBUG_MODE ? 3000 : 50000;
  for (int test = 0; test < num_tests; ++test) {
    const int num_nodes = absl::Uniform(random, 0, 20);
    // Half tests for directed and the other half for undirected graphs.
    const bool directed = test < num_tests / 2;
    const int num_arcs_or_edges = absl::Uniform(
        random, 0, num_nodes * (num_nodes - 1) / (directed ? 1 : 2) + 1);
    // GraphHasSelfArcs() shouldn't need the graph to be finalized.
    StaticGraph<>::Builder builder =
        directed ? GenerateRandomDirectedSimpleGraph(num_nodes,
                                                     num_arcs_or_edges, random)
                 : GenerateRandomUndirectedSimpleGraph(
                       num_nodes, num_arcs_or_edges, random);
    ASSERT_EQ(num_nodes, builder.num_nodes());
    ASSERT_EQ(num_arcs_or_edges * (directed ? 1 : 2), builder.num_arcs());
    ASSERT_FALSE(
        GraphHasSelfArcs(*StaticGraph<>::Builder(builder).Build(nullptr)));

    // Add a random self-arc.
    const int node = absl::Uniform(random, 0, num_nodes);
    builder.AddArc(node, node);
    const auto graph_with_random_self_arc =
        StaticGraph<>::Builder(builder).Build(nullptr);
    ASSERT_TRUE(GraphHasSelfArcs(*graph_with_random_self_arc));

    // Add a random duplicated arc (could be the self arc).
    const int arc =
        absl::Uniform<int>(random, 0, graph_with_random_self_arc->num_arcs());
    builder.AddArc(graph_with_random_self_arc->Tail(arc),
                   graph_with_random_self_arc->Head(arc));
    const auto graph_with_random_duplicate_arc =
        std::move(builder).Build(nullptr);
    ASSERT_TRUE(GraphHasSelfArcs(*graph_with_random_duplicate_arc));
    ASSERT_TRUE(GraphHasDuplicateArcs(*graph_with_random_duplicate_arc));

    // Tests whether RemoveSelfArcsAndDuplicateArcs() works well.
    const std::unique_ptr<StaticGraph<>> cleaned_graph =
        RemoveSelfArcsAndDuplicateArcs(*graph_with_random_duplicate_arc);
    ASSERT_EQ(cleaned_graph->num_nodes(),
              graph_with_random_duplicate_arc->num_nodes());
    ASSERT_EQ(cleaned_graph->num_arcs(),
              graph_with_random_duplicate_arc->num_arcs() - 2);
    ASSERT_FALSE(GraphHasSelfArcs(*cleaned_graph));
    ASSERT_FALSE(GraphHasDuplicateArcs(*cleaned_graph));
  }
}

TEST(GraphIsSymmetricTest, SimpleSymmetric) {
  StaticGraph<>::Builder builder(/*num_nodes=*/3, /*arc_capacity=*/3);
  builder.AddArc(2, 1);
  builder.AddArc(1, 1);
  builder.AddArc(1, 2);
  const auto graph = std::move(builder).Build(nullptr);
  EXPECT_TRUE(GraphIsSymmetric(*graph));
}

TEST(GraphIsSymmetricTest, SimpleAsymmetric) {
  StaticGraph<>::Builder builder(/*num_nodes=*/3, /*arc_capacity=*/3);
  builder.AddArc(2, 1);
  builder.AddArc(1, 1);
  builder.AddArc(2, 1);
  const auto graph = std::move(builder).Build(nullptr);
  EXPECT_FALSE(GraphIsSymmetric(*graph));
}

TEST(GraphIsSymmetricTest, MultiArcsSymmetric) {
  StaticGraph<>::Builder builder(/*num_nodes=*/3, /*arc_capacity=*/8);
  builder.AddArc(2, 1);
  builder.AddArc(1, 2);
  builder.AddArc(2, 1);
  builder.AddArc(1, 2);
  builder.AddArc(1, 2);
  builder.AddArc(1, 1);
  builder.AddArc(2, 1);
  builder.AddArc(1, 1);
  const auto graph = std::move(builder).Build(nullptr);
  EXPECT_TRUE(GraphIsSymmetric(*graph));
}

TEST(GraphIsSymmetricTest, MultiArcsAsymmetric) {
  StaticGraph<>::Builder builder(/*num_nodes=*/3, /*arc_capacity=*/6);
  builder.AddArc(2, 1);
  builder.AddArc(1, 2);
  builder.AddArc(2, 1);
  builder.AddArc(1, 2);
  builder.AddArc(1, 2);
  builder.AddArc(1, 2);
  const auto graph = std::move(builder).Build(nullptr);
  EXPECT_FALSE(GraphIsSymmetric(*graph));
}

TEST(GraphIsSymmetricTest, StressTest) {
  std::mt19937 random(1234);
  const int kMaxSize = DEBUG_MODE ? 1000000 : 100000000;
  for (int n = 3; n < kMaxSize; n *= 2) {
    const int edge_range_width =
        std::min(n, absl::Bernoulli(random, 1.0 / 2) ? 100 : n);
    const int num_edges = absl::Bernoulli(random, 1.0 / 2) ? 1000 : n;
    std::vector<std::pair<int, int>> arcs;
    arcs.reserve(num_edges * 2);
    for (int i = 0; i < num_edges; ++i) {
      const int from =
          absl::Uniform(random, 0, edge_range_width) + n - edge_range_width;
      const int to =
          absl::Uniform(random, 0, edge_range_width) + n - edge_range_width;
      arcs.emplace_back(from, to);
      arcs.emplace_back(to, from);
    }
    std::shuffle(arcs.begin(), arcs.end(), random);
    const bool expect_symmetric = absl::Bernoulli(random, 1.0 / 2);
    if (!expect_symmetric) {
      while (arcs.back().first == arcs.back().second) arcs.pop_back();
      arcs.pop_back();
    }

    StaticGraph<>::Builder builder(n, num_edges * 2);
    for (const std::pair<int, int>& fromto : arcs) {
      builder.AddArc(fromto.first, fromto.second);
    }
    const auto graph = std::move(builder).Build(nullptr);
    ASSERT_EQ(expect_symmetric, GraphIsSymmetric(*graph))
        << GraphToString(*graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED);
  }
}

TEST(CopyGraphTest, ThoroughRandomizedTest) {
  typedef StaticGraph<> Graph;
  // Timing as of 2016-10-10 (forge, fastbuild): about 70ms.
  const int kNumTests = 1000;
  const int kMaxNumNodes = 10;
  const int kMaxNumArcs = 100;
  auto random = std::mt19937(1234);
  for (int test = 0; test < kNumTests; ++test) {
    const int num_nodes = absl::Uniform(random, 0, kMaxNumNodes);
    const int num_arcs =
        num_nodes == 0 ? 0 : absl::Uniform(random, 0, kMaxNumArcs);
    std::unique_ptr<Graph> graph =
        GenerateRandomMultiGraph(num_nodes, num_arcs, random).Build(nullptr);
    std::unique_ptr<Graph> graph_copy = CopyGraph(*graph);
    ASSERT_EQ(GraphToString(*graph_copy, PRINT_GRAPH_ADJACENCY_LISTS_SORTED),
              GraphToString(*graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
  }
}

TEST(RemapGraphTest, SimpleRemap) {
  typedef StaticGraph<> Graph;
  Graph::Builder builder;
  builder.AddArc(4, 2);
  builder.AddArc(1, 3);
  builder.AddArc(3, 1);
  builder.AddArc(2, 2);
  builder.AddArc(1, 3);
  const auto graph = std::move(builder).Build(nullptr);
  std::unique_ptr<Graph> new_graph = RemapGraph(*graph, {3, 1, 0, 4, 2});
  EXPECT_EQ(
      "0: 0\n"    // Former node 2
      "1: 4 4\n"  // Former node 1
      "2: 0\n"    // Former node 4
      "3: \n"     // Former node 0
      "4: 1",     // Former node 3
      GraphToString(*new_graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
}

TEST(RemapGraphTest, EmptyGraph) {
  typedef StaticGraph<> Graph;
  Graph::Builder builder(/*num_nodes=*/0, /*arc_capacity=*/0);
  const auto graph = std::move(builder).Build(nullptr);
  std::unique_ptr<Graph> new_graph = RemapGraph(*graph, {});
  EXPECT_EQ("", GraphToString(*new_graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
}

TEST(RemapGraphTestDeath, PermutationWithInvalidSize) {
  typedef StaticGraph<> Graph;
  Graph::Builder builder(/*num_nodes=*/345, /*arc_capacity=*/0);
  const auto graph = std::move(builder).Build(nullptr);
  EXPECT_DEATH(RemapGraph(*graph, {}), "345");
}

TEST(RemapGraphDeathTest, InvalidPermutationDiesInDebugMode) {
  if (!DEBUG_MODE) return;
  typedef StaticGraph<> Graph;
  Graph::Builder builder(/*num_nodes=*/3, /*arc_capacity*/ 0);
  const auto graph = std::move(builder).Build(nullptr);
  EXPECT_DEATH(RemapGraph(*graph, {0, 1, 1}), "Invalid permutation");
}

TEST(GetSubgraphOfNodesTest, Simple) {
  typedef StaticGraph<> Graph;
  Graph::Builder builder;
  builder.AddArc(4, 2);
  builder.AddArc(1, 3);
  builder.AddArc(3, 1);
  builder.AddArc(2, 2);
  builder.AddArc(1, 3);
  const auto graph = std::move(builder).Build(nullptr);
  std::unique_ptr<Graph> new_graph = GetSubgraphOfNodes(*graph, {3, 0, 1});
  EXPECT_EQ(
      "0: 2\n"   // Former node 3
      "1: \n"    // Former node 0
      "2: 0 0",  // Former node 1
      GraphToString(*new_graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
}

TEST(GetSubgraphOfNodesTest, EmptyGraph) {
  typedef StaticGraph<> Graph;
  const auto graph =
      Graph::Builder(/*num_nodes=*/0, /*arc_capacity=*/0).Build(nullptr);
  std::unique_ptr<Graph> new_graph = GetSubgraphOfNodes(*graph, {});
  EXPECT_EQ("", GraphToString(*new_graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
}

TEST(GetSubgraphOfNodesDeathTest, InvalidPermutationDiesInDebugMode) {
  if (!DEBUG_MODE) return;
  const auto graph = StaticGraph<>::Builder(/*num_nodes=*/3, /*arc_capacity*/ 0)
                         .Build(nullptr);
  EXPECT_DEATH(GetSubgraphOfNodes(*graph, {0, 1, 1}), "Invalid subset");
}

TEST(GetSubgraphOfNodesTest, ExtractingSmallSubgraphUsesLittleMemory) {
  // Build a relatively big graph (~10M). Two nodes A and B will have only a few
  // edges, and the rest of the nodes will form a sub-clique.
  typedef StaticGraph<> Graph;
  const int kNumNodes = 1000;
  const int A = 345;
  const int B = 764;
  Graph::Builder builder(/*num_nodes=*/kNumNodes,
                         /*arc_capacity=*/kNumNodes * kNumNodes);
  // Arcs adjacent to A or B.
  builder.AddArc(A, B);
  builder.AddArc(A, 111);
  builder.AddArc(111, B);
  builder.AddArc(A, B);
  builder.AddArc(222, A);
  builder.AddArc(B, A);
  builder.AddArc(A, 222);
  // All other arcs.
  for (int i = 0; i < kNumNodes; ++i) {
    if (i == A || i == B) continue;
    for (int j = 0; j < kNumNodes; ++j) {
      if (j != A && j != B) builder.AddArc(i, j);
    }
  }
  const auto graph = std::move(builder).Build(nullptr);
  // Extract the subgraph so many times that if it was taking as much RAM as
  // the original graph, it would OOM the unit test.
  const int kNumExtractions = 100000;  // 100000 * 10M ~= 1TB.
  std::vector<std::unique_ptr<Graph>> graphs;
  for (int i = 0; i < kNumExtractions; ++i) {
    graphs.push_back(GetSubgraphOfNodes(*graph, {A, B}));
  }
  // Do something with "graphs" to prevent it being optimized away.
  CHECK_EQ(GraphToString(*graphs[0], PRINT_GRAPH_ADJACENCY_LISTS_SORTED),
           GraphToString(*graphs.back(), PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
}

TEST(IsSubsetOf0NTest, Thorough) {
  EXPECT_TRUE(IsSubsetOf0N({}, 0));
  EXPECT_TRUE(IsSubsetOf0N({}, 1));
  EXPECT_TRUE(IsSubsetOf0N({}, 1219084124));
  EXPECT_TRUE(IsSubsetOf0N({0, 1, 2, 3}, 4));
  EXPECT_TRUE(IsSubsetOf0N({0}, 1));
  EXPECT_TRUE(IsSubsetOf0N({0, 5}, 6));
  EXPECT_TRUE(IsSubsetOf0N({3, 4}, 5));
  EXPECT_TRUE(IsSubsetOf0N({0, 3, 5}, 6));
  EXPECT_TRUE(IsSubsetOf0N({1, 0, 2, 4, 3}, 5));
  EXPECT_FALSE(IsSubsetOf0N({0, 1, 1}, 3));
  EXPECT_FALSE(IsSubsetOf0N({0, -1, 1}, 3));
  EXPECT_FALSE(IsSubsetOf0N({0, 1, 3}, 3));
  EXPECT_FALSE(IsSubsetOf0N({1}, 0));
  EXPECT_FALSE(IsSubsetOf0N({-1}, 1000));
  EXPECT_FALSE(IsSubsetOf0N({0, 5}, 5));
}

TEST(IsValidPermutationTest, Thorough) {
  EXPECT_TRUE(IsValidPermutation({}));
  EXPECT_TRUE(IsValidPermutation({0, 1, 2, 3}));
  EXPECT_TRUE(IsValidPermutation({0}));
  EXPECT_TRUE(IsValidPermutation({3, 4, 0, 2, 1}));
  EXPECT_TRUE(IsValidPermutation({1, 0, 2, 4, 3}));
  EXPECT_FALSE(IsValidPermutation({0, 1, 1}));
  EXPECT_FALSE(IsValidPermutation({0, -1, 1}));
  EXPECT_FALSE(IsValidPermutation({0, 1, 3}));
}

TEST(DuplicateArcsTest, EmptyGraph) {
  typedef StaticGraph<> Graph;
  const auto graph = Graph::Builder(0, 0).Build(nullptr);
  EXPECT_FALSE(GraphHasDuplicateArcs(*graph));
  std::unique_ptr<Graph> graph2 = RemoveSelfArcsAndDuplicateArcs(*graph);
  EXPECT_EQ(0, graph2->num_nodes());
  EXPECT_EQ(0, graph2->num_arcs());
}

TEST(DuplicateArcsTest, SmallGraph) {
  typedef StaticGraph<> Graph;
  Graph::Builder builder;
  builder.AddArc(1, 3);
  builder.AddArc(3, 1);
  builder.AddArc(3, 1);
  builder.AddArc(2, 4);
  builder.AddArc(5, 5);
  builder.AddArc(5, 5);
  builder.AddArc(1, 3);
  builder.AddArc(5, 5);
  builder.AddArc(1, 3);
  builder.AddArc(4, 4);
  const auto graph = std::move(builder).Build(nullptr);
  EXPECT_TRUE(GraphHasDuplicateArcs(*graph));
  std::unique_ptr<Graph> graph2 = RemoveSelfArcsAndDuplicateArcs(*graph);
  EXPECT_EQ(6, graph2->num_nodes());
  EXPECT_EQ("1->3\n2->4\n3->1", GraphToString(*graph2, PRINT_GRAPH_ARCS));
  EXPECT_FALSE(GraphHasDuplicateArcs(*graph2));
}

// Construct a graph from a single "node path" with the simple API needed by
// RemoveCyclesFromPath().
class PathGraph {
 public:
  // The arc with index i will be the arc from the node node_path[i] to the node
  // node_path[i + 1]. The number of arcs will be one less that the number of
  // points in the given node_path.
  explicit PathGraph(absl::Span<const int> node_path) {
    for (int i = 1; i < node_path.size(); ++i) {
      tail_.push_back(node_path[i - 1]);
      head_.push_back(node_path[i]);
    }
  }

  int Head(int arc) const { return head_[arc]; }
  int Tail(int arc) const { return tail_[arc]; }

  // Returns the list of all the arcs of this graph in order.
  std::vector<int> AllArcs() const {
    std::vector<int> result(head_.size(), 0);
    std::iota(result.begin(), result.end(), 0);
    return result;
  }

 private:
  std::vector<int> head_;
  std::vector<int> tail_;
};

TEST(RemoveCyclesFromPathTest, EmptyPath) {
  PathGraph graph({});
  std::vector<int> path = graph.AllArcs();
  RemoveCyclesFromPath(graph, &path);
  EXPECT_TRUE(path.empty());
}

TEST(RemoveCyclesFromPathTest, NothingHappenIfNoCycle) {
  PathGraph graph({0, 1, 2, 3, 4, 5});
  std::vector<int> path = graph.AllArcs();
  RemoveCyclesFromPath(graph, &path);
  EXPECT_THAT(path, ::testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST(RemoveCyclesFromPathTest, BackAndForthIsRemoved) {
  PathGraph graph({0, 1, 2, 3, 4, 5, 4, 3, 10});
  std::vector<int> path = graph.AllArcs();
  RemoveCyclesFromPath(graph, &path);
  EXPECT_THAT(path, ::testing::ElementsAre(0, 1, 2, 7));
}

TEST(RemoveCyclesFromPathTest, FullCycleLeadToEmptyPath) {
  PathGraph graph({0, 1, 2, 3, 4, 5, 0});
  std::vector<int> path = graph.AllArcs();
  RemoveCyclesFromPath(graph, &path);
  EXPECT_TRUE(path.empty());
}

// The second cycle arrives in the middle of the first, in which cases
// there is actually two possible return values.
TEST(RemoveCyclesFromPathTest, MultipleCycles) {
  //      /--------\
  //      |        | <----\
  // -----|--------/      |
  //      \---------------/
  PathGraph graph({0, 1, 2, 3, 4, 5, 6, 3, 8, 5});
  std::vector<int> path = graph.AllArcs();
  RemoveCyclesFromPath(graph, &path);
  EXPECT_THAT(path, ::testing::ElementsAre(0, 1, 2, 7, 8));
}

TEST(PathHasCycleTest, EmptyPath) {
  PathGraph graph({});
  EXPECT_FALSE(PathHasCycle(graph, graph.AllArcs()));
}

TEST(PathHasCycleTest, NoCycle) {
  PathGraph graph({0, 1, 2, 3, 4, 5});
  EXPECT_FALSE(PathHasCycle(graph, graph.AllArcs()));
}

TEST(PathHasCycleTest, Cycle) {
  PathGraph graph({0, 1, 2, 3, 4, 5, 2, 6});
  EXPECT_TRUE(PathHasCycle(graph, graph.AllArcs()));
}

TEST(PathHasCycleTest, FullCycle) {
  PathGraph graph({0, 1, 2, 3, 0});
  EXPECT_TRUE(PathHasCycle(graph, graph.AllArcs()));
}

TEST(PathHasCycleTest, SelfLoop) {
  PathGraph graph({0, 0});
  EXPECT_TRUE(PathHasCycle(graph, graph.AllArcs()));
}

TEST(ComputeOnePossibleReverseArcMappingTest, EmptyGraph) {
  EXPECT_THAT(
      ComputeOnePossibleReverseArcMapping(util::ListGraph<>(),
                                          /*die_if_not_symmetric=*/true),
      ElementsAre());
}

TEST(ComputeOnePossibleReverseArcMappingTest, OneArcGraph) {
  util::ListGraph<> graph;
  graph.AddArc(3, 1);
  EXPECT_THAT(ComputeOnePossibleReverseArcMapping(
                  graph, /*die_if_not_symmetric=*/false),
              ElementsAre(-1));
}

TEST(ComputeOnePossibleReverseArcMappingDeathTest, OneArcGraphSymmetryCheck) {
  util::ListGraph<> graph;
  graph.AddArc(3, 1);
  EXPECT_DEATH(
      ComputeOnePossibleReverseArcMapping(graph, /*die_if_not_symmetric=*/true),
      "not symmetric");
}

TEST(ComputeOnePossibleReverseArcMappingDeathTest, SmallAsymmetricCase) {
  // Using ListGraph so that the final arc ordering is the one defined by the
  // AddArc() sequence.
  util::ListGraph<> graph;
  graph.AddArc(3, 1);
  graph.AddArc(3, 1);
  graph.AddArc(1, 3);
  graph.AddArc(2, 0);
  EXPECT_THAT(ComputeOnePossibleReverseArcMapping(
                  graph, /*die_if_not_symmetric=*/false),
              AnyOf(ElementsAre(2, -1, 0, -1), ElementsAre(-1, 2, 1, -1)));
}

TEST(ComputeOnePossibleReverseArcMappingTest, RandomizedStressTest) {
  std::mt19937 random(1234);
  // Timing as of 2017-09-22 (forge): 0.6s in fastbuild, 1.3s in opt.
  for (const bool symmetric : {false, true}) {
    for (const int num_nodes : {1, 10, 100000}) {
      for (const int num_arcs_or_edges :
           {1, 101, DEBUG_MODE ? 10000 : 100000}) {
        SCOPED_TRACE(absl::StrCat(
            symmetric ? "Symmetric" : "Not symmetric", ", #Nodes=", num_nodes,
            ", #", symmetric ? "Edges" : "Arcs", num_arcs_or_edges));
        const int num_arcs =
            symmetric ? num_arcs_or_edges * 2 : num_arcs_or_edges;
        util::StaticGraph<>::Builder builder(num_nodes, num_arcs);
        for (int i = 0; i < num_arcs_or_edges; ++i) {
          const int a = absl::Uniform(random, 0, num_nodes);
          const int b = absl::Uniform(random, 0, num_nodes);
          builder.AddArc(a, b);
          if (symmetric) builder.AddArc(b, a);
        }
        const auto graph = std::move(builder).Build(nullptr);
        const std::vector<int> reverse_arc =
            ComputeOnePossibleReverseArcMapping(
                *graph, /*die_if_not_symmetric=*/symmetric);

        // Verify that the mapping is reflexive and correct. Note that
        // reflexivity implies bijectivity on the domain set.
        ASSERT_EQ(reverse_arc.size(), graph->num_arcs());
        for (int arc = 0; arc < graph->num_arcs(); ++arc) {
          const int rev_arc = reverse_arc[arc];
          if (rev_arc == -1) {
            ASSERT_FALSE(symmetric)
                << "Symmetric graph yielded some unassigned reverse_arc!";
          } else {
            SCOPED_TRACE(absl::StrCat("When testing Arc #", arc, "(",
                                      graph->Tail(arc), "->", graph->Head(arc),
                                      ")"));
            ASSERT_GE(rev_arc, 0);
            ASSERT_LT(rev_arc, graph->num_arcs());
            ASSERT_EQ(graph->Head(arc), graph->Tail(rev_arc))
                << "Reverse Arc #" << rev_arc << " (" << graph->Tail(rev_arc)
                << "->" << graph->Head(rev_arc) << ").";
            ASSERT_EQ(reverse_arc[rev_arc], arc)
                << "Reverse Reverse Arc: " << graph->Tail(reverse_arc[rev_arc])
                << "->" << graph->Head(reverse_arc[rev_arc]) << ").";
          }
          if (graph->Tail(arc) == graph->Head(arc)) {
            ASSERT_EQ(rev_arc, arc) << "Self-arc should be mapped to itself.";
          }
        }

        // TODO(user): for the asymmetric case, add a unit test that
        // verifies that all arcs that have a reverse are indeed mapped:
        // the current stress test would accept an 'empty' mapping where
        // all arcs are mapped to -1.
      }
    }
  }
}

TEST(UndirectedAdjacencyListsOfDirectedGraphTest, Simple) {
  ReverseArcListGraph<> graph(4, 3);
  graph.AddArc(0, 1);
  graph.AddArc(1, 2);
  graph.AddArc(2, 2);
  UndirectedAdjacencyListsOfDirectedGraph ugraph(graph);

  static_assert(std::input_iterator<decltype(ugraph[0].begin())>);
}

TEST(GetWeaklyConnectedComponentsTest, Basic) {
  // The two components of GetWeaklyConnectedComponents() are already well
  // tested: GetConnectedComponents() and UndirectedViewOfDirectedGraph().
  // So we just test the "plumbing" here.
  ReverseArcListGraph<> graph;
  graph.AddArc(0, 1);
  graph.AddArc(1, 2);
  graph.AddArc(2, 1);
  graph.AddArc(3, 4);
  EXPECT_THAT(GetWeaklyConnectedComponents(graph), ElementsAre(0, 0, 0, 1, 1));
}

}  // namespace
}  // namespace util
