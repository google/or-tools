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

#include "ortools/graph/connected_components.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/hash/hash.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"

namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;

TEST(GetConnectedComponentsTest, ZeroNodes) {
  EXPECT_THAT(util::GetConnectedComponents(0, std::vector<std::vector<int>>()),
              ElementsAre());
}

TEST(GetConnectedComponentsTest, ZeroEdges) {
  EXPECT_THAT(util::GetConnectedComponents(3, std::vector<std::vector<int>>(3)),
              ElementsAre(0, 1, 2));
}

TEST(GetConnectedComponentsTest, SmallButDiverseGraph) {
  // Test graph: ("*" means that there is a self-edge (aka "loop"), and [x2]
  // means duplicate edge):
  //
  // 0 -- 5** -- 11*    1    6 -- 2 -- 10 -[x2]- 14
  //
  // 3 -- 12 -- 4    8 -- 15    13 -[x3]- 9    16
  //       |
  //       7
  const std::deque<std::set<int64_t>> graph = {
      {5},       {},        {6, 10},  {12},         {12},        {0, 5, 5, 11},
      {2},       {12},      {15},     {13, 13, 13}, {14, 2, 14}, {11, 5},
      {4, 7, 3}, {9, 9, 9}, {10, 10}, {8},          {}};
  EXPECT_THAT(util::GetConnectedComponents(graph.size(), graph),
              ElementsAre(0, 1, 2, 3, 3, 0, 2, 3, 4, 5, 2, 0, 3, 5, 2, 4, 6));
}

TEST(GetConnectedComponentsAndDenseConnectedComponentsFinderTest,
     StressTestComparison) {
  absl::BitGen random;
  constexpr int kNumTests = 100'000;
  for (int test = 0; test < kNumTests; ++test) {
    const int num_nodes = absl::Uniform(random, 0, 12);
    const int num_edges = absl::LogUniform(random, 0, num_nodes * 2);
    std::vector<absl::InlinedVector<int, 4>> adj(num_nodes);
    for (int edge = 0; edge < num_edges; ++edge) {
      const int a = absl::Uniform(random, 0, num_nodes);
      const int b = absl::Uniform(random, 0, num_nodes);
      adj[a].push_back(b);
      adj[b].push_back(a);
    }
    DenseConnectedComponentsFinder finder;
    finder.SetNumberOfNodes(num_nodes);
    for (int from = 0; from < num_nodes; ++from) {
      for (int to : adj[from]) {
        finder.AddEdge(from, to);
      }
    }
    ASSERT_THAT(util::GetConnectedComponents(num_nodes, adj),
                ElementsAreArray(finder.GetComponentIds()))
        << DUMP_VARS(test, num_nodes, num_edges, adj);
  }
}

TEST(GetConnectedComponentsTest, HugeGraphDoesntFitInt32) {
  // Timing: 21s as of 2022-10 (forge, opt).
  if (DEBUG_MODE) GTEST_SKIP() << "This test is too slow in non-opt";
  // Building an actual graph with more than 2**31 nodes is problematic,
  // since we'd typically require at least an InlinedVector<int> for each
  // node, whose size starts at 24. That's 2B * 24 = 48GB, which is 4x the
  // forge memory limit as of 2022 (go/forge-memory).
  // So instead we design a custom graph structure that doesn't actually
  // need any memory.
  // But even then, the internal data structure used by
  // GetConnectedComponents() is challenging: it returns a vector<NodeType>.
  // So we cannot use int64_t, since it'd use at least 16GB. We can use
  // uint32_t though.
  const unsigned int num_nodes =
      (1U << 31) + 1024;  // Doesn't fit in an int32_t.
  // Our fake graph has num_nodes/2 edges, connecting node #i with #i+1.
  // That way, the internal bfs_queue stays very small and we don't go much
  // over 8GB of memory.
  struct FakePairGraph {
    std::array<int, 1> operator[](int node) const { return {node ^ 1}; }
  };
  const std::vector<unsigned int> components =
      util::GetConnectedComponentsTpl(num_nodes, FakePairGraph());
  for (unsigned int i = 0; i < num_nodes; ++i) {
    ASSERT_EQ(components[i], i / 2);
  }
}

// The number of graph nodes used in all the tests.
static const int kNumNodes = 15;

