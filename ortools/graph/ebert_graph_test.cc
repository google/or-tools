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

#include "ortools/graph/ebert_graph.h"

#include <cstdint>
#include <random>
#include <string>

#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/util/permutation.h"

namespace operations_research {

template <typename GraphType>
std::string Stringify(const GraphType& graph,
                      typename GraphType::ArcIndex arc) {
  return absl::StrCat("    Arc ", arc, ": ", graph.Tail(arc), " -> ",
                      graph.Head(arc), "\n");
}

template <typename GraphType>
std::string Stringify(const GraphType& graph,
                      typename GraphType::NodeIndex tail,
                      typename GraphType::ArcIndex arc) {
  return absl::StrCat("    Arc ", arc, ": ", tail, " -> ", graph.Head(arc),
                      "\n");
}

// We have to base the TestEbertGraph utility function template on a
// class/struct template because partial specialization is not allowed for
// function templates, and we use partial specialization to turn off reverse-arc
// testing for graph representations that don't have reverse arcs.
//
// First, we have the complete case for graphs that represent their reverse arcs
// and therefore can be more thoroughly tested. The constructor does all the
// testing work.
template <typename GraphType, bool has_reverse_arcs, bool is_dynamic>
struct TestEbertGraphRunner {
  TestEbertGraphRunner(
      const GraphType& graph, absl::string_view expected_graph_arc_list,
      absl::string_view expected_adjacency_list,
      absl::string_view expected_incoming_arc_list,
      absl::string_view expected_outgoing_arc_list,
      absl::string_view expected_debug_string,
      absl::string_view unused_expected_forward_debug_string,
      absl::string_view unused_expected_forward_static_debug_string) {
    std::string graph_arc_list = "";
    for (typename GraphType::ArcIterator arc_it(graph); arc_it.Ok();
         arc_it.Next()) {
      typename GraphType::ArcIndex arc = arc_it.Index();
      absl::StrAppend(&graph_arc_list, Stringify(graph, arc));
      EXPECT_EQ(graph.DirectArc(arc), graph.Opposite(graph.ReverseArc(arc)));
    }
    EXPECT_EQ(expected_graph_arc_list, graph_arc_list);

    std::string adjacency_list = "";
    for (typename GraphType::NodeIterator node_it(graph); node_it.Ok();
         node_it.Next()) {
      typename GraphType::NodeIndex node = node_it.Index();
      absl::StrAppend(&adjacency_list, "  Node ", node, ":\n");
      for (typename GraphType::OutgoingOrOppositeIncomingArcIterator arc_it(
               graph, node);
           arc_it.Ok(); arc_it.Next()) {
        typename GraphType::ArcIndex arc = arc_it.Index();
        EXPECT_TRUE(graph.IsOutgoingOrOppositeIncoming(arc, node));
        absl::StrAppend(&adjacency_list, Stringify(graph, arc));
        EXPECT_EQ(node, graph.Tail(arc));
      }
    }
    EXPECT_EQ(expected_adjacency_list, adjacency_list);

    std::string incoming_arc_list = "";
    for (typename GraphType::NodeIterator node_it(graph); node_it.Ok();
         node_it.Next()) {
      typename GraphType::NodeIndex node = node_it.Index();
      absl::StrAppend(&incoming_arc_list, "  Node ", node, ":\n");
      for (typename GraphType::IncomingArcIterator arc_it(graph, node);
           arc_it.Ok(); arc_it.Next()) {
        typename GraphType::ArcIndex arc = arc_it.Index();
        EXPECT_TRUE(graph.IsIncoming(arc, node));
        // We assume there are no self-loops in the graph.
        EXPECT_FALSE(graph.IsOutgoing(arc, node));
        absl::StrAppend(&incoming_arc_list, Stringify(graph, arc));
        EXPECT_FALSE(graph.IsReverse(arc));
        EXPECT_EQ(node, graph.Head(arc));
      }
    }
    EXPECT_EQ(expected_incoming_arc_list, incoming_arc_list);

    std::string outgoing_arc_list = "";
    for (typename GraphType::NodeIterator node_it(graph); node_it.Ok();
         node_it.Next()) {
      typename GraphType::NodeIndex node = node_it.Index();
      absl::StrAppend(&outgoing_arc_list, "  Node ", node, ":\n");
      for (typename GraphType::OutgoingArcIterator arc_it(graph, node);
           arc_it.Ok(); arc_it.Next()) {
        typename GraphType::ArcIndex arc = arc_it.Index();
        // We assume there are no self-loops in the graph.
        EXPECT_FALSE(graph.IsIncoming(arc, node));
        EXPECT_TRUE(graph.IsOutgoing(arc, node));
        absl::StrAppend(&outgoing_arc_list, Stringify(graph, arc));
        EXPECT_TRUE(graph.IsDirect(arc));
        EXPECT_EQ(node, graph.Tail(arc));
        EXPECT_EQ(node, graph.DirectArcTail(arc));
      }
    }
    EXPECT_EQ(expected_outgoing_arc_list, outgoing_arc_list);
    EXPECT_EQ(expected_debug_string, graph.DebugString());
  }
};

// Case for graphs that don't have their reverse arcs and therefore cannot be
// tested as completely as if they did, but that are dynamic and so can be
// tested somewhat more than if they weren't. Again, the constructor does all
// the testing work.
template <typename GraphType>
struct TestEbertGraphRunner<GraphType, false, true> {
  TestEbertGraphRunner(
      const GraphType& graph, absl::string_view unused_expected_graph_arc_list,
      absl::string_view unused_expected_adjacency_list,
      absl::string_view unused_expected_incoming_arc_list,
      absl::string_view expected_outgoing_arc_list,
      absl::string_view unused_expected_debug_string,
      absl::string_view expected_forward_debug_string,
      absl::string_view unused_expected_forward_static_debug_string) {
    std::string outgoing_arc_list = "";
    for (typename GraphType::NodeIterator node_it(graph); node_it.Ok();
         node_it.Next()) {
      typename GraphType::NodeIndex node = node_it.Index();
      absl::StrAppend(&outgoing_arc_list, "  Node ", node, ":\n");
      for (typename GraphType::OutgoingArcIterator arc_it(graph, node);
           arc_it.Ok(); arc_it.Next()) {
        typename GraphType::ArcIndex arc = arc_it.Index();
        // We assume no self-loops in the graph.
        EXPECT_FALSE(graph.IsIncoming(arc, node));
        absl::StrAppend(&outgoing_arc_list, Stringify(graph, node, arc));
      }
    }
    EXPECT_EQ(expected_outgoing_arc_list, outgoing_arc_list);
    EXPECT_EQ(expected_forward_debug_string, graph.DebugString());
  }
};

// Case for graphs that don't have their reverse arcs and are static. Due to
// these restrictions, there is less to check about these graphs than for the
// more complicated kinds of graphs.
template <typename GraphType>
struct TestEbertGraphRunner<GraphType, false, false> {
  TestEbertGraphRunner(const GraphType& graph,
                       absl::string_view unused_expected_graph_arc_list,
                       absl::string_view unused_expected_adjacency_list,
                       absl::string_view unused_expected_incoming_arc_list,
                       absl::string_view unused_expected_outgoing_arc_list,
                       absl::string_view unused_expected_debug_string,
                       absl::string_view unused_expected_forward_debug_string,
                       absl::string_view expected_forward_static_debug_string) {
    EXPECT_EQ(expected_forward_static_debug_string, graph.DebugString());
  }
};

// Tests that various string representations of the given graph match
// the given strings.
template <typename GraphType>
void TestEbertGraph(const GraphType& graph,
                    absl::string_view expected_graph_arc_list,
                    absl::string_view expected_adjacency_list,
                    absl::string_view expected_incoming_arc_list,
                    absl::string_view expected_outgoing_arc_list,
                    absl::string_view expected_debug_string,
                    absl::string_view expected_forward_debug_string,
                    absl::string_view expected_forward_static_debug_string) {
  TestEbertGraphRunner<GraphType, graph_traits<GraphType>::has_reverse_arcs,
                       graph_traits<GraphType>::is_dynamic>
      test_object(graph, expected_graph_arc_list, expected_adjacency_list,
                  expected_incoming_arc_list, expected_outgoing_arc_list,
                  expected_debug_string, expected_forward_debug_string,
                  expected_forward_static_debug_string);
}

template <typename GraphType>
class DebugStringEbertGraphTest : public ::testing::Test {};

typedef ::testing::Types<EbertGraph<int16_t, int16_t> >
    EbertGraphTypesForDebugStringTesting;

TYPED_TEST_SUITE(DebugStringEbertGraphTest,
                 EbertGraphTypesForDebugStringTesting);

TYPED_TEST(DebugStringEbertGraphTest, Test1) {
  TypeParam graph(4, 6);
  graph.AddArc(0, 1);
  graph.AddArc(0, 2);
  graph.AddArc(1, 3);
  graph.AddArc(2, 3);
  graph.AddArc(2, 1);
  graph.AddArc(1, 2);

  const std::string kGraphArcList =
      "    Arc 0: 0 -> 1\n"
      "    Arc 1: 0 -> 2\n"
      "    Arc 2: 1 -> 3\n"
      "    Arc 3: 2 -> 3\n"
      "    Arc 4: 2 -> 1\n"
      "    Arc 5: 1 -> 2\n";

  const std::string kExpectedAdjacencyList =
      "  Node 0:\n"
      "    Arc 1: 0 -> 2\n"
      "    Arc 0: 0 -> 1\n"
      "  Node 1:\n"
      "    Arc 5: 1 -> 2\n"
      "    Arc -5: 1 -> 2\n"
      "    Arc 2: 1 -> 3\n"
      "    Arc -1: 1 -> 0\n"
      "  Node 2:\n"
      "    Arc -6: 2 -> 1\n"
      "    Arc 4: 2 -> 1\n"
      "    Arc 3: 2 -> 3\n"
      "    Arc -2: 2 -> 0\n"
      "  Node 3:\n"
      "    Arc -4: 3 -> 2\n"
      "    Arc -3: 3 -> 1\n";

  const std::string kExpectedIncomingArcList =
      "  Node 0:\n"
      "  Node 1:\n"
      "    Arc 4: 2 -> 1\n"
      "    Arc 0: 0 -> 1\n"
      "  Node 2:\n"
      "    Arc 5: 1 -> 2\n"
      "    Arc 1: 0 -> 2\n"
      "  Node 3:\n"
      "    Arc 3: 2 -> 3\n"
      "    Arc 2: 1 -> 3\n";

  const std::string kExpectedOutgoingArcList =
      "  Node 0:\n"
      "    Arc 1: 0 -> 2\n"
      "    Arc 0: 0 -> 1\n"
      "  Node 1:\n"
      "    Arc 5: 1 -> 2\n"
      "    Arc 2: 1 -> 3\n"
      "  Node 2:\n"
      "    Arc 4: 2 -> 1\n"
      "    Arc 3: 2 -> 3\n"
      "  Node 3:\n";

  const std::string kExpectedDebugString =
      "Arcs:(node, next arc) :\n"
      " -6:(1,4)\n"
      " -5:(2,2)\n"
      " -4:(2,-3)\n"
      " -3:(1,NilArc)\n"
      " -2:(0,NilArc)\n"
      " -1:(0,NilArc)\n"
      " 0:(1,NilArc)\n"
      " 1:(2,0)\n"
      " 2:(3,-1)\n"
      " 3:(3,-2)\n"
      " 4:(1,3)\n"
      " 5:(2,-5)\n"
      "Node:First arc :\n"
      " 0:1\n"
      " 1:5\n"
      " 2:-6\n"
      " 3:-4\n";

  const std::string kExpectedForwardDebugString =
      "Arcs:(node, next arc) :\n"
      " 0:(1,NilArc)\n"
      " 1:(2,0)\n"
      " 2:(3,NilArc)\n"
      " 3:(3,NilArc)\n"
      " 4:(1,3)\n"
      " 5:(2,2)\n"
      "Node:First arc :\n"
      " 0:1\n"
      " 1:5\n"
      " 2:4\n"
      " 3:NilArc\n";

  TestEbertGraph(graph, kGraphArcList, kExpectedAdjacencyList,
                 kExpectedIncomingArcList, kExpectedOutgoingArcList,
                 kExpectedDebugString, kExpectedForwardDebugString, "");
}

// Unfortunately, this class template has to be defined outside the Test2 method
// where it is used, or a compiler bug gets tickled.
template <typename GraphType>
class ArcFunctorByHead {
 public:
  explicit ArcFunctorByHead(const GraphType& graph) : graph_(graph) {}

