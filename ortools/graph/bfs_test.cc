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

#include "ortools/graph/bfs.h"

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/io.h"

namespace util {
namespace graph {
namespace {

using ::testing::ElementsAre;
using ::testing::status::IsOkAndHolds;

struct GraphSize {
  int num_nodes;
  int num_arcs;

  std::strong_ordering operator<=>(const GraphSize&) const = default;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const GraphSize& s) {
    absl::Format(&sink, "{num_nodes = %d, num_arcs = %d}", s.num_nodes,
                 s.num_arcs);
  }
};

// This function pre-generates "interesting" test graph sizes at random and
// sorts them (smallest first), so that the randomized stress tests that run on
// many graph instances of these sizes stop on the smallest possible test graph
// when there's a bug somewhere.
template <typename NodeIndex>
std::vector<GraphSize> GenerateRandomGraphSizes(absl::BitGen& random,
                                                int num_graph_sizes) {
  // We pick up to 300 nodes, or 254 if the type is uint8_t, or 126 if the type
  // is int8_t: that way we are sure that we exercise the overflow boundary i.e.
  // the maximum possible num_nodes for these types, while keeping our graphs
  // relatively small. Note that the max valid value is
  // `std::numeric_limits<NodeIndex>::max() - 1` since the`kNilArc` uses
  // `std::numeric_limits<NodeIndex>::max()`.
  constexpr NodeIndex kMaxNumNodes =
      std::min<uint64_t>(std::numeric_limits<NodeIndex>::max() - 1, 300);
  std::vector<GraphSize> graph_sizes;
  for (int attempt = 0;; ++attempt) {
    int num_tests_with_max_num_nodes = 0;
    for (int i = 0; i < num_graph_sizes; ++i) {
      const int num_nodes = absl::LogUniform<int>(random, 1, kMaxNumNodes);
      const int num_arcs =
          absl::LogUniform(random, 0, std::min(1000, num_nodes * num_nodes));
      graph_sizes.push_back({.num_nodes = num_nodes, .num_arcs = num_arcs});
      if (num_nodes == kMaxNumNodes) ++num_tests_with_max_num_nodes;
    }
    if (kMaxNumNodes >= 300 ||
        num_tests_with_max_num_nodes > num_graph_sizes / 2000) {
      break;
    }
    graph_sizes.clear();
    CHECK_LE(attempt, 10)
        << "Couldn't generate random graph sizes with sufficient ratio of"
           " cases where num_nodes = kMaxNumNodes";
  }
  absl::c_sort(graph_sizes);
  return graph_sizes;
}

template <typename NodeIndex>
class BFSTypedTest : public testing::Test {};

TYPED_TEST_SUITE_P(BFSTypedTest);

TYPED_TEST_P(BFSTypedTest, CorrectnessStressTest) {
  absl::BitGen random;
  typedef TypeParam NodeIndex;
  constexpr int kNumTests = DEBUG_MODE ? 20'000 : 100'000;
  const std::vector<GraphSize> graph_sizes =
      GenerateRandomGraphSizes<NodeIndex>(random, kNumTests);

  // Now run the stress test on all the test graphs.
  for (int test = 0; test < kNumTests; ++test) {
    const int num_nodes = graph_sizes[test].num_nodes;
    const int num_arcs = graph_sizes[test].num_arcs;
    // Generate a random graph. Multiple and self-arcs are allowed. We build
    // a reverse graph, so that we can easily access incoming arcs.
    typename ReverseArcStaticGraph<NodeIndex>::Builder builder(num_nodes,
                                                               num_arcs);
    for (int a = 0; a < num_arcs; ++a) {
      builder.AddArc(absl::Uniform<NodeIndex>(random, 0, num_nodes),
                     absl::Uniform<NodeIndex>(random, 0, num_nodes));
    }
    const auto graph = std::move(builder).Build(nullptr);

    // Select a random source node.
    const NodeIndex source = absl::Uniform<NodeIndex>(random, 0, num_nodes);
    SCOPED_TRACE(DUMP_VARS(test, num_nodes, num_arcs, source));

    // Run a BFS from the source, and get the distances.
    ASSERT_OK_AND_ASSIGN(
        const std::vector<NodeIndex> bfs_tree,
        GetBFSRootedTree(*graph, static_cast<NodeIndex>(num_nodes), source));
    ASSERT_EQ(bfs_tree.size(), num_nodes);
    ASSERT_OK_AND_ASSIGN(const std::vector<NodeIndex> distances,
                         GetBFSDistances(bfs_tree));
    ASSERT_EQ(distances.size(), num_nodes);

    // Run a few verifications of expected invariants. For each node #i with
    // distance d:
    // - At least one of its incoming neighbors will have distance d-1: that's
    //   the one in bfs_tree[i]. All other distances will be ≥ d-1.
    // - All outgoing neighbors have distance ≤ d+1.
    std::vector<absl::InlinedVector<int, 4>> tie_break_arcs(num_nodes);
    constexpr NodeIndex kNone = -1;  // NOLINT
    EXPECT_EQ(bfs_tree[source], source);
    EXPECT_EQ(distances[source], 0);
    int max_distance = 0;
    for (int node = 0; node < num_nodes; ++node) {
      const NodeIndex d = distances[node];
      if (d == kNone) {  // Not reached!
        EXPECT_EQ(bfs_tree[node], kNone);
        for (const int in_arc : graph->IncomingArcs(node)) {
          const NodeIndex in_neigh = graph->Tail(in_arc);
          EXPECT_EQ(distances[in_neigh], kNone);
        }
        continue;
      }
      max_distance = std::max<int>(max_distance, d);
      // The checks below are in the "reached" case.
      const NodeIndex parent = bfs_tree[node];
      ASSERT_GE(parent, 0);
      ASSERT_LT(parent, num_nodes);
      ASSERT_GE(d, 0);
      ASSERT_LT(d, num_nodes);
      if (d == 0) {
        EXPECT_EQ(node, source);
      } else {
        EXPECT_EQ(distances[parent], d - 1);
      }
      for (const NodeIndex out_neigh : (*graph)[node]) {
        EXPECT_LE(distances[out_neigh], d + 1) << DUMP_VARS(out_neigh);
      }
      if (d != 0) {
        bool seen_parent = false;
        for (const int in_arc : graph->IncomingArcs(node)) {
          const NodeIndex in_neigh = graph->Tail(in_arc);
          if (distances[in_neigh] == kNone) continue;
          EXPECT_GE(distances[in_neigh], d - 1);
          if (in_neigh == parent) {
            seen_parent = true;
          } else if (distances[in_neigh] == d - 1) {
            // 'in_neigh' is after 'parent' in the tie-breaking order.
            tie_break_arcs[parent].push_back(in_neigh);
          }
        }
        EXPECT_TRUE(seen_parent);
      }
      if (TestFixture::HasFailure()) {
        FAIL() << "Found failure, stopping adjacency list invariant checks at: "
               << DUMP_VARS(node, parent, d);
        break;
      }
    }
    if (TestFixture::HasFailure()) {
      FAIL() << "Found failure, stopping. "
             << DUMP_VARS(util::GraphToString(
                    *graph, util::PRINT_GRAPH_ADJACENCY_LISTS));
    }
  }
}

TYPED_TEST_P(BFSTypedTest, SimpleExample) {
  typedef TypeParam NodeIndex;
  // This is the example given in the toplevel comment of bfs.h.
  std::vector<std::vector<NodeIndex>> graph = {{1}, {0}, {1, 2}};
  ASSERT_OK_AND_ASSIGN(const std::vector<NodeIndex> bfs_tree,
                       GetBFSRootedTree(graph, NodeIndex{3}, NodeIndex{1}));
  EXPECT_THAT(bfs_tree, ElementsAre(1, 1, -1));
  EXPECT_THAT(GetBFSDistances(bfs_tree), IsOkAndHolds(ElementsAre(1, 0, -1)));
  EXPECT_THAT(GetBFSShortestPath(bfs_tree, NodeIndex{0}),
              IsOkAndHolds(ElementsAre(1, 0)));
  EXPECT_THAT(GetBFSShortestPath(bfs_tree, NodeIndex{1}),
              IsOkAndHolds(ElementsAre(1)));
  EXPECT_THAT(GetBFSShortestPath(bfs_tree, NodeIndex{2}),
              IsOkAndHolds(ElementsAre()));
}

TYPED_TEST_P(BFSTypedTest, TieBreaking) {
  typedef TypeParam NodeIndex;
  constexpr int kNumTests = DEBUG_MODE ? 10'000 : 50'000;
  absl::BitGen random;
  for (int test = 0; test < kNumTests; ++test) {
    // The graph is a multi-layer graph with 4 layers; 3 nodes per layer:
    // layer #0 = {0, 1, 2}, layer #1 = {3, 4, 5}, layer #2 = {6, 7, 8}, ...
    // Each node of layer i has outgoing arcs to all nodes of layer i+1, but
    // with a randomized order.
    // In addition, to better test the robustness, we add a few random backwards
    // or transverse arcs that shouldn't affect the BFS.
    std::vector<std::vector<NodeIndex>> graph(12);
    for (int layer = 0; layer < 3; ++layer) {
      for (int from = layer * 3; from < layer * 3 + 3; ++from) {
        for (int i = 0; i < 3; ++i) graph[from].push_back((layer + 1) * 3 + i);
        absl::c_shuffle(graph[from], random);
      }
    }
    const int num_backwards_or_transverse_arcs =
        absl::LogUniform(random, 0, 16);
    for (int a = 0; a < num_backwards_or_transverse_arcs; ++a) {
      const int src = absl::Uniform<int>(random, 0, graph.size());
      const int dst = absl::Uniform<int>(random, 0, 3 * (src / 3 + 1));
      graph[src].push_back(dst);
    }
    SCOPED_TRACE(DUMP_VARS(num_backwards_or_transverse_arcs, graph));
    const int source = absl::Uniform(random, 0, 3);
    const int first_layer1 = graph[source][0];
    const int first_layer2 = graph[first_layer1][0];
    ASSERT_OK_AND_ASSIGN(
        const std::vector<NodeIndex> bfs_tree,
        GetBFSRootedTree(graph, static_cast<NodeIndex>(graph.size()),
                         static_cast<NodeIndex>(source)));
    for (int i = 0; i < 3; ++i) {
      const NodeIndex node(3 * 3 + i);
      ASSERT_EQ(bfs_tree[node], first_layer2) << i;
      ASSERT_THAT(
          GetBFSShortestPath(bfs_tree, node),
          IsOkAndHolds(ElementsAre(source, first_layer1, first_layer2, node)));
    }
  }
}

template <typename NodeIndex>
NodeIndex RandomNodeMaybeBad(absl::BitGenRef random, int num_nodes,
                             double proba_bad) {
  if (!absl::Bernoulli(random, proba_bad)) {
    return absl::Uniform<NodeIndex>(random, 0, num_nodes);
  }
  const double p = absl::Uniform(random, 0.0, 1.0);
  if (p < 0.3) return -1;
  if (p < 0.6) return num_nodes;
  constexpr NodeIndex min_val = std::numeric_limits<NodeIndex>::min();
  constexpr NodeIndex max_val = std::numeric_limits<NodeIndex>::min();
  if (min_val < 0 && p < 0.8) {
    return absl::Uniform<NodeIndex>(random, min_val, 0);
  }
  return absl::Uniform<NodeIndex>(absl::IntervalClosedClosed, random, num_nodes,
                                  max_val);
}

// NOTE(user): See the note in the .h: GetBFSRootedTree() may, in fact, crash
// if calling the [] operator on the graph with an argument in [0..num_nodes-1]
// may crash.
TYPED_TEST_P(BFSTypedTest, GetBFSRootedTreeDoesNotCrashOnBadGraphs) {
  typedef TypeParam NodeIndex;
  constexpr int kNumTests = DEBUG_MODE ? 10'000 : 50'000;
  absl::BitGen random;
  const std::vector<GraphSize> graph_sizes =
      GenerateRandomGraphSizes<NodeIndex>(random, kNumTests);
  int num_failures = 0;
  for (int test = 0; test < kNumTests; ++test) {
    const double proba_bad_node = std::exp(-absl::Uniform(random, 0, 4));
    const int num_nodes = graph_sizes[test].num_nodes;
    const int num_arcs = graph_sizes[test].num_arcs;
    std::vector<std::vector<NodeIndex>> graph(
        num_nodes + (absl::Bernoulli(random, 0.5)
                         ? 0
                         : absl::LogUniform(random, 1, 2 * num_nodes)));
    for (int a = 0; a < num_arcs; ++a) {
      graph[absl::Uniform<NodeIndex>(random, 0, num_nodes)].push_back(
          RandomNodeMaybeBad<NodeIndex>(random, num_nodes, proba_bad_node));
    }
    auto result_or = GetBFSRootedTree(
        graph, static_cast<NodeIndex>(num_nodes),
        RandomNodeMaybeBad<NodeIndex>(random, num_nodes, proba_bad_node));
    if (!result_or.ok()) ++num_failures;
  }
  EXPECT_GE(num_failures, kNumTests / 2);
}

TYPED_TEST_P(BFSTypedTest,
             GetBfsDistancesAndGetBFSShortestPathDoNotCrashOnBadBfsTree) {
  typedef TypeParam NodeIndex;
  constexpr int kNumTests = DEBUG_MODE ? 50'000 : 200'000;
  absl::BitGen random;
  constexpr NodeIndex kMaxNumNodes =
      std::min<uint64_t>(std::numeric_limits<NodeIndex>::max(), 300);
  std::vector<int> num_nodes_list;
  for (int test = 0; test < kNumTests; ++test) {
    num_nodes_list.push_back(absl::LogUniform<int>(random, 1, kMaxNumNodes));
  }
  if (kMaxNumNodes < 300) {
    ASSERT_GE(absl::c_count(num_nodes_list, kMaxNumNodes), 20);
  }
  absl::c_sort(num_nodes_list);

  int num_failures_get_distances = 0;
  int num_failures_get_shortest_path = 0;
  for (int test = 0; test < kNumTests; ++test) {
    const int num_nodes = num_nodes_list[test];
    const int num_extra_nodes = absl::Bernoulli(random, 0.1)
                                    ? absl::LogUniform(random, 1, 2 * num_nodes)
                                    : 0;
    std::vector<NodeIndex> bfs_tree(num_nodes + num_extra_nodes, -1);
    const double proba_bad = std::exp(-absl::Uniform(random, 0, 6));
    for (NodeIndex& node : bfs_tree) {
      node = RandomNodeMaybeBad<NodeIndex>(random, num_nodes, proba_bad);
    }
    if (!GetBFSDistances(bfs_tree).ok()) ++num_failures_get_distances;
    if (!GetBFSShortestPath(bfs_tree, RandomNodeMaybeBad<NodeIndex>(
                                          random, num_nodes, proba_bad))
             .ok()) {
      ++num_failures_get_shortest_path;
    }
  }
  EXPECT_GE(num_failures_get_distances, kNumTests / 2);
  EXPECT_GE(num_failures_get_shortest_path, kNumTests / 2);
}

REGISTER_TYPED_TEST_SUITE_P(
    BFSTypedTest, SimpleExample, TieBreaking, CorrectnessStressTest,
    GetBFSRootedTreeDoesNotCrashOnBadGraphs,
    GetBfsDistancesAndGetBFSShortestPathDoNotCrashOnBadBfsTree);

typedef testing::Types<char, uint8_t, int16_t, uint16_t, int32_t, uint32_t,
                       int64_t, uint64_t>
    IntTypes;
INSTANTIATE_TYPED_TEST_SUITE_P(AllInts, BFSTypedTest, IntTypes);

}  // namespace
}  // namespace graph
}  // namespace util
