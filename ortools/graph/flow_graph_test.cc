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

#include "ortools/graph/flow_graph.h"

#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/stl_util.h"

namespace {

TEST(FlowGraphTest, SelfEdgeAreNotMarkedAsReverseOfEachOther) {
  util::FlowGraph<>::Builder graph_builder;
  graph_builder.AddArc(0, 0);
  const auto graph = std::move(graph_builder).Build(nullptr);
  EXPECT_EQ(graph->num_arcs(), 2);
  EXPECT_EQ(graph->OppositeArc(0), 1);
}

// TODO(user): Here we could mark them as reverse, but the current code do not.
// We expect duplicate self-edge to be rare in any case, so not too important.
TEST(FlowGraphTest, TwoSelfEdgeCanBeEachOtherReverse) {
  util::FlowGraph<>::Builder graph_builder;
  graph_builder.AddArc(0, 0);
  graph_builder.AddArc(0, 0);
  const auto graph = std::move(graph_builder).Build(nullptr);
  EXPECT_EQ(graph->num_arcs(), 4);
  EXPECT_EQ(graph->OppositeArc(0), 2);
  EXPECT_EQ(graph->OppositeArc(1), 3);
}

TEST(FlowGraphTest, MultiArcsAreSupported) {
  util::FlowGraph<>::Builder graph_builder;
  graph_builder.AddArc(0, 1);
  graph_builder.AddArc(0, 1);
  graph_builder.AddArc(1, 0);  // There is a single reverse.
  const auto graph = std::move(graph_builder).Build(nullptr);
  EXPECT_EQ(graph->num_arcs(), 4);
  EXPECT_EQ(graph->OppositeArc(0), 2);
  EXPECT_EQ(graph->OppositeArc(1), 3);
}

TEST(FlowGraphTest, RandomGraphNoDetection) {
  const int num_nodes = 10'000;
  const int num_arcs = 100 * num_nodes;

  // Lets create a random graph with possible self-arc or duplicates.
  absl::BitGen random;
  auto graph_builder =
      util::FlowGraph<>::Builder().SetDetectReverse(false).SetSortByHead(true);
  std::vector<std::vector<int>> reference(num_nodes);
  for (int i = 0; i < num_arcs; ++i) {
    const int tail = absl::Uniform<int>(random, 0, num_nodes);
    const int head = absl::Uniform<int>(random, 0, num_nodes);
    graph_builder.AddArc(tail, head);

    // Note that for each arc we add one reverse in the "reference".
    reference[tail].push_back(head);
    reference[head].push_back(tail);
  }

  std::vector<int> permutation;
  const auto graph_ptr = std::move(graph_builder).Build(&permutation);
  const auto& graph = *graph_ptr;

  EXPECT_EQ(permutation.size(), graph.num_arcs());
  std::vector<int> reverse_perm(graph.num_arcs());
  for (int i = 0; i < reverse_perm.size(); ++i) {
    reverse_perm[permutation[i]] = i;
  }

  // Without detection, we should have exactly twice more arcs.
  EXPECT_EQ(2 * num_arcs, graph.num_arcs());

  std::vector<int> adjacent_head;
  std::vector<bool> seen(graph.num_arcs());
  for (int node = 0; node < num_nodes; ++node) {
    adjacent_head.clear();
    for (const int arc : graph.OutgoingArcs(node)) {
      ASSERT_EQ(graph.Tail(arc), node);
      adjacent_head.push_back(graph.Head(arc));

      // test the reverse interface.
      const int reverse_arc = graph.OppositeArc(arc);

      // head <-> tail should be swapped.
      ASSERT_EQ(graph.Tail(arc), graph.Head(reverse_arc));
      ASSERT_EQ(graph.Tail(reverse_arc), graph.Head(arc));

      // Each arc should have an unique reverse.
      ASSERT_FALSE(seen[reverse_arc]);
      seen[reverse_arc] = true;

      // The reverse of a reverse should be the original arc.
      // Arc should be different from its reverse.
      ASSERT_NE(arc, reverse_arc);
      ASSERT_EQ(graph.OppositeArc(reverse_arc), arc);

      // If we don't have detection, all original reverse indices should be
      // after num_arcs.
      int a = reverse_perm[arc];
      int b = reverse_perm[reverse_arc];
      if (a > b) std::swap(a, b);
      ASSERT_LT(a, num_arcs);
      ASSERT_GE(b, num_arcs);
    }

    // Note that with this Build() option, heads should be sorted.
    absl::c_sort(reference[node]);
    ASSERT_EQ(adjacent_head, reference[node]);
  }
}

// This one is similar, but we generate a lot more arcs to exercise the
// detection.
TEST(FlowGraphTest, RandomGraphDetection) {
  const int num_nodes = 1'000;
  const int num_arcs = num_nodes * num_nodes;

  // Lets create a random graph with possible self-arc or duplicates.
  absl::BitGen random;
  auto graph_builder =
      util::FlowGraph<>::Builder().SetDetectReverse(true).SetSortByHead(true);
  std::vector<std::vector<int>> reference(num_nodes);
  for (int i = 0; i < num_arcs; ++i) {
    const int tail = absl::Uniform<int>(random, 0, num_nodes);
    const int head = absl::Uniform<int>(random, 0, num_nodes);
    graph_builder.AddArc(tail, head);

    // Note that for each arc we add one reverse in the "reference".
    reference[tail].push_back(head);
    reference[head].push_back(tail);
  }

  std::vector<int> permutation;
  const auto graph_ptr = std::move(graph_builder).Build(&permutation);
  const auto& graph = *graph_ptr;
  EXPECT_EQ(permutation.size(), graph.num_arcs());

  // with detection, we should have less than twice.
  EXPECT_LT(graph.num_arcs(), 2 * num_arcs);
  LOG(INFO) << "actual num arcs : " << graph.num_arcs() << " /  "
            << 2 * num_arcs;

  std::vector<int> adjacent_head;
  std::vector<bool> seen(graph.num_arcs());
  for (int node = 0; node < num_nodes; ++node) {
    adjacent_head.clear();
    for (const int arc : graph.OutgoingArcs(node)) {
      ASSERT_EQ(graph.Tail(arc), node);
      adjacent_head.push_back(graph.Head(arc));

      // test the reverse interface.
      const int reverse_arc = graph.OppositeArc(arc);

      // head <-> tail should be swapped.
      ASSERT_EQ(graph.Tail(arc), graph.Head(reverse_arc));
      ASSERT_EQ(graph.Tail(reverse_arc), graph.Head(arc));

      // Each arc should have an unique reverse.
      ASSERT_FALSE(seen[reverse_arc]);
      seen[reverse_arc] = true;

      // The reverse of a reverse should be the original arc.
      // Arc should be different from its reverse.
      ASSERT_NE(arc, reverse_arc);
      ASSERT_EQ(graph.OppositeArc(reverse_arc), arc);
    }

    // TODO(user): With reverse detection, it is harder to compare with a
    // reference, for sure the de-duplicated adjacency list should be the same.
    gtl::STLSortAndRemoveDuplicates(&adjacent_head);
    gtl::STLSortAndRemoveDuplicates(&reference[node]);
    ASSERT_EQ(adjacent_head, reference[node]);
  }
}

}  // namespace