  bool operator()(typename GraphType::ArcIndex a,
                  typename GraphType::ArcIndex b) const {
    return ((graph_.Head(a) < graph_.Head(b)) ||
            ((graph_.Head(a) == graph_.Head(b)) &&
             (graph_.Tail(a) < graph_.Tail(b))));
  }

 private:
  const GraphType& graph_;
};

// Unfortunately this class template has to be defined outside the Test2 method
// where it is used, or a compiler bug gets tickled.
template <typename GraphType>
class ArcFunctorByTail {
 public:
  explicit ArcFunctorByTail(const GraphType& graph) : graph_(graph) {}

  bool operator()(typename GraphType::ArcIndex a,
                  typename GraphType::ArcIndex b) const {
    return ((graph_.Tail(a) < graph_.Tail(b)) ||
            ((graph_.Tail(a) == graph_.Tail(b)) &&
             (graph_.Head(a) < graph_.Head(b))));
  }

 private:
  const GraphType& graph_;
};

TYPED_TEST(DebugStringEbertGraphTest, Test2) {
  TypeParam graph(3, 6);
  graph.AddArc(0, 1);
  graph.AddArc(1, 0);
  graph.AddArc(1, 2);
  graph.AddArc(2, 1);
  graph.AddArc(0, 2);
  graph.AddArc(2, 0);

  const std::string kGraphArcList =
      "    Arc 0: 0 -> 1\n"
      "    Arc 1: 1 -> 0\n"
      "    Arc 2: 1 -> 2\n"
      "    Arc 3: 2 -> 1\n"
      "    Arc 4: 0 -> 2\n"
      "    Arc 5: 2 -> 0\n";

  const std::string kExpectedAdjacencyList =
      "  Node 0:\n"
      "    Arc -6: 0 -> 2\n"
      "    Arc 4: 0 -> 2\n"
      "    Arc -2: 0 -> 1\n"
      "    Arc 0: 0 -> 1\n"
      "  Node 1:\n"
      "    Arc -4: 1 -> 2\n"
      "    Arc 2: 1 -> 2\n"
      "    Arc 1: 1 -> 0\n"
      "    Arc -1: 1 -> 0\n"
      "  Node 2:\n"
      "    Arc 5: 2 -> 0\n"
      "    Arc -5: 2 -> 0\n"
      "    Arc 3: 2 -> 1\n"
      "    Arc -3: 2 -> 1\n";

  const std::string kExpectedIncomingArcList =
      "  Node 0:\n"
      "    Arc 5: 2 -> 0\n"
      "    Arc 1: 1 -> 0\n"
      "  Node 1:\n"
      "    Arc 3: 2 -> 1\n"
      "    Arc 0: 0 -> 1\n"
      "  Node 2:\n"
      "    Arc 4: 0 -> 2\n"
      "    Arc 2: 1 -> 2\n";

  const std::string kExpectedOutgoingArcList =
      "  Node 0:\n"
      "    Arc 4: 0 -> 2\n"
      "    Arc 0: 0 -> 1\n"
      "  Node 1:\n"
      "    Arc 2: 1 -> 2\n"
      "    Arc 1: 1 -> 0\n"
      "  Node 2:\n"
      "    Arc 5: 2 -> 0\n"
      "    Arc 3: 2 -> 1\n";

  const std::string kExpectedDebugString =
      "Arcs:(node, next arc) :\n"
      " -6:(2,4)\n"
      " -5:(0,3)\n"
      " -4:(2,2)\n"
      " -3:(1,NilArc)\n"
      " -2:(1,0)\n"
      " -1:(0,NilArc)\n"
      " 0:(1,NilArc)\n"
      " 1:(0,-1)\n"
      " 2:(2,1)\n"
      " 3:(1,-3)\n"
      " 4:(2,-2)\n"
      " 5:(0,-5)\n"
      "Node:First arc :\n"
      " 0:-6\n"
      " 1:-4\n"
      " 2:5\n";

  const std::string kExpectedForwardDebugString =
      "Arcs:(node, next arc) :\n"
      " 0:(1,NilArc)\n"
      " 1:(0,NilArc)\n"
      " 2:(2,1)\n"
      " 3:(1,NilArc)\n"
      " 4:(2,0)\n"
      " 5:(0,3)\n"
      "Node:First arc :\n"
      " 0:4\n"
      " 1:2\n"
      " 2:5\n";

  TestEbertGraph(graph, kGraphArcList, kExpectedAdjacencyList,
                 kExpectedIncomingArcList, kExpectedOutgoingArcList,
                 kExpectedDebugString, kExpectedForwardDebugString, "");

  graph.GroupForwardArcsByFunctor(ArcFunctorByHead<TypeParam>(graph), nullptr);

  const std::string kGraphHeadGroupedArcList =
      "    Arc 0: 1 -> 0\n"
      "    Arc 1: 2 -> 0\n"
      "    Arc 2: 0 -> 1\n"
      "    Arc 3: 2 -> 1\n"
      "    Arc 4: 0 -> 2\n"
      "    Arc 5: 1 -> 2\n";

  const std::string kExpectedHeadGroupedAdjacencyList =
      "  Node 0:\n"
      "    Arc 4: 0 -> 2\n"
      "    Arc 2: 0 -> 1\n"
      "    Arc -2: 0 -> 2\n"
      "    Arc -1: 0 -> 1\n"
      "  Node 1:\n"
      "    Arc 5: 1 -> 2\n"
      "    Arc -4: 1 -> 2\n"
      "    Arc -3: 1 -> 0\n"
      "    Arc 0: 1 -> 0\n"
      "  Node 2:\n"
      "    Arc -6: 2 -> 1\n"
      "    Arc -5: 2 -> 0\n"
      "    Arc 3: 2 -> 1\n"
      "    Arc 1: 2 -> 0\n";

  const std::string kExpectedHeadGroupedIncomingArcList =
      "  Node 0:\n"
      "    Arc 1: 2 -> 0\n"
      "    Arc 0: 1 -> 0\n"
      "  Node 1:\n"
      "    Arc 3: 2 -> 1\n"
      "    Arc 2: 0 -> 1\n"
      "  Node 2:\n"
      "    Arc 5: 1 -> 2\n"
      "    Arc 4: 0 -> 2\n";

  const std::string kExpectedHeadGroupedOutgoingArcList =
      "  Node 0:\n"
      "    Arc 4: 0 -> 2\n"
      "    Arc 2: 0 -> 1\n"
      "  Node 1:\n"
      "    Arc 5: 1 -> 2\n"
      "    Arc 0: 1 -> 0\n"
      "  Node 2:\n"
      "    Arc 3: 2 -> 1\n"
      "    Arc 1: 2 -> 0\n";

  const std::string kExpectedHeadGroupedDebugString =
      "Arcs:(node, next arc) :\n"
      " -6:(1,-5)\n"
      " -5:(0,3)\n"
      " -4:(2,-3)\n"
      " -3:(0,0)\n"
      " -2:(2,-1)\n"
      " -1:(1,NilArc)\n"
      " 0:(0,NilArc)\n"
      " 1:(0,NilArc)\n"
      " 2:(1,-2)\n"
      " 3:(1,1)\n"
      " 4:(2,2)\n"
      " 5:(2,-4)\n"
      "Node:First arc :\n"
      " 0:4\n"
      " 1:5\n"
      " 2:-6\n";

  const std::string kExpectedHeadGroupedForwardDebugString =
      "Arcs:(node, next arc) :\n"
      " 0:(0,NilArc)\n"
      " 1:(0,NilArc)\n"
      " 2:(1,NilArc)\n"
      " 3:(1,1)\n"
      " 4:(2,2)\n"
      " 5:(2,0)\n"
      "Node:First arc :\n"
      " 0:4\n"
      " 1:5\n"
      " 2:3\n";

  TestEbertGraph(
      graph, kGraphHeadGroupedArcList, kExpectedHeadGroupedAdjacencyList,
      kExpectedHeadGroupedIncomingArcList, kExpectedHeadGroupedOutgoingArcList,
      kExpectedHeadGroupedDebugString, kExpectedHeadGroupedForwardDebugString,
      "");

  // Test that the GroupForwardArcsByFunctor method correctly permutes arc
  // annotation data.
  int arc_annotations[] = {103, 105, 101, 106, 102, 104};
  ArrayIndexCycleHandler<int, typename TypeParam::ArcIndex> handler(
      arc_annotations);
  graph.GroupForwardArcsByFunctor(ArcFunctorByTail<TypeParam>(graph), &handler);

  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(101 + i, arc_annotations[i]);
  }