// Tests that all the nodes are uniquely present in a vector of connected
// components.
static void TestAllNodesPresent(
    absl::Span<const absl::btree_set<int>> components) {
  std::set<int> all_components;
  for (const absl::btree_set<int>& component : components) {
    for (const int node : component) {
      EXPECT_TRUE(all_components.find(node) == all_components.end())
          << "Node " << node << " duplicated.";
      all_components.insert(node);
    }
  }

  EXPECT_EQ(kNumNodes, all_components.size());

  for (int i = 0; i < kNumNodes; i++) {
    EXPECT_TRUE(all_components.find(i) != all_components.end())
        << "Node " << i << " missing";
  }
}

TEST(ConnectedComponentsFinder, EmptyGraph) {
  ConnectedComponentsFinder<int> finder;
  EXPECT_EQ(0, finder.GetNumberOfNodes());
  EXPECT_EQ(0, finder.GetNumberOfComponents());
  std::vector<absl::btree_set<int>> components;
  finder.FindConnectedComponents(&components);
  EXPECT_EQ(0, components.size());
}

// Creates a graph with no edges (completely disconnected).
TEST(ConnectedComponentsFinder, DisconnectedGraph) {
  ConnectedComponentsFinder<int> cc;
  for (int i = 0; i < kNumNodes; i++) {
    EXPECT_EQ(i, cc.GetNumberOfNodes());
    EXPECT_EQ(i, cc.GetNumberOfComponents());
    EXPECT_EQ(0, cc.GetSize(i));
    cc.AddNode(i);
    EXPECT_EQ(1, cc.GetSize(i));
  }
  EXPECT_EQ(kNumNodes, cc.GetNumberOfNodes());

  std::vector<absl::btree_set<int>> components;
  cc.FindConnectedComponents(&components);
  EXPECT_EQ(kNumNodes, components.size());
  EXPECT_EQ(kNumNodes, cc.GetNumberOfComponents());
  for (auto i = components.begin(); i != components.end(); ++i) {
    EXPECT_EQ(1, i->size());
  }

  TestAllNodesPresent(components);
}

// Creates a completely connected graph.
TEST(ConnectedComponentsFinder, CompletelyConnectedGraph) {
  ConnectedComponentsFinder<int> cc;
  int num_edge_additions = 0;
  for (int i = 0; i < kNumNodes; i++) {
    cc.AddNode(i);
    EXPECT_EQ(std::min(i + 1, 2), cc.GetNumberOfComponents());
    EXPECT_EQ(i + 1, cc.GetNumberOfNodes());
    for (int j = 0; j <= i; j++) {
      if (cc.AddEdge(i, j)) ++num_edge_additions;
      EXPECT_EQ(i + 1, cc.GetSize(j));
    }
    EXPECT_EQ(1, cc.GetNumberOfComponents());
  }
  EXPECT_EQ(kNumNodes, cc.GetNumberOfNodes());
  EXPECT_EQ(num_edge_additions, kNumNodes - 1);

  std::vector<absl::btree_set<int>> components;
  cc.FindConnectedComponents(&components);
  ASSERT_EQ(1, components.size());
  EXPECT_EQ(kNumNodes, components[0].size());
  TestAllNodesPresent(components);
}

// Tests that adding a node more than once is harmless.
TEST(ConnectedComponentsFinder, RedundantAddNode) {
  ConnectedComponentsFinder<int> cc;
  for (int i = 0; i < kNumNodes; i++) {
    cc.AddNode(i);
    for (int j = 0; j <= i; j++) {
      cc.AddNode(j);
      cc.AddEdge(i, j);
      cc.AddNode(j);
      EXPECT_EQ(i + 1, cc.GetSize(j));
    }
    EXPECT_EQ(i + 1, cc.GetNumberOfNodes());
    EXPECT_EQ(1, cc.GetNumberOfComponents());
  }

  std::vector<absl::btree_set<int>> components;
  cc.FindConnectedComponents(&components);
  ASSERT_EQ(1, components.size());
  EXPECT_EQ(kNumNodes, components[0].size());
  TestAllNodesPresent(components);
}

