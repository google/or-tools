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

#include "ortools/graph/bidirectional_dijkstra.h"

#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/container/flat_hash_map.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/graph/bounded_dijkstra.h"
#include "ortools/graph/graph.h"

namespace operations_research {
namespace {

using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::util::ListGraph;
using ::util::StaticGraph;

TEST(BidirectionalDijkstraTest, EmptyPathInspection) {
  ListGraph<> empty_graph;
  std::vector<double> empty_vector;
  BidirectionalDijkstra<ListGraph<>, double> dijkstra(
      &empty_graph, &empty_vector, &empty_graph, &empty_vector);
  const auto path = dijkstra.SetToSetShortestPath({}, {});
  ASSERT_EQ(path.meeting_point, -1);
  EXPECT_THAT(dijkstra.PathToNodePath(path), IsEmpty());
  EXPECT_EQ(dijkstra.PathDebugString(path), "<NO PATH>");
}

TEST(BidirectionalDijkstraTest, SmallTest) {
  // Build a small "grid" graph. Arc indices and lengths of the forward graph
  // are in (); and the backward graph has the same, but reversed arcs.
  //
  //     0 --(#0:0.1)--> 1 --(#1:1.1)--> 2
  //     |               |               |
  //  (#2:0.1)        (#3:0.19)        (#4:0.3)
  //     |               |               |
  //     v               v               v
  //     3 --(#5:0.2)--> 4 --(#6:1.2)--> 5
  ListGraph<> forward_graph;
  std::vector<double> arc_lengths;
  forward_graph.AddArc(0, 1);
  arc_lengths.push_back(0.1);
  forward_graph.AddArc(1, 2);
  arc_lengths.push_back(1.1);
  forward_graph.AddArc(0, 3);
  arc_lengths.push_back(0.1);
  forward_graph.AddArc(1, 4);
  arc_lengths.push_back(0.19);
  forward_graph.AddArc(2, 5);
  arc_lengths.push_back(0.3);
  forward_graph.AddArc(3, 4);
  arc_lengths.push_back(0.2);
  forward_graph.AddArc(4, 5);
  arc_lengths.push_back(1.2);
  ListGraph<> backward_graph;
  for (int arc = 0; arc < forward_graph.num_arcs(); ++arc) {
    backward_graph.AddArc(forward_graph.Head(arc), forward_graph.Tail(arc));
  }
  BidirectionalDijkstra<ListGraph<>, double> dijkstra(
      &forward_graph, &arc_lengths, &backward_graph, &arc_lengths);
  // Since the meeting point may vary, depending on which search direction goes
  // faster, we run it many times to try and exercise more code paths.
  const int kNumPaths = 1000;
  for (int i = 0; i < kNumPaths; ++i) {
    SCOPED_TRACE(absl::StrCat("On attempt #", i));
    const auto path = dijkstra.OneToOneShortestPath(0, 5);
    EXPECT_THAT(dijkstra.PathToNodePath(path), ElementsAre(0, 1, 4, 5));
    ASSERT_THAT(dijkstra.PathDebugString(path),
                AnyOf("0 --(#0:0.1)--> 1 --(#3:0.19)--> 4 --(#6:1.2)--> [5]",
                      "0 --(#0:0.1)--> 1 --(#3:0.19)--> [4] <--(#6:1.2)-- 5",
                      "0 --(#0:0.1)--> [1] <--(#3:0.19)-- 4 <--(#6:1.2)-- 5",
                      "[0] <--(#0:0.1)-- 1 <--(#3:0.19)-- 4 <--(#6:1.2)-- 5"));
  }
}

TEST(BidirectionalDijkstraTest, RandomizedCorrectnessTest) {
  std::mt19937 random(12345);
  const int kNumGraphs = DEBUG_MODE ? 100 : 300;
  const int kNumQueriesPerGraph = DEBUG_MODE ? 10 : 30;
  const int kNumNodes = 1000;
  const int kNumArcs = 10000;
  for (int graph_iter = 0; graph_iter < kNumGraphs; ++graph_iter) {
    // Build the random graphs.
    StaticGraph<> forward_graph;
    StaticGraph<> backward_graph;
    std::vector<double> forward_lengths;
    std::vector<double> backward_lengths;
    std::vector<int> forward_arc_of_backward_arc;
    forward_graph.AddNode(kNumNodes - 1);
    backward_graph.AddNode(kNumNodes - 1);
    for (int i = 0; i < kNumArcs; ++i) {
      const int a = absl::Uniform(random, 0, kNumNodes);
      const int b = absl::Uniform(random, 0, kNumNodes);
      forward_graph.AddArc(a, b);
      backward_graph.AddArc(b, a);
      forward_arc_of_backward_arc.push_back(i);
      const double length = absl::Uniform<double>(random, 0.0, 1.0);
      forward_lengths.push_back(length);
      backward_lengths.push_back(length);
    }
    std::vector<int> forward_perm;
    forward_graph.Build(&forward_perm);
    util::Permute(forward_perm, &forward_lengths);
    for (int& a : forward_arc_of_backward_arc) {
      a = forward_perm[a];
    }
    std::vector<int> backward_perm;
    backward_graph.Build(&backward_perm);
    util::Permute(backward_perm, &backward_lengths);
    util::Permute(backward_perm, &forward_arc_of_backward_arc);

    // Initialize the tested Dijkstra and the reference Dijkstra.
    typedef BidirectionalDijkstra<StaticGraph<>, double> Dijkstra;
    Dijkstra tested_dijkstra(&forward_graph, &forward_lengths, &backward_graph,
                             &backward_lengths);
    BoundedDijkstraWrapper<StaticGraph<>, double> ref_dijkstra(
        &forward_graph, &forward_lengths);

    // To print some debugging info in case the test fails.
    auto print_arc_path = [&](absl::Span<const int> arc_path) -> std::string {
      if (arc_path.empty()) return "<EMPTY>";
      std::string out = absl::StrCat(forward_graph.Tail(arc_path[0]));
      double total_length = 0.0;
      for (const int arc : arc_path) {
        absl::StrAppend(&out, "--(#", arc, ":", (forward_lengths[arc]), ")-->",
                        forward_graph.Head(arc));
        total_length += forward_lengths[arc];
      }
      absl::StrAppend(&out, "; Total length=", (total_length));
      return out;
    };
    auto print_node_distances =
        [&](absl::Span<const Dijkstra::NodeDistance> nds) -> std::string {
      std::string out = "{";
      for (const Dijkstra::NodeDistance& nd : nds) {
        absl::StrAppend(&out, " #", nd.node, " dist=", (nd.distance), ",");
      }
      if (!nds.empty()) out.back() = ' ';  // Replace the last ','.
      out += '}';
      return out;
    };

    // Run random Dijkstra queries.
    for (int q = 0; q < kNumQueriesPerGraph; ++q) {
      const int num_src = ceil(absl::Exponential<double>(random));
      const int num_dst = ceil(absl::Exponential<double>(random));
      std::vector<std::pair<int, double>> ref_srcs;
      std::vector<std::pair<int, double>> ref_dsts;
      std::vector<Dijkstra::NodeDistance> srcs;
      std::vector<Dijkstra::NodeDistance> dsts;
      for (int i = 0; i < num_src; ++i) {
        const int src = absl::Uniform(random, 0, kNumNodes);
        // Try costs < 0.
        const double dist = absl::Uniform<double>(random, 1.0, 2.0);
        ref_srcs.push_back({src, dist});
        srcs.push_back({src, dist});
      }
      for (int i = 0; i < num_dst; ++i) {
        const int dst = absl::Uniform(random, 0, kNumNodes);
        const double dist = absl::Uniform<double>(random, 1.0, 2.0);
        ref_dsts.push_back({dst, dist});
        dsts.push_back({dst, dist});
      }
      const std::vector<int> ref_dests =
          ref_dijkstra
              .RunBoundedDijkstraFromMultipleSourcesToMultipleDestinations(
                  ref_srcs, ref_dsts,
                  /*num_destinations_to_reach=*/1,
                  std::numeric_limits<double>::infinity());
      if (ref_dests.empty()) {
        // No path. Verify that.
        EXPECT_EQ(
            tested_dijkstra.SetToSetShortestPath(srcs, dsts).meeting_point, -1);
        continue;
      }
      const std::vector<int> ref_arc_path =
          ref_dijkstra.ArcPathToNode(ref_dests[0]);
      const auto path = tested_dijkstra.SetToSetShortestPath(srcs, dsts);
      std::vector<int> arc_path = path.forward_arc_path;
      for (const int arc : ::gtl::reversed_view(path.backward_arc_path)) {
        arc_path.push_back(forward_arc_of_backward_arc[arc]);
      }
      ASSERT_THAT(arc_path, ElementsAreArray(ref_arc_path))
          << "On graph #" << graph_iter << ", query #" << q
          << "\nSources     : " << print_node_distances(srcs)
          << "\nDestinations: " << print_node_distances(dsts)
          << "\nReference path : " << print_arc_path(ref_arc_path)
          << "\nObtained path  : " << print_arc_path(arc_path);
    }
  }
}

}  // namespace
}  // namespace operations_research