  const std::string kGraphTailGroupedArcList =
      "    Arc 0: 0 -> 1\n"
      "    Arc 1: 0 -> 2\n"
      "    Arc 2: 1 -> 0\n"
      "    Arc 3: 1 -> 2\n"
      "    Arc 4: 2 -> 0\n"
      "    Arc 5: 2 -> 1\n";

  const std::string kExpectedTailGroupedAdjacencyList =
      "  Node 0:\n"
      "    Arc -5: 0 -> 2\n"
      "    Arc -3: 0 -> 1\n"
      "    Arc 1: 0 -> 2\n"
      "    Arc 0: 0 -> 1\n"
      "  Node 1:\n"
      "    Arc -6: 1 -> 2\n"
      "    Arc 3: 1 -> 2\n"
      "    Arc 2: 1 -> 0\n"
      "    Arc -1: 1 -> 0\n"
      "  Node 2:\n"
      "    Arc 5: 2 -> 1\n"
      "    Arc 4: 2 -> 0\n"
      "    Arc -4: 2 -> 1\n"
      "    Arc -2: 2 -> 0\n";

  const std::string kExpectedTailGroupedIncomingArcList =
      "  Node 0:\n"
      "    Arc 4: 2 -> 0\n"
      "    Arc 2: 1 -> 0\n"
      "  Node 1:\n"
      "    Arc 5: 2 -> 1\n"
      "    Arc 0: 0 -> 1\n"
      "  Node 2:\n"
      "    Arc 3: 1 -> 2\n"
      "    Arc 1: 0 -> 2\n";