// Tests that nodes do not need to be added before their edges.
TEST(ConnectedComponentsFinder, AutoAddNode) {
  ConnectedComponentsFinder<int> cc;
  for (int i = 0; i < kNumNodes; i++) {
    for (int j = i; j >= 0; j--) {
      cc.AddEdge(i, j);
      EXPECT_EQ(i + 1, cc.GetNumberOfNodes());
      if (j == i) {
        EXPECT_EQ(1, cc.GetSize(j));
        EXPECT_EQ(std::min(i + 1, 2), cc.GetNumberOfComponents());
      } else {
        EXPECT_EQ(i + 1, cc.GetSize(j));
        EXPECT_EQ(1, cc.GetNumberOfComponents());
      }
    }
  }
  EXPECT_EQ(kNumNodes, cc.GetNumberOfNodes());

  std::vector<absl::btree_set<int>> components;
  cc.FindConnectedComponents(&components);
  ASSERT_EQ(1, components.size());
  EXPECT_EQ(kNumNodes, components[0].size());
  TestAllNodesPresent(components);
}

// Creates a linearly connected graph.
TEST(ConnectedComponentsFinder, ForwardLinkedGraph) {
  ConnectedComponentsFinder<int> cc;
  for (int i = 0; i < kNumNodes; i++) {
    cc.AddNode(i);
    EXPECT_FALSE(cc.Connected(i + 1, i));
    if (i > 0) {
      EXPECT_FALSE(cc.Connected(0, i));
      EXPECT_FALSE(cc.Connected(i - 1, i));
      EXPECT_EQ(2, cc.GetNumberOfComponents());
      ASSERT_TRUE(cc.AddEdge(i - 1, i));
      EXPECT_TRUE(cc.Connected(i - 1, i));
      EXPECT_TRUE(cc.Connected(0, i));
    }
    EXPECT_EQ(i + 1, cc.GetNumberOfNodes());
    EXPECT_EQ(1, cc.GetNumberOfComponents());
    EXPECT_EQ(i + 1, cc.GetSize(0));
  }

  std::vector<absl::btree_set<int>> components;
  cc.FindConnectedComponents(&components);
  ASSERT_EQ(1, components.size());
  EXPECT_EQ(kNumNodes, components[0].size());
  TestAllNodesPresent(components);
}

// Creates a linearly connected graph but reverses the nodes in each edge,
// to make sure that it doesn't matter.
TEST(ConnectedComponentsFinder, ForwardLinkedGraphReversedEdges) {
  ConnectedComponentsFinder<int> cc;
  for (int i = 0; i < kNumNodes; i++) {
    cc.AddNode(i);
    EXPECT_FALSE(cc.Connected(i, i + 1));
    if (i > 0) {
      EXPECT_FALSE(cc.Connected(i, 0));
      EXPECT_FALSE(cc.Connected(i, i - 1));
      EXPECT_EQ(2, cc.GetNumberOfComponents());
      ASSERT_TRUE(cc.AddEdge(i, i - 1));
      EXPECT_TRUE(cc.Connected(i, i - 1));
      EXPECT_TRUE(cc.Connected(i, 0));
    }
    EXPECT_EQ(i + 1, cc.GetNumberOfNodes());
    EXPECT_EQ(1, cc.GetNumberOfComponents());
    EXPECT_EQ(i + 1, cc.GetSize(0));
  }

  std::vector<absl::btree_set<int>> components;
  cc.FindConnectedComponents(&components);
  ASSERT_EQ(1, components.size());
  EXPECT_EQ(kNumNodes, components[0].size());
  TestAllNodesPresent(components);
}

// Creates a linearly connected graph but reverses the order the edges are
// added, to make sure that it doesn't matter.
TEST(ConnectedComponentsFinder, ReverseLinkedGraph) {
  ConnectedComponentsFinder<int> cc;
  for (int i = 0; i < kNumNodes; i++) {
    EXPECT_FALSE(cc.Connected(-1, i));
    cc.AddNode(i);
    EXPECT_EQ(i + 1, cc.GetNumberOfNodes());
    EXPECT_EQ(i + 1, cc.GetNumberOfComponents());
  }

  for (int i = kNumNodes - 1; i >= 1; i--) {
    EXPECT_FALSE(cc.Connected(i, i - 1));
    ASSERT_TRUE(cc.AddEdge(i, i - 1));
    EXPECT_TRUE(cc.Connected(i, i - 1));
    EXPECT_EQ(i, cc.GetNumberOfComponents());
    EXPECT_EQ(kNumNodes, cc.GetNumberOfNodes());
  }

  std::vector<absl::btree_set<int>> components;
  cc.FindConnectedComponents(&components);
  ASSERT_EQ(1, components.size());
  EXPECT_EQ(kNumNodes, components[0].size());
  TestAllNodesPresent(components);
}

