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

#include "ortools/graph_base/generic_graph.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"

namespace util::graph {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::SizeIs;

TEST(GenericGraphTest, EmptyGraph) {
  const auto graph = GenericGraphBuilder<std::string>().Build();
  EXPECT_EQ(graph.Graph().num_nodes(), 0);
  EXPECT_EQ(graph.Graph().num_arcs(), 0);
  EXPECT_THAT(graph.Nodes({}), IsEmpty());
  EXPECT_EQ(graph.LookupNode("foo"), std::nullopt);
}

TEST(GenericGraphTest, EmptyGraphFromEdges) {
  const auto graph = GenericGraph<std::string>::FromEdges({});
  const auto& g = graph.Graph();
  EXPECT_EQ(g.num_nodes(), 0);
  EXPECT_EQ(g.num_arcs(), 0);
  EXPECT_THAT(graph.Nodes({}), IsEmpty());
}

TEST(GenericGraphTest, SimpleGraphIncrementalConstruction) {
  GenericGraphBuilder<std::string> builder;
  builder.AddEdge("b", "a");
  builder.AddEdge("b", "c");
  EXPECT_THAT(builder.AllNodes(), ElementsAre("b", "a", "c"));
  EXPECT_EQ(builder.LookupOrAddNode(""), 3);
  builder.AddEdge("d", "d");
  builder.AddEdge("c", "b");
  builder.AddEdge("b", "c");
  builder.AddEdge("b", "c");
  EXPECT_EQ(builder.LookupOrAddNode("d"), 4);
  EXPECT_EQ(builder.LookupOrAddNode("b"), 0);

  const auto graph = std::move(builder).Build();
  EXPECT_EQ(graph.Graph().num_nodes(), 5);
  EXPECT_EQ(graph.Graph().num_arcs(), 6);
  EXPECT_EQ(graph.Node(0), "b");
  EXPECT_THAT(graph.Nodes(std::vector<int>{4, 3, 2, 1, 0}),
              ElementsAre("d", "", "c", "a", "b"));
  EXPECT_THAT(graph.LookupNode("d"), Optional(4));
  EXPECT_EQ(graph.LookupNode("baz"), std::nullopt);
}

TEST(GenericGraphTest, PreReservedIncrementalConstruction) {
  GenericGraphBuilder<std::string> builder(/*num_nodes=*/2, /*num_edges=*/1);
  builder.LookupOrAddNode("X");
  builder.AddEdge("Y", "Y");
  const auto graph = std::move(builder).Build();
  EXPECT_EQ(graph.Graph().num_nodes(), 2);
  EXPECT_EQ(graph.Graph().num_arcs(), 1);
  EXPECT_EQ(graph.Node(0), "X");
  EXPECT_EQ(graph.Node(1), "Y");
}

TEST(GenericGraphTest, FromEdgesAndReaderFunctions) {
  const auto graph =
      GenericGraph<std::string>::FromEdges({{"b", "a"}, {"b", "c"}});
  EXPECT_EQ(graph.Graph().num_nodes(), 3);
  EXPECT_EQ(graph.Graph().num_arcs(), 2);
  EXPECT_EQ(graph.Node(0), "b");
  EXPECT_THAT(graph.Nodes({0, 1, 2}), ElementsAre("b", "a", "c"));

  const auto& g = graph.Graph();
  EXPECT_EQ(g.num_nodes(), 3);
  EXPECT_EQ(g.num_arcs(), 2);
  std::vector<std::pair<int, int>> arcs;
  for (int arc : g.AllForwardArcs()) {
    arcs.push_back(std::make_pair(g.Tail(arc), g.Head(arc)));
  }
  EXPECT_THAT(arcs, ElementsAre(Pair(0, 1), Pair(0, 2)));
}

TEST(GenericGraphTest, BuilderAndGraphAreMovable) {
  GenericGraphBuilder<int> builder1;
  builder1.AddEdge(1, 2);
  GenericGraphBuilder<int> builder2 = std::move(builder1);
  builder2.AddEdge(3, 4);

  auto graph2 = std::move(builder2).Build();
  EXPECT_EQ(graph2.Graph().num_nodes(), 4);
  EXPECT_EQ(graph2.Node(1), 2);
  EXPECT_EQ(graph2.Node(3), 4);
  // We can move the graph itself.
  const GenericGraph<int> graph3 = std::move(graph2);
  // And the graph didn't change.
  EXPECT_EQ(graph3.Graph().num_nodes(), 4);
  EXPECT_EQ(graph3.Node(1), 2);
  EXPECT_EQ(graph3.Node(3), 4);
}

TEST(GenericGraphDeathTest, AddMoreNodesThanReserved) {
  GenericGraph<char>::Builder builder(/*num_nodes=*/2, /*num_edges=*/3);
  builder.AddEdge('a', 'b');
  if constexpr (DEBUG_MODE) {
    EXPECT_DEATH(builder.AddEdge('c', 'a'), "const_capacities");
  } else {
    builder.AddEdge('c', 'a');
    EXPECT_THAT(std::move(builder).Build().AllNodes(),
                ElementsAre('a', 'b', 'c'));
  }
}

TEST(GenericGraphDeathTest, AddMoreEdgesThanReserved) {
  GenericGraph<char>::Builder builder(/*num_nodes=*/4, /*num_edges=*/2);
  builder.AddEdge('a', 'b');
  builder.AddEdge('c', 'b');
  builder.AddEdge('c', 'a');
  EXPECT_THAT(std::move(builder).Build().Graph().AllForwardArcs(), SizeIs(3));
}

TEST(GenericGraphTest, ComparableOnlyCustomComparator) {
  struct Node {
    int id;
  };
  struct CompareNode {
    bool operator()(const Node& a, const Node& b) const { return a.id < b.id; }
  };
  GenericGraphBuilder<Node, CompareNode> builder;
  builder.AddEdge(Node{1}, Node{5});
  const GenericGraph<Node, CompareNode> graph = std::move(builder).Build();
  EXPECT_EQ(graph.Node(0).id, 1);
  EXPECT_EQ(graph.Node(1).id, 5);
}

TEST(GenericGraphTest, HashableOnlyCustomHash) {
  struct Node {
    int id;
    bool operator==(const Node& other) const { return id == other.id; }
  };
  struct HashNode {
    size_t operator()(const Node& node) const { return node.id; }
  };
  GenericGraphBuilder<Node, HashNode> builder;
  builder.AddEdge(Node{4}, Node{2});
  const GenericGraph<Node, HashNode> graph = std::move(builder).Build();
  EXPECT_EQ(graph.Node(0).id, 4);
  EXPECT_EQ(graph.Node(1).id, 2);
}

TEST(GenericGraphTest, ComparableOnlyNoCustomComparator) {
  GenericGraphBuilder<std::vector<int>> builder;
  builder.AddEdge(std::vector<int>{1, 2}, std::vector<int>{2, 3});
  const GenericGraph<std::vector<int>> graph = std::move(builder).Build();
  EXPECT_THAT(graph.AllNodes(),
              ElementsAre(ElementsAre(1, 2), ElementsAre(2, 3)));
}

}  // namespace
}  // namespace util::graph