  const std::string kExpectedTailGroupedOutgoingArcList =
      "  Node 0:\n"
      "    Arc 1: 0 -> 2\n"
      "    Arc 0: 0 -> 1\n"
      "  Node 1:\n"
      "    Arc 3: 1 -> 2\n"
      "    Arc 2: 1 -> 0\n"
      "  Node 2:\n"
      "    Arc 5: 2 -> 1\n"
      "    Arc 4: 2 -> 0\n";

  const std::string kExpectedTailGroupedDebugString =
      "Arcs:(node, next arc) :\n"
      " -6:(2,3)\n"
      " -5:(2,-3)\n"
      " -4:(1,-2)\n"
      " -3:(1,1)\n"
      " -2:(0,NilArc)\n"
      " -1:(0,NilArc)\n"
      " 0:(1,NilArc)\n"
      " 1:(2,0)\n"
      " 2:(0,-1)\n"
      " 3:(2,2)\n"
      " 4:(0,-4)\n"
      " 5:(1,4)\n"
      "Node:First arc :\n"
      " 0:-5\n"
      " 1:-6\n"
      " 2:5\n";

  const std::string kExpectedTailGroupedForwardDebugString =
      "Arcs:(node, next arc) :\n"
      " 0:(1,NilArc)\n"
      " 1:(2,0)\n"
      " 2:(0,NilArc)\n"
      " 3:(2,2)\n"
      " 4:(0,NilArc)\n"
      " 5:(1,4)\n"
      "Node:First arc :\n"
      " 0:1\n"
      " 1:3\n"
      " 2:5\n";