// Creates a graph with two components in it.
TEST(ConnectedComponentsFinder, TwoComponents) {
  ConnectedComponentsFinder<int> cc;
  for (int i = 0; i < kNumNodes; i++) {
    cc.AddNode(i);
    if (i != 0 && i != kNumNodes / 2) {
      ASSERT_TRUE(cc.AddEdge(i, i - 1));
    }
    EXPECT_EQ(i + 1, cc.GetNumberOfNodes());
  }
  EXPECT_EQ(2, cc.GetNumberOfComponents());

  EXPECT_EQ(kNumNodes / 2, cc.GetSize(0));
  EXPECT_EQ(kNumNodes - kNumNodes / 2, cc.GetSize(kNumNodes - 1));
  EXPECT_EQ(0, cc.GetSize(kNumNodes));

  std::vector<absl::btree_set<int>> components;
  cc.FindConnectedComponents(&components);
  ASSERT_EQ(2, components.size());
  TestAllNodesPresent(components);
  bool zero_in_first = components[0].find(0) != components[0].end();
  int first_half = zero_in_first ? 0 : 1;
  int second_half = zero_in_first ? 1 : 0;

  for (int i = 0; i < kNumNodes / 2; i++) {
    EXPECT_TRUE(components[first_half].find(i) != components[first_half].end());
    EXPECT_TRUE(components[second_half].find(i) ==
                components[second_half].end());
  }
  for (int i = kNumNodes / 2; i < kNumNodes; i++) {
    EXPECT_TRUE(components[first_half].find(i) == components[first_half].end());
    EXPECT_TRUE(components[second_half].find(i) !=
                components[second_half].end());
  }
}

TEST(ConnectedComponentsFinder, DeterministicOrder) {
  // We test that both the component order, and the order of nodes within
  // each component, follow the API guarantees, i.e. sorted by node insertion
  // order.
  ConnectedComponentsFinder<std::string, absl::Hash<std::string>> finder;
  finder.AddNode("A");
  finder.AddEdge("A", "E");
  finder.AddNode("T");
  finder.AddEdge("C", "A");
  finder.AddEdge("B", "B");
  finder.AddEdge("C", "A");
  finder.AddEdge("W", "O");
  finder.AddNode("C");
  finder.AddNode("D");
  finder.AddEdge("W", "T");
  finder.AddEdge("W", "D");
  // When sorting by insertion order.
  EXPECT_THAT(
      finder.FindConnectedComponents(NodeOrderInsideComponent::kInsertionOrder),
      ElementsAre(ElementsAre("A", "E", "C"), ElementsAre("T", "W", "O", "D"),
                  ElementsAre("B")));
  // When sorting by value.
  EXPECT_THAT(
      finder.FindConnectedComponents(NodeOrderInsideComponent::kByValue),
      ElementsAre(ElementsAre("A", "C", "E"), ElementsAre("D", "O", "T", "W"),
                  ElementsAre("B")));
  // When not sorting at all.
  EXPECT_THAT(finder.FindConnectedComponents(
                  NodeOrderInsideComponent::kNotOrderedUndeterministic),
              ElementsAre(UnorderedElementsAre("A", "C", "E"),
                          UnorderedElementsAre("T", "W", "O", "D"),
                          UnorderedElementsAre("B")));
}

TEST(ConnectedComponentsFinder, DeterministicOrderGoldenTest) {
  std::mt19937 random(0);  // Deterministic, same across platforms.
  ConnectedComponentsFinder<int> finder;
  // T keep things interesting, define a large graph with non-trivial components
  // including a quasi-giant component:
  // https://en.wikipedia.org/wiki/Giant_component.
  constexpr int kNumNodes = 1'000'000;
  constexpr int kNumEdges = kNumNodes;
  for (int i = 0; i < kNumEdges; ++i) {
    const int node1 = absl::Uniform<int>(random, 0, kNumNodes);
    const int node2 = absl::Uniform<int>(random, 0, kNumNodes);
    finder.AddEdge(node1, node2);
  }
  struct TestCase {
    NodeOrderInsideComponent order;
    uint64_t expected_fprint;
  };
  constexpr auto kTestCases = std::to_array<TestCase>(
      {{.order = NodeOrderInsideComponent::kInsertionOrder,
        .expected_fprint = 1750945623296128363ULL},
       {.order = NodeOrderInsideComponent::kByValue,
        .expected_fprint = 7482613051014086088ULL}});
  const auto mix = [](uint64_t h, uint64_t v) -> uint64_t {
    return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
  };
  for (const auto& test_case : kTestCases) {
    uint64_t fprint = 0;
    for (const auto& component :
         finder.FindConnectedComponents(test_case.order)) {
      for (uint64_t node : component) {
        fprint = mix(fprint, node);
      }
    }
    EXPECT_EQ(fprint, test_case.expected_fprint);
  }
}