  TestEbertGraph(
      graph, kGraphTailGroupedArcList, kExpectedTailGroupedAdjacencyList,
      kExpectedTailGroupedIncomingArcList, kExpectedTailGroupedOutgoingArcList,
      kExpectedTailGroupedDebugString, kExpectedTailGroupedForwardDebugString,
      "");
}

template <typename GraphType>
class DebugStringTestWithGraphBuildManager : public ::testing::Test {};

typedef ::testing::Types<EbertGraph<int16_t, int16_t> >
    GraphTypesForDebugStringTestWithGraphBuildManager;

TYPED_TEST_SUITE(DebugStringTestWithGraphBuildManager,
                 GraphTypesForDebugStringTestWithGraphBuildManager);

TYPED_TEST(DebugStringTestWithGraphBuildManager,
           UnsortedArcsWithoutAnnotation) {
  AnnotatedGraphBuildManager<TypeParam>* builder =
      new AnnotatedGraphBuildManager<TypeParam>(
          4, 6, false /* don't sort adjacency lists */);

  EXPECT_EQ(0, builder->AddArc(0, 2));
  EXPECT_EQ(1, builder->AddArc(2, 0));
  EXPECT_EQ(2, builder->AddArc(2, 3));
  EXPECT_EQ(3, builder->AddArc(3, 2));
  EXPECT_EQ(4, builder->AddArc(0, 3));
  EXPECT_EQ(5, builder->AddArc(3, 0));

  const TypeParam* graph = builder->Graph(nullptr);
  ASSERT_TRUE(graph != nullptr);

  const std::string kGraphArcList =
      "    Arc 0: 0 -> 2\n"
      "    Arc 1: 2 -> 0\n"
      "    Arc 2: 2 -> 3\n"
      "    Arc 3: 3 -> 2\n"
      "    Arc 4: 0 -> 3\n"
      "    Arc 5: 3 -> 0\n";

  const std::string kExpectedAdjacencyList =
      "  Node 0:\n"
      "    Arc -6: 0 -> 3\n"
      "    Arc 4: 0 -> 3\n"
      "    Arc -2: 0 -> 2\n"
      "    Arc 0: 0 -> 2\n"
      "  Node 1:\n"
      "  Node 2:\n"
      "    Arc -4: 2 -> 3\n"
      "    Arc 2: 2 -> 3\n"
      "    Arc 1: 2 -> 0\n"
      "    Arc -1: 2 -> 0\n"
      "  Node 3:\n"
      "    Arc 5: 3 -> 0\n"
      "    Arc -5: 3 -> 0\n"
      "    Arc 3: 3 -> 2\n"
      "    Arc -3: 3 -> 2\n";

  const std::string kExpectedIncomingArcList =
      "  Node 0:\n"
      "    Arc 5: 3 -> 0\n"
      "    Arc 1: 2 -> 0\n"
      "  Node 1:\n"
      "  Node 2:\n"
      "    Arc 3: 3 -> 2\n"
      "    Arc 0: 0 -> 2\n"
      "  Node 3:\n"
      "    Arc 4: 0 -> 3\n"
      "    Arc 2: 2 -> 3\n";

  const std::string kExpectedOutgoingArcList =
      "  Node 0:\n"
      "    Arc 4: 0 -> 3\n"
      "    Arc 0: 0 -> 2\n"
      "  Node 1:\n"
      "  Node 2:\n"
      "    Arc 2: 2 -> 3\n"
      "    Arc 1: 2 -> 0\n"
      "  Node 3:\n"
      "    Arc 5: 3 -> 0\n"
      "    Arc 3: 3 -> 2\n";

  const std::string kExpectedDebugString =
      "Arcs:(node, next arc) :\n"
      " -6:(3,4)\n"
      " -5:(0,3)\n"
      " -4:(3,2)\n"
      " -3:(2,NilArc)\n"
      " -2:(2,0)\n"
      " -1:(0,NilArc)\n"
      " 0:(2,NilArc)\n"
      " 1:(0,-1)\n"
      " 2:(3,1)\n"
      " 3:(2,-3)\n"
      " 4:(3,-2)\n"
      " 5:(0,-5)\n"
      "Node:First arc :\n"
      " 0:-6\n"
      " 1:NilArc\n"
      " 2:-4\n"
      " 3:5\n";

  const std::string kExpectedForwardDebugString =
      "Arcs:(node, next arc) :\n"
      " 0:(2,NilArc)\n"
      " 1:(0,NilArc)\n"
      " 2:(3,1)\n"
      " 3:(2,NilArc)\n"
      " 4:(3,0)\n"
      " 5:(0,3)\n"
      "Node:First arc :\n"
      " 0:4\n"
      " 1:NilArc\n"
      " 2:2\n"
      " 3:5\n";

  const std::string kExpectedForwardStaticDebugString =
      "Arcs:(node) :\n"
      " 0:(2)\n"
      " 1:(3)\n"
      " 2:(0)\n"
      " 3:(3)\n"
      " 4:(2)\n"
      " 5:(0)\n"
      "Node:First arc :\n"
      " 0:0\n"
      " 1:2\n"
      " 2:2\n"
      " 3:4\n"
      " 4:6\n";

  TestEbertGraph(*graph, kGraphArcList, kExpectedAdjacencyList,
                 kExpectedIncomingArcList, kExpectedOutgoingArcList,
                 kExpectedDebugString, kExpectedForwardDebugString,
                 kExpectedForwardStaticDebugString);

  delete graph;
}

TYPED_TEST(DebugStringTestWithGraphBuildManager, SortedArcsWithAnnotation) {
  AnnotatedGraphBuildManager<TypeParam>* builder =
      new AnnotatedGraphBuildManager<TypeParam>(
          4, 6, true /* sort adjacency lists */);

  EXPECT_EQ(0, builder->AddArc(0, 2));
  EXPECT_EQ(1, builder->AddArc(2, 0));
  EXPECT_EQ(2, builder->AddArc(2, 3));
  EXPECT_EQ(3, builder->AddArc(3, 2));
  EXPECT_EQ(4, builder->AddArc(0, 3));
  EXPECT_EQ(5, builder->AddArc(3, 0));

  // Test that the graph building and arc sorting operations correctly
  // permute arc annotation data.
  int arc_annotations[] = {101, 103, 104, 106, 102, 105};
  ArrayIndexCycleHandler<int, typename TypeParam::ArcIndex> handler(
      arc_annotations);
  const TypeParam* graph = builder->Graph(&handler);
  ASSERT_TRUE(graph != nullptr);
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(101 + i, arc_annotations[i]);
  }

  const std::string kGraphArcList =
      "    Arc 0: 0 -> 2\n"
      "    Arc 1: 0 -> 3\n"
      "    Arc 2: 2 -> 0\n"
      "    Arc 3: 2 -> 3\n"
      "    Arc 4: 3 -> 0\n"
      "    Arc 5: 3 -> 2\n";

  const std::string kExpectedAdjacencyList =
      "  Node 0:\n"
      "    Arc -5: 0 -> 3\n"
      "    Arc -3: 0 -> 2\n"
      "    Arc 1: 0 -> 3\n"
      "    Arc 0: 0 -> 2\n"
      "  Node 1:\n"
      "  Node 2:\n"
      "    Arc -6: 2 -> 3\n"
      "    Arc 3: 2 -> 3\n"
      "    Arc 2: 2 -> 0\n"
      "    Arc -1: 2 -> 0\n"
      "  Node 3:\n"
      "    Arc 5: 3 -> 2\n"
      "    Arc 4: 3 -> 0\n"
      "    Arc -4: 3 -> 2\n"
      "    Arc -2: 3 -> 0\n";

  const std::string kExpectedIncomingArcList =
      "  Node 0:\n"
      "    Arc 4: 3 -> 0\n"
      "    Arc 2: 2 -> 0\n"
      "  Node 1:\n"
      "  Node 2:\n"
      "    Arc 5: 3 -> 2\n"
      "    Arc 0: 0 -> 2\n"
      "  Node 3:\n"
      "    Arc 3: 2 -> 3\n"
      "    Arc 1: 0 -> 3\n";

  const std::string kExpectedOutgoingArcList =
      "  Node 0:\n"
      "    Arc 1: 0 -> 3\n"
      "    Arc 0: 0 -> 2\n"
      "  Node 1:\n"
      "  Node 2:\n"
      "    Arc 3: 2 -> 3\n"
      "    Arc 2: 2 -> 0\n"
      "  Node 3:\n"
      "    Arc 5: 3 -> 2\n"
      "    Arc 4: 3 -> 0\n";

  const std::string kExpectedDebugString =
      "Arcs:(node, next arc) :\n"
      " -6:(3,3)\n"
      " -5:(3,-3)\n"
      " -4:(2,-2)\n"
      " -3:(2,1)\n"
      " -2:(0,NilArc)\n"
      " -1:(0,NilArc)\n"
      " 0:(2,NilArc)\n"
      " 1:(3,0)\n"
      " 2:(0,-1)\n"
      " 3:(3,2)\n"
      " 4:(0,-4)\n"
      " 5:(2,4)\n"
      "Node:First arc :\n"
      " 0:-5\n"
      " 1:NilArc\n"
      " 2:-6\n"
      " 3:5\n";

  const std::string kExpectedForwardDebugString =
      "Arcs:(node, next arc) :\n"
      " 0:(2,NilArc)\n"
      " 1:(3,0)\n"
      " 2:(0,NilArc)\n"
      " 3:(3,2)\n"
      " 4:(0,NilArc)\n"
      " 5:(2,4)\n"
      "Node:First arc :\n"
      " 0:1\n"
      " 1:NilArc\n"
      " 2:3\n"
      " 3:5\n";

  const std::string kExpectedForwardStaticDebugString =
      "Arcs:(node) :\n"
      " 0:(2)\n"
      " 1:(3)\n"
      " 2:(0)\n"
      " 3:(3)\n"
      " 4:(0)\n"
      " 5:(2)\n"
      "Node:First arc :\n"
      " 0:0\n"
      " 1:2\n"
      " 2:2\n"
      " 3:4\n"
      " 4:6\n";

  TestEbertGraph(*graph, kGraphArcList, kExpectedAdjacencyList,
                 kExpectedIncomingArcList, kExpectedOutgoingArcList,
                 kExpectedDebugString, kExpectedForwardDebugString,
                 kExpectedForwardStaticDebugString);

  delete graph;
}