TEST(ConnectedComponentsFinder, NodeOrderWithCustomComparator) {
  ConnectedComponentsFinder<std::string, std::greater<std::string>> finder;
  finder.AddEdge("A", "E");
  EXPECT_THAT(
      finder.FindConnectedComponents(NodeOrderInsideComponent::kByValue),
      ElementsAre(ElementsAre("E", "A")));
  EXPECT_THAT(
      finder.FindConnectedComponents(NodeOrderInsideComponent::kInsertionOrder),
      ElementsAre(ElementsAre("A", "E")));
}

struct WithHashPrintEqNoLess {
  int x;

  bool operator==(const WithHashPrintEqNoLess& rhs) const { return x == rhs.x; }

  template <typename H>
  friend H AbslHashValue(H h, const WithHashPrintEqNoLess& t) {
    return H::combine(std::move(h), t.x);
  }
};

TEST(ConnectedComponentsFinder, SortByValueDFatalAndIgnoredWhenUncomparable) {
  using T = WithHashPrintEqNoLess;
  ConnectedComponentsFinder<T, absl::Hash<T>> finder;
  finder.AddEdge({.x = 5}, {.x = 2});
  finder.AddNode({.x = 1});
  finder.AddEdge({.x = 3}, {.x = 1});
  finder.AddEdge({.x = 3}, {.x = 9});
  finder.AddEdge({.x = 3}, {.x = 6});
  EXPECT_DEBUG_DEATH(
      finder.FindConnectedComponents(NodeOrderInsideComponent::kByValue),
      "not sortable");
}

// Makes sure ConnectedComponentsFinder works with supplied equality operators
// (useful for protos, see comment in header).
// We create a graph with a struct `IntegerNode` as node type. `IntegerNode`
// does not have 'operator ==' defined, and `IntegerNodeEquals` is defined and
// supplied to `ConnectedComponentsFinder`.
class IntegerNode {
 public:
  explicit IntegerNode(int value) : value_(value) {}

  int value_;

  template <typename H>
  friend H AbslHashValue(H h, const IntegerNode& s) {
    return H::combine(std::move(h), s.value_);
  }
};

struct IntegerNodeEquals {
  bool operator()(const IntegerNode& x, const IntegerNode& y) const {
    return x.value_ == y.value_;
  }
};

TEST(ConnectedComponentsFinder, NonEqualityComparable) {
  using ConnectedComponentType =
      absl::flat_hash_set<IntegerNode, absl::Hash<IntegerNode>,
                          IntegerNodeEquals>;
  using Finder = ConnectedComponentsFinder<ConnectedComponentType::key_type,
                                           ConnectedComponentType::hasher,
                                           ConnectedComponentType::key_equal>;

  Finder cc;

  // Create a graph with two components:
  // 1--2  3
  IntegerNode n1(1);
  IntegerNode n2(2);
  IntegerNode n3(3);
  cc.AddNode(n1);
  cc.AddNode(n2);
  cc.AddNode(n2);
  cc.AddNode(n3);
  cc.AddEdge(n1, n2);
  EXPECT_EQ(cc.GetNumberOfNodes(), 3);
  EXPECT_EQ(cc.GetNumberOfComponents(), 2);
  std::vector<ConnectedComponentType> components;
  cc.FindConnectedComponents(&components);
}

TEST(DenseConnectedComponentsFinder, GetComponentRoots) {
  DenseConnectedComponentsFinder finder;
  finder.SetNumberOfNodes(4);
  EXPECT_THAT(finder.GetComponentRoots(), testing::ElementsAre(0, 1, 2, 3));

  ASSERT_TRUE(finder.AddEdge(0, 1));
  EXPECT_THAT(finder.GetComponentRoots(), testing::ElementsAre(1, 2, 3));

  ASSERT_TRUE(finder.AddEdge(2, 3));
  EXPECT_THAT(finder.GetComponentRoots(), testing::ElementsAre(1, 3));

  ASSERT_TRUE(finder.AddEdge(1, 2));
  EXPECT_THAT(finder.GetComponentRoots(), testing::ElementsAre(3));
}