TYPED_TEST(DebugStringTestWithGraphBuildManager, SortedArcsWithoutAnnotation) {
  AnnotatedGraphBuildManager<TypeParam>* builder =
      new AnnotatedGraphBuildManager<TypeParam>(
          4, 6, true /* sort adjacency lists */);

  EXPECT_EQ(0, builder->AddArc(0, 2));
  EXPECT_EQ(1, builder->AddArc(2, 0));
  EXPECT_EQ(2, builder->AddArc(2, 3));
  EXPECT_EQ(3, builder->AddArc(3, 2));
  EXPECT_EQ(4, builder->AddArc(0, 3));
  EXPECT_EQ(5, builder->AddArc(3, 0));

  const TypeParam* graph = builder->Graph(nullptr);

  const std::string kGraphArcList =
      "    Arc 0: 0 -> 2\n"
      "    Arc 1: 0 -> 3\n"
      "    Arc 2: 2 -> 0\n"
      "    Arc 3: 2 -> 3\n"
      "    Arc 4: 3 -> 0\n"
      "    Arc 5: 3 -> 2\n";

  const std::string kExpectedAdjacencyList =
      "  Node 0:\n"
      "    Arc -5: 0 -> 3\n"
      "    Arc -3: 0 -> 2\n"
      "    Arc 1: 0 -> 3\n"
      "    Arc 0: 0 -> 2\n"
      "  Node 1:\n"
      "  Node 2:\n"
      "    Arc -6: 2 -> 3\n"
      "    Arc 3: 2 -> 3\n"
      "    Arc 2: 2 -> 0\n"
      "    Arc -1: 2 -> 0\n"
      "  Node 3:\n"
      "    Arc 5: 3 -> 2\n"
      "    Arc 4: 3 -> 0\n"
      "    Arc -4: 3 -> 2\n"
      "    Arc -2: 3 -> 0\n";

  const std::string kExpectedIncomingArcList =
      "  Node 0:\n"
      "    Arc 4: 3 -> 0\n"
      "    Arc 2: 2 -> 0\n"
      "  Node 1:\n"
      "  Node 2:\n"
      "    Arc 5: 3 -> 2\n"
      "    Arc 0: 0 -> 2\n"
      "  Node 3:\n"
      "    Arc 3: 2 -> 3\n"
      "    Arc 1: 0 -> 3\n";

  const std::string kExpectedOutgoingArcList =
      "  Node 0:\n"
      "    Arc 1: 0 -> 3\n"
      "    Arc 0: 0 -> 2\n"
      "  Node 1:\n"
      "  Node 2:\n"
      "    Arc 3: 2 -> 3\n"
      "    Arc 2: 2 -> 0\n"
      "  Node 3:\n"
      "    Arc 5: 3 -> 2\n"
      "    Arc 4: 3 -> 0\n";

  const std::string kExpectedDebugString =
      "Arcs:(node, next arc) :\n"
      " -6:(3,3)\n"
      " -5:(3,-3)\n"
      " -4:(2,-2)\n"
      " -3:(2,1)\n"
      " -2:(0,NilArc)\n"
      " -1:(0,NilArc)\n"
      " 0:(2,NilArc)\n"
      " 1:(3,0)\n"
      " 2:(0,-1)\n"
      " 3:(3,2)\n"
      " 4:(0,-4)\n"
      " 5:(2,4)\n"
      "Node:First arc :\n"
      " 0:-5\n"
      " 1:NilArc\n"
      " 2:-6\n"
      " 3:5\n";

  const std::string kExpectedForwardDebugString =
      "Arcs:(node, next arc) :\n"
      " 0:(2,NilArc)\n"
      " 1:(3,0)\n"
      " 2:(0,NilArc)\n"
      " 3:(3,2)\n"
      " 4:(0,NilArc)\n"
      " 5:(2,4)\n"
      "Node:First arc :\n"
      " 0:1\n"
      " 1:NilArc\n"
      " 2:3\n"
      " 3:5\n";

  const std::string kExpectedForwardStaticDebugString =
      "Arcs:(node) :\n"
      " 0:(2)\n"
      " 1:(3)\n"
      " 2:(0)\n"
      " 3:(3)\n"
      " 4:(0)\n"
      " 5:(2)\n"
      "Node:First arc :\n"
      " 0:0\n"
      " 1:2\n"
      " 2:2\n"
      " 3:4\n"
      " 4:6\n";

  TestEbertGraph(*graph, kGraphArcList, kExpectedAdjacencyList,
                 kExpectedIncomingArcList, kExpectedOutgoingArcList,
                 kExpectedDebugString, kExpectedForwardDebugString,
                 kExpectedForwardStaticDebugString);

  delete graph;
}

// An empty fixture template to collect the types of tiny graphs for which we
// want to do very basic tests.
template <typename GraphType>
class TinyEbertGraphTest : public ::testing::Test {};

typedef ::testing::Types<EbertGraph<int8_t, int8_t> >
    TinyEbertGraphTypesForTesting;

TYPED_TEST_SUITE(TinyEbertGraphTest, TinyEbertGraphTypesForTesting);

TYPED_TEST(TinyEbertGraphTest, CheckDeathOnBadBounds) {
  typedef TypeParam SmallStarGraph;
  int num_nodes = SmallStarGraph::kMaxNumNodes;
  int num_arcs = SmallStarGraph::kMaxNumArcs;
  SmallStarGraph(num_nodes, num_arcs);  // Construct an unused graph. All fine.
}

// An empty fixture to collect the types of small graphs for which we want to do
// some fairly trivial tests.
template <typename GraphType>
class SmallEbertGraphTest : public ::testing::Test {};

typedef ::testing::Types<EbertGraph<int8_t, int8_t>,
                         EbertGraph<int16_t, int16_t> >
    SmallEbertGraphTypesForTesting;

TYPED_TEST_SUITE(SmallEbertGraphTest, SmallEbertGraphTypesForTesting);

TYPED_TEST(SmallEbertGraphTest, EmptyGraph) {
  TypeParam graph(3, 6);
  const std::string kGraphArcList = "";
  const std::string kExpectedAdjacencyList = "";
  const std::string kExpectedIncomingArcList = "";
  const std::string kExpectedOutgoingArcList = "";
  const std::string kExpectedDebugString =
      "Arcs:(node, next arc) :\n"
      "Node:First arc :\n";
  TestEbertGraph(graph, kGraphArcList, kExpectedAdjacencyList,
                 kExpectedIncomingArcList, kExpectedOutgoingArcList,
                 kExpectedDebugString, kExpectedDebugString,
                 kExpectedDebugString);
}

TEST(EbertGraphTest, CheckBounds) {
  typedef EbertGraph<int16_t, int16_t> SmallStarGraph;
  SmallStarGraph g(SmallStarGraph::kMaxNumNodes, SmallStarGraph::kMaxNumArcs);
  EXPECT_TRUE(g.CheckArcBounds(SmallStarGraph::kNilArc));
  EXPECT_FALSE(g.CheckArcValidity(SmallStarGraph::kNilArc));
  EXPECT_FALSE(g.CheckArcValidity(SmallStarGraph::kMaxNumArcs));
  EXPECT_TRUE(g.CheckArcValidity(g.SmallStarGraph::kMaxNumArcs - 1));
  EXPECT_TRUE(g.CheckArcValidity(g.Opposite(SmallStarGraph::kMaxNumArcs - 1)));
}

template <typename GraphType, bool sort_arcs>
static void BM_RandomArcs(benchmark::State& state) {
  const int kRandomSeed = 0;
  const int kNodes = 10 * 1000 * 1000;
  const int kArcs = 5 * kNodes;
  for (auto _ : state) {
    AnnotatedGraphBuildManager<GraphType>* builder =
        new AnnotatedGraphBuildManager<GraphType>(kNodes, kArcs, sort_arcs);
    std::mt19937 randomizer(kRandomSeed);
    for (int i = 0; i < kArcs; ++i) {
      builder->AddArc(absl::Uniform(randomizer, 0, kNodes),
                      absl::Uniform(randomizer, 0, kNodes));
    }
    (void)builder->Graph(nullptr);
  }
  // An item is an arc here.
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kArcs);
}

BENCHMARK_TEMPLATE2(BM_RandomArcs, StarGraph, false);

BENCHMARK_TEMPLATE2(BM_RandomArcs, StarGraph, true);

template <typename GraphType, bool sort_arcs>
static void BM_RandomAnnotatedArcs(benchmark::State& state) {
  const int kRandomSeed = 0;
  const int kNodes = 10 * 1000 * 1000;
  const int kArcs = 5 * kNodes;
  int* annotation = new int[kArcs];
  for (auto _ : state) {
    AnnotatedGraphBuildManager<GraphType>* builder =
        new AnnotatedGraphBuildManager<GraphType>(kNodes, kArcs, sort_arcs);
    std::mt19937 randomizer(kRandomSeed);
    for (int i = 0; i < kArcs; ++i) {
      ArcIndex arc = builder->AddArc(absl::Uniform(randomizer, 0, kNodes),
                                     absl::Uniform(randomizer, 0, kNodes));
      annotation[arc] = absl::Uniform(randomizer, 0, kNodes);
    }
    ArrayIndexCycleHandler<int, ArcIndex> cycle_handler(annotation);
    (void)builder->Graph(&cycle_handler);
  }
  delete[] annotation;
  // An item is an arc here.
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kArcs);
}

BENCHMARK_TEMPLATE2(BM_RandomAnnotatedArcs, StarGraph, false);

BENCHMARK_TEMPLATE2(BM_RandomAnnotatedArcs, StarGraph, true);

template <typename GraphType>
static void BM_AddRandomArcsAndDoNotRetrieveGraph(benchmark::State& state) {
  const int kRandomSeed = 0;
  const int kNodes = 10 * 1000 * 1000;
  const int kArcs = 5 * kNodes;
  for (auto _ : state) {
    AnnotatedGraphBuildManager<GraphType>* builder =
        new AnnotatedGraphBuildManager<GraphType>(kNodes, kArcs, false);
    std::mt19937 randomizer(kRandomSeed);
    for (int i = 0; i < kArcs; ++i) {
      builder->AddArc(absl::Uniform(randomizer, 0, kNodes),
                      absl::Uniform(randomizer, 0, kNodes));
    }
    delete builder;
  }
  // An item is an arc here.
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kArcs);
}

BENCHMARK_TEMPLATE(BM_AddRandomArcsAndDoNotRetrieveGraph, StarGraph);

}  // namespace operations_research