TEST(DenseConnectedComponentsFinder, GetComponentRootsGrowDynamically) {
  DenseConnectedComponentsFinder finder;
  EXPECT_TRUE(finder.GetComponentRoots().empty());

  ASSERT_TRUE(finder.AddEdge(0, 1));
  EXPECT_THAT(finder.GetComponentRoots(), testing::ElementsAre(1));

  ASSERT_TRUE(finder.AddEdge(2, 3));
  EXPECT_THAT(finder.GetComponentRoots(), testing::ElementsAre(1, 3));

  ASSERT_TRUE(finder.AddEdge(1, 2));
  EXPECT_THAT(finder.GetComponentRoots(), testing::ElementsAre(3));

  finder.SetNumberOfNodes(8);
  EXPECT_THAT(finder.GetComponentRoots(), testing::ElementsAre(3, 4, 5, 6, 7));
}

TEST(DenseConnectedComponentsFinder, AddEdgeReturnValue) {
  DenseConnectedComponentsFinder finder;
  ASSERT_TRUE(finder.AddEdge(1, 2));
  ASSERT_TRUE(finder.AddEdge(1, 3));
  ASSERT_FALSE(finder.AddEdge(2, 3));
  ASSERT_TRUE(finder.AddEdge(2, 5));
  ASSERT_TRUE(finder.AddEdge(5, 4));
  ASSERT_FALSE(finder.AddEdge(1, 4));
}

TEST(DenseConnectedComponentsFinder, CopyConstructor) {
  DenseConnectedComponentsFinder finder;
  ASSERT_TRUE(finder.AddEdge(1, 2));
  ASSERT_TRUE(finder.AddEdge(3, 4));
  ASSERT_TRUE(finder.AddEdge(4, 5));
  ASSERT_TRUE(finder.AddEdge(6, 7));
  ASSERT_TRUE(finder.AddEdge(1, 5));
  ASSERT_TRUE(finder.AddEdge(9, 7));

  // Test the copy constructor.
  DenseConnectedComponentsFinder finder2 = finder;
  EXPECT_EQ(finder2.GetNumberOfComponents(), finder.GetNumberOfComponents());
  EXPECT_EQ(finder2.GetNumberOfNodes(), finder.GetNumberOfNodes());
  EXPECT_THAT(finder2.GetComponentRoots(),
              ElementsAreArray(finder.GetComponentRoots()));
  EXPECT_THAT(finder2.GetComponentIds(),
              ElementsAreArray(finder.GetComponentIds()));

  // Now test the copy assignment operator.
  DenseConnectedComponentsFinder finder3;
  finder3.AddEdge(23, 34);
  finder3 = finder;
  EXPECT_EQ(finder2.GetNumberOfComponents(), finder.GetNumberOfComponents());
  EXPECT_EQ(finder2.GetNumberOfNodes(), finder.GetNumberOfNodes());
  EXPECT_THAT(finder2.GetComponentRoots(),
              ElementsAreArray(finder.GetComponentRoots()));
  EXPECT_THAT(finder2.GetComponentIds(),
              ElementsAreArray(finder.GetComponentIds()));
}

TEST(DenseConnectedComponentsFinder, MoveOperatorAndConstructor) {
  constexpr int kNumFinders = 1000000;
  constexpr int kNumNodes = 1000000;
  std::vector<DenseConnectedComponentsFinder> finders(kNumFinders);
  // Add kNumNodes nodes and kNumNodes-1 edges to finders[0].
  finders[0].SetNumberOfNodes(kNumNodes);
  for (int i = 1; i < kNumNodes; ++i) {
    ASSERT_TRUE(finders[0].AddEdge(i - 1, i));
  }
  // Now, move that finder kNumFinders times. If the = operator wasn't a proper
  // move, then we would copy the payload, resulting in
  // O(kNumFinders * kNumNodes) memory usage, which would OOM forge.
  for (int i = 1; i < kNumFinders; ++i) {
    finders[i] = std::move(finders[i - 1]);
  }
  EXPECT_EQ(finders.back().GetNumberOfNodes(), kNumNodes);
  EXPECT_EQ(finders.back().GetNumberOfComponents(), 1);
  // Keep do the same test with the move constructor.
  for (int i = 0; i < kNumFinders; ++i) {
    finders.emplace_back(std::move(finders.back()));
  }
  EXPECT_EQ(finders.back().GetNumberOfNodes(), kNumNodes);
  EXPECT_EQ(finders.back().GetNumberOfComponents(), 1);
}

}  // namespace
