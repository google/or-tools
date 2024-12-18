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

#include "ortools/graph/generic_max_flow.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/graph/ebert_graph.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/graphs.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {
namespace {

using ::testing::ContainerEq;
using ::testing::WhenSorted;

template <typename Graph>
typename GenericMaxFlow<Graph>::Status MaxFlowTester(
    const typename Graph::NodeIndex num_nodes,
    const typename Graph::ArcIndex num_arcs,
    const typename Graph::NodeIndex* tail,
    const typename Graph::NodeIndex* head, const FlowQuantity* capacity,
    const FlowQuantity* expected_flow, const FlowQuantity expected_total_flow,
    const std::vector<typename Graph::NodeIndex>* expected_source_min_cut =
        nullptr,
    const std::vector<typename Graph::NodeIndex>* expected_sink_min_cut =
        nullptr) {
  Graph graph(num_nodes, num_arcs);
  for (int i = 0; i < num_arcs; ++i) {
    graph.AddArc(tail[i], head[i]);
  }
  std::vector<typename Graph::ArcIndex> permutation;
  Graphs<Graph>::Build(&graph, &permutation);
  EXPECT_TRUE(permutation.empty());

  GenericMaxFlow<Graph> max_flow(&graph, 0, num_nodes - 1);
  for (typename Graph::ArcIndex arc = 0; arc < num_arcs; ++arc) {
    max_flow.SetArcCapacity(arc, capacity[arc]);
    EXPECT_EQ(max_flow.Capacity(arc), capacity[arc]);
  }
  EXPECT_TRUE(max_flow.Solve());
  if (max_flow.status() == GenericMaxFlow<Graph>::OPTIMAL) {
    const FlowQuantity total_flow = max_flow.GetOptimalFlow();
    EXPECT_EQ(expected_total_flow, total_flow);
    for (int i = 0; i < num_arcs; ++i) {
      EXPECT_EQ(expected_flow[i], max_flow.Flow(i)) << " i = " << i;
    }
  }

  // Tests the min-cut functions.
  if (expected_source_min_cut != nullptr) {
    std::vector<NodeIndex> cut;
    max_flow.GetSourceSideMinCut(&cut);
    std::sort(cut.begin(), cut.end());
    EXPECT_THAT(*expected_source_min_cut, WhenSorted(ContainerEq(cut)));
  }
  if (expected_sink_min_cut != nullptr) {
    std::vector<NodeIndex> cut;
    max_flow.GetSinkSideMinCut(&cut);
    std::sort(cut.begin(), cut.end());
    EXPECT_THAT(*expected_sink_min_cut, WhenSorted(ContainerEq(cut)));
  }

  return max_flow.status();
}

template <typename Graph>
class GenericMaxFlowTest : public ::testing::Test {};

typedef ::testing::Types<StarGraph, util::ReverseArcListGraph<>,
                         util::ReverseArcStaticGraph<>,
                         util::ReverseArcMixedGraph<> >
    GraphTypes;

TYPED_TEST_SUITE(GenericMaxFlowTest, GraphTypes);

TYPED_TEST(GenericMaxFlowTest, FeasibleFlow1) {
  const int kNumNodes = 4;
  const int kNumArcs = 3;
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 1, 2};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {1, 2, 3};
  const FlowQuantity kCapacity[kNumArcs] = {8, 10, 8};
  const FlowQuantity kExpectedFlow[kNumArcs] = {8, 8, 8};
  const FlowQuantity kExpectedTotalFlow = 8;
  std::vector<int> source_cut({0});
  std::vector<int> sink_cut({3});
  EXPECT_EQ(GenericMaxFlow<TypeParam>::OPTIMAL,
            MaxFlowTester<TypeParam>(
                kNumNodes, kNumArcs, kTail, kHead, kCapacity, kExpectedFlow,
                kExpectedTotalFlow, &source_cut, &sink_cut));
}

TYPED_TEST(GenericMaxFlowTest, FeasibleFlow2) {
  const int kNumNodes = 6;
  const int kNumArcs = 9;
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 0, 0, 0, 1,
                                                         2, 3, 3, 4};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {1, 2, 3, 4, 3,
                                                         4, 4, 5, 5};
  const FlowQuantity kCapacity[kNumArcs] = {6, 8, 5, 0, 1, 4, 0, 6, 4};
  const FlowQuantity kExpectedFlow[kNumArcs] = {1, 4, 5, 0, 1, 4, 0, 6, 4};
  const FlowQuantity kExpectedTotalFlow = 10;
  std::vector<int> source_cut({0, 1, 2});
  std::vector<int> sink_cut({5});
  EXPECT_EQ(GenericMaxFlow<TypeParam>::OPTIMAL,
            MaxFlowTester<TypeParam>(
                kNumNodes, kNumArcs, kTail, kHead, kCapacity, kExpectedFlow,
                kExpectedTotalFlow, &source_cut, &sink_cut));
}

TYPED_TEST(GenericMaxFlowTest, FeasibleFlowWithMultipleArcs) {
  const int kNumNodes = 5;
  const int kNumArcs = 8;
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 0, 1, 1,
                                                         2, 2, 3, 3};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {1, 1, 2, 2,
                                                         3, 3, 4, 4};
  const FlowQuantity kCapacity[kNumArcs] = {5, 3, 5, 3, 4, 4, 4, 4};
  const FlowQuantity kExpectedFlow[kNumArcs] = {5, 3, 5, 3, 4, 4, 4, 4};
  const FlowQuantity kExpectedTotalFlow = 8;
  std::vector<int> source_cut({0});
  std::vector<int> sink_cut({4});
  EXPECT_EQ(GenericMaxFlow<TypeParam>::OPTIMAL,
            MaxFlowTester<TypeParam>(
                kNumNodes, kNumArcs, kTail, kHead, kCapacity, kExpectedFlow,
                kExpectedTotalFlow, &source_cut, &sink_cut));
}

TYPED_TEST(GenericMaxFlowTest, HugeCapacity) {
  const FlowQuantity kCapacityMax = std::numeric_limits<FlowQuantity>::max();
  const int kNumNodes = 5;
  const int kNumArcs = 5;
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 0, 1, 2, 3};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {1, 2, 3, 3, 4};
  const FlowQuantity kCapacity[kNumArcs] = {kCapacityMax, kCapacityMax, 5, 3,
                                            kCapacityMax};
  const FlowQuantity kExpectedFlow[kNumArcs] = {5, 3, 5, 3, 8};
  const FlowQuantity kExpectedTotalFlow = 8;
  std::vector<int> source_cut({0, 1, 2});
  std::vector<int> sink_cut({4, 3});
  EXPECT_EQ(GenericMaxFlow<TypeParam>::OPTIMAL,
            MaxFlowTester<TypeParam>(
                kNumNodes, kNumArcs, kTail, kHead, kCapacity, kExpectedFlow,
                kExpectedTotalFlow, &source_cut, &sink_cut));
}

TYPED_TEST(GenericMaxFlowTest, FlowQuantityOverflowLimitCase) {
  const FlowQuantity kCapacityMax = std::numeric_limits<FlowQuantity>::max();
  const FlowQuantity kHalfLow = kCapacityMax / 2;
  const FlowQuantity kHalfHigh = kCapacityMax - kHalfLow;
  const int kNumNodes = 5;
  const int kNumArcs = 5;
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 0, 1, 2, 3};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {1, 2, 3, 3, 4};
  const FlowQuantity kCapacity[kNumArcs] = {kCapacityMax, kCapacityMax,
                                            kHalfLow, kHalfHigh, kCapacityMax};
  const FlowQuantity kExpectedFlow[kNumArcs] = {kHalfLow, kHalfHigh, kHalfLow,
                                                kHalfHigh, kCapacityMax};
  const FlowQuantity kExpectedTotalFlow = kCapacityMax;
  std::vector<int> source_cut({0, 1, 2});
  std::vector<int> sink_cut({4});
  EXPECT_EQ(GenericMaxFlow<TypeParam>::OPTIMAL,
            MaxFlowTester<TypeParam>(
                kNumNodes, kNumArcs, kTail, kHead, kCapacity, kExpectedFlow,
                kExpectedTotalFlow, &source_cut, &sink_cut));
}

TYPED_TEST(GenericMaxFlowTest, FlowQuantityOverflow) {
  const FlowQuantity kCapacityMax = std::numeric_limits<FlowQuantity>::max();
  const int kNumNodes = 4;
  const int kNumArcs = 4;
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 0, 1, 2};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {1, 2, 3, 3};
  const FlowQuantity kCapacity[kNumArcs] = {kCapacityMax, kCapacityMax,
                                            kCapacityMax, kCapacityMax};
  const FlowQuantity kExpectedFlow[kNumArcs] = {kCapacityMax, kCapacityMax,
                                                kCapacityMax, kCapacityMax};
  const FlowQuantity kExpectedTotalFlow = kCapacityMax;
  EXPECT_EQ(
      GenericMaxFlow<TypeParam>::INT_OVERFLOW,
      MaxFlowTester<TypeParam>(kNumNodes, kNumArcs, kTail, kHead, kCapacity,
                               kExpectedFlow, kExpectedTotalFlow));
}

TYPED_TEST(GenericMaxFlowTest, DirectArcFromSourceToSink) {
  const int kNumNodes = 4;
  const int kNumArcs = 5;
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 0, 0, 1, 2};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {1, 3, 2, 3, 3};
  const FlowQuantity kCapacity[kNumArcs] = {5, 8, 5, 2, 2};
  const FlowQuantity kExpectedFlow[kNumArcs] = {2, 8, 2, 2, 2};
  const FlowQuantity kExpectedTotalFlow = 12;
  std::vector<int> source_cut({0, 1, 2});
  std::vector<int> sink_cut({3});
  EXPECT_EQ(GenericMaxFlow<TypeParam>::OPTIMAL,
            MaxFlowTester<TypeParam>(
                kNumNodes, kNumArcs, kTail, kHead, kCapacity, kExpectedFlow,
                kExpectedTotalFlow, &source_cut, &sink_cut));
}

TYPED_TEST(GenericMaxFlowTest, FlowOnDisconnectedGraph1) {
  const int kNumNodes = 6;
  const int kNumArcs = 7;
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 0, 0, 0, 1, 2, 3};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {1, 2, 3, 4, 3, 4, 4};
  const FlowQuantity kCapacity[kNumArcs] = {5, 8, 5, 3, 4, 5, 6};
  const FlowQuantity kExpectedFlow[kNumArcs] = {0, 0, 0, 0, 0, 0, 0};
  const FlowQuantity kExpectedTotalFlow = 0;
  std::vector<int> source_cut({0, 1, 2, 3, 4});
  std::vector<int> sink_cut({5});
  EXPECT_EQ(GenericMaxFlow<TypeParam>::OPTIMAL,
            MaxFlowTester<TypeParam>(
                kNumNodes, kNumArcs, kTail, kHead, kCapacity, kExpectedFlow,
                kExpectedTotalFlow, &source_cut, &sink_cut));
}

TYPED_TEST(GenericMaxFlowTest, FlowOnDisconnectedGraph2) {
  const int kNumNodes = 6;
  const int kNumArcs = 5;
  const typename TypeParam::NodeIndex kTail[kNumArcs] = {0, 0, 3, 3, 4};
  const typename TypeParam::NodeIndex kHead[kNumArcs] = {1, 2, 4, 5, 5};
  const FlowQuantity kCapacity[kNumArcs] = {5, 8, 6, 6, 4};
  const FlowQuantity kExpectedFlow[kNumArcs] = {0, 0, 0, 0, 0};
  const FlowQuantity kExpectedTotalFlow = 0;
  std::vector<int> source_cut({0, 1, 2});
  std::vector<int> sink_cut({3, 4, 5});
  EXPECT_EQ(GenericMaxFlow<TypeParam>::OPTIMAL,
            MaxFlowTester<TypeParam>(
                kNumNodes, kNumArcs, kTail, kHead, kCapacity, kExpectedFlow,
                kExpectedTotalFlow, &source_cut, &sink_cut));
}

template <typename Graph>
void AddSourceAndSink(const typename Graph::NodeIndex num_tails,
                      const typename Graph::NodeIndex num_heads, Graph* graph) {
  const typename Graph::NodeIndex source = num_tails + num_heads;
  const typename Graph::NodeIndex sink = num_tails + num_heads + 1;
  for (typename Graph::NodeIndex tail = 0; tail < num_tails; ++tail) {
    graph->AddArc(source, tail);
  }
  for (typename Graph::NodeIndex head = 0; head < num_heads; ++head) {
    graph->AddArc(num_tails + head, sink);
  }
}

template <typename Graph>
void GenerateCompleteGraph(const typename Graph::NodeIndex num_tails,
                           const typename Graph::NodeIndex num_heads,
                           Graph* graph) {
  const typename Graph::NodeIndex num_nodes = num_tails + num_heads + 2;
  const typename Graph::ArcIndex num_arcs =
      num_tails * num_heads + num_tails + num_heads;
  graph->Reserve(num_nodes, num_arcs);
  for (typename Graph::NodeIndex tail = 0; tail < num_tails; ++tail) {
    for (typename Graph::NodeIndex head = 0; head < num_heads; ++head) {
      graph->AddArc(tail, head + num_tails);
    }
  }
  AddSourceAndSink(num_tails, num_heads, graph);
}

template <typename Graph>
void GeneratePartialRandomGraph(const typename Graph::NodeIndex num_tails,
                                const typename Graph::NodeIndex num_heads,
                                const typename Graph::NodeIndex degree,
                                Graph* graph) {
  const typename Graph::NodeIndex num_nodes = num_tails + num_heads + 2;
  const typename Graph::ArcIndex num_arcs =
      num_tails * degree + num_tails + num_heads;
  graph->Reserve(num_nodes, num_arcs);
  std::mt19937 randomizer(0);
  for (typename Graph::NodeIndex tail = 0; tail < num_tails; ++tail) {
    for (typename Graph::NodeIndex d = 0; d < degree; ++d) {
      const typename Graph::NodeIndex head =
          absl::Uniform(randomizer, 0, num_heads);
      graph->AddArc(tail, head + num_tails);
    }
  }
  AddSourceAndSink(num_tails, num_heads, graph);
}

template <typename Graph>
void GenerateRandomArcValuations(const Graph& graph, const int64_t max_range,
                                 std::vector<int64_t>* arc_valuation) {
  const typename Graph::ArcIndex num_arcs = graph.num_arcs();
  arc_valuation->resize(num_arcs);
  std::mt19937 randomizer(0);
  for (typename Graph::ArcIndex arc = 0; arc < graph.num_arcs(); ++arc) {
    (*arc_valuation)[arc] = absl::Uniform(randomizer, 0, max_range);
  }
}

template <typename Graph>
void SetUpNetworkData(absl::Span<const int64_t> arc_capacity,
                      GenericMaxFlow<Graph>* max_flow) {
  const Graph* graph = max_flow->graph();
  for (typename Graph::ArcIndex arc = 0; arc < graph->num_arcs(); ++arc) {
    max_flow->SetArcCapacity(arc, arc_capacity[arc]);
  }
}

template <typename Graph>
FlowQuantity SolveMaxFlow(GenericMaxFlow<Graph>* max_flow) {
  EXPECT_TRUE(max_flow->Solve());
  EXPECT_EQ(GenericMaxFlow<Graph>::OPTIMAL, max_flow->status());
  const Graph* graph = max_flow->graph();
  for (typename Graph::ArcIndex arc = 0; arc < graph->num_arcs(); ++arc) {
    EXPECT_LE(max_flow->Flow(Graphs<Graph>::OppositeArc(*graph, arc)), 0)
        << arc;
    EXPECT_EQ(0, max_flow->Capacity(Graphs<Graph>::OppositeArc(*graph, arc)))
        << arc;
    EXPECT_LE(0, max_flow->Flow(arc)) << arc;
    EXPECT_LE(max_flow->Flow(arc), max_flow->Capacity(arc)) << arc;
  }
  return max_flow->GetOptimalFlow();
}

template <typename Graph>
FlowQuantity SolveMaxFlowWithLP(GenericMaxFlow<Graph>* max_flow) {
  MPSolver solver("LPSolver", MPSolver::GLOP_LINEAR_PROGRAMMING);
  const double infinity = solver.infinity();
  const Graph* graph = max_flow->graph();
  const typename Graph::NodeIndex num_nodes = graph->num_nodes();
  const typename Graph::ArcIndex num_arcs = graph->num_arcs();
  const typename Graph::NodeIndex source_index = num_nodes - 2;
  std::unique_ptr<MPConstraint*[]> constraint(new MPConstraint*[num_nodes]);
  for (typename Graph::NodeIndex node = 0; node < graph->num_nodes(); ++node) {
    constraint[node] = solver.MakeRowConstraint();
    if (node < source_index) {  // Node is neither source nor sink.
      constraint[node]->SetBounds(0.0, 0.0);  // Flow is conserved.
    } else {
      constraint[node]->SetBounds(-infinity, +infinity);
    }
  }
  std::unique_ptr<MPVariable*[]> var(new MPVariable*[num_arcs]);
  for (typename Graph::ArcIndex arc = 0; arc < graph->num_arcs(); ++arc) {
    var[arc] = solver.MakeNumVar(0.0, max_flow->Capacity(arc),
                                 absl::StrFormat("v%d", arc));
    constraint[graph->Tail(arc)]->SetCoefficient(var[arc], 1.0);
    constraint[graph->Head(arc)]->SetCoefficient(var[arc], -1.0);
  }
  MPObjective* const objective = solver.MutableObjective();
  for (typename Graph::OutgoingArcIterator arc_it(*graph, source_index);
       arc_it.Ok(); arc_it.Next()) {
    const typename Graph::ArcIndex arc = arc_it.Index();
    objective->SetCoefficient(var[arc], -1.0);
  }
  solver.Solve();
  return static_cast<FlowQuantity>(-objective->Value() + .5);
}

template <typename Graph>
struct MaxFlowSolver {
  typedef FlowQuantity (*Solver)(GenericMaxFlow<Graph>* max_flow);
};

template <typename Graph>
void FullRandomAssignment(typename MaxFlowSolver<Graph>::Solver f,
                          typename Graph::NodeIndex num_tails,
                          typename Graph::NodeIndex num_heads,
                          FlowQuantity expected_flow1,
                          FlowQuantity expected_flow2) {
  Graph graph;
  GenerateCompleteGraph(num_tails, num_heads, &graph);
  Graphs<Graph>::Build(&graph);
  std::vector<int64_t> arc_capacity(graph.num_arcs(), 1);
  std::unique_ptr<GenericMaxFlow<Graph> > max_flow(new GenericMaxFlow<Graph>(
      &graph, graph.num_nodes() - 2, graph.num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());
  FlowQuantity flow = f(max_flow.get());
  EXPECT_EQ(expected_flow1, flow);
}

template <typename Graph>
void PartialRandomAssignment(typename MaxFlowSolver<Graph>::Solver f,
                             typename Graph::NodeIndex num_tails,
                             typename Graph::NodeIndex num_heads,
                             FlowQuantity expected_flow1,
                             FlowQuantity expected_flow2) {
  const typename Graph::NodeIndex kDegree = 10;
  Graph graph;
  GeneratePartialRandomGraph(num_tails, num_heads, kDegree, &graph);
  Graphs<Graph>::Build(&graph);
  CHECK_EQ(graph.num_arcs(), num_tails * kDegree + num_tails + num_heads);
  std::vector<int64_t> arc_capacity(graph.num_arcs(), 1);
  std::unique_ptr<GenericMaxFlow<Graph> > max_flow(new GenericMaxFlow<Graph>(
      &graph, graph.num_nodes() - 2, graph.num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());
  FlowQuantity flow = f(max_flow.get());
  EXPECT_EQ(expected_flow1, flow);
}

template <typename Graph>
void ChangeCapacities(absl::Span<const int64_t> arc_capacity,
                      FlowQuantity delta, GenericMaxFlow<Graph>* max_flow) {
  const Graph* graph = max_flow->graph();
  for (typename Graph::ArcIndex arc = 0; arc < graph->num_arcs(); ++arc) {
    max_flow->SetArcCapacity(arc,
                             std::max(arc_capacity[arc] - delta, int64_t{0}));
  }
}

template <typename Graph>
void PartialRandomFlow(typename MaxFlowSolver<Graph>::Solver f,
                       typename Graph::NodeIndex num_tails,
                       typename Graph::NodeIndex num_heads,
                       FlowQuantity expected_flow1,
                       FlowQuantity expected_flow2) {
  const typename Graph::NodeIndex kDegree = 10;
  const FlowQuantity kCapacityRange = 10000;
  const FlowQuantity kCapacityDelta = 1000;
  Graph graph;
  GeneratePartialRandomGraph(num_tails, num_heads, kDegree, &graph);
  std::vector<int64_t> arc_capacity(graph.num_arcs());
  GenerateRandomArcValuations(graph, kCapacityRange, &arc_capacity);

  std::vector<typename Graph::ArcIndex> permutation;
  Graphs<Graph>::Build(&graph, &permutation);
  util::Permute(permutation, &arc_capacity);

  std::unique_ptr<GenericMaxFlow<Graph> > max_flow(new GenericMaxFlow<Graph>(
      &graph, graph.num_nodes() - 2, graph.num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());
  FlowQuantity flow = f(max_flow.get());
  EXPECT_EQ(expected_flow1, flow);
  ChangeCapacities(arc_capacity, kCapacityDelta, max_flow.get());
  flow = f(max_flow.get());
  EXPECT_EQ(expected_flow2, flow);
  ChangeCapacities(arc_capacity, 0, max_flow.get());
  flow = f(max_flow.get());
  EXPECT_EQ(expected_flow1, flow);
}

template <typename Graph>
void FullRandomFlow(typename MaxFlowSolver<Graph>::Solver f,
                    typename Graph::NodeIndex num_tails,
                    typename Graph::NodeIndex num_heads,
                    FlowQuantity expected_flow1, FlowQuantity expected_flow2) {
  const FlowQuantity kCapacityRange = 10000;
  const FlowQuantity kCapacityDelta = 1000;
  Graph graph;
  GenerateCompleteGraph(num_tails, num_heads, &graph);
  std::vector<int64_t> arc_capacity(graph.num_arcs());
  GenerateRandomArcValuations(graph, kCapacityRange, &arc_capacity);

  std::vector<typename Graph::ArcIndex> permutation;
  Graphs<Graph>::Build(&graph, &permutation);
  util::Permute(permutation, &arc_capacity);

  std::unique_ptr<GenericMaxFlow<Graph> > max_flow(new GenericMaxFlow<Graph>(
      &graph, graph.num_nodes() - 2, graph.num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());
  FlowQuantity flow = f(max_flow.get());
  EXPECT_EQ(expected_flow1, flow);
  ChangeCapacities(arc_capacity, kCapacityDelta, max_flow.get());
  flow = f(max_flow.get());
  EXPECT_EQ(expected_flow2, flow);
  ChangeCapacities(arc_capacity, 0, max_flow.get());
  flow = f(max_flow.get());
  EXPECT_EQ(expected_flow1, flow);
}

#define LP_AND_FLOW_TEST(test_name, size, expected_flow1, expected_flow2) \
  LP_ONLY_TEST(test_name, size, expected_flow1, expected_flow2)           \
  FLOW_ONLY_TEST(test_name, size, expected_flow1, expected_flow2)         \
  FLOW_ONLY_TEST_SG(test_name, size, expected_flow1, expected_flow2)

#define LP_ONLY_TEST(test_name, size, expected_flow1, expected_flow2) \
  TEST(LPMaxFlowTest, test_name##size) {                              \
    test_name<StarGraph>(SolveMaxFlowWithLP<StarGraph>, size, size,   \
                         expected_flow1, expected_flow2);             \
  }

#define FLOW_ONLY_TEST(test_name, size, expected_flow1, expected_flow2) \
  TEST(MaxFlowTest, test_name##size) {                                  \
    test_name<StarGraph>(SolveMaxFlow, size, size, expected_flow1,      \
                         expected_flow2);                               \
  }

#define FLOW_ONLY_TEST_SG(test_name, size, expected_flow1, expected_flow2)     \
  TEST(MaxFlowTestStaticGraph, test_name##size) {                              \
    test_name<util::ReverseArcStaticGraph<> >(SolveMaxFlow, size, size,        \
                                              expected_flow1, expected_flow2); \
  }

LP_AND_FLOW_TEST(FullRandomAssignment, 300, 300, 300);
LP_AND_FLOW_TEST(PartialRandomAssignment, 100, 100, 100);
LP_AND_FLOW_TEST(PartialRandomAssignment, 1000, 1000, 1000);
LP_AND_FLOW_TEST(PartialRandomFlow, 400, 1898664, 1515203);
LP_AND_FLOW_TEST(FullRandomFlow, 100, 482391, 386587);

// LARGE must be defined from the build command line to test larger instances.
#ifdef LARGE
LP_AND_FLOW_TEST(PartialRandomAssignment, 10000, 10000, 10000);
#endif

template <typename Graph>
static void BM_FullRandomAssignment(benchmark::State& state) {
  const int kSize = 3000;
  for (auto _ : state) {
    FullRandomAssignment<Graph>(SolveMaxFlow, kSize, kSize, kSize, kSize);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kSize);
}

template <typename Graph>
static void BM_PartialRandomAssignment(benchmark::State& state) {
  const int kSize = 10100;
  for (auto _ : state) {
    PartialRandomAssignment<Graph>(SolveMaxFlow, kSize, kSize, kSize, kSize);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kSize);
}

template <typename Graph>
static void BM_PartialRandomFlow(benchmark::State& state) {
  const int kSize = 800;
  for (auto _ : state) {
    PartialRandomFlow<Graph>(SolveMaxFlow, kSize, kSize, 3884850, 3112123);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kSize);
}

template <typename Graph>
static void BM_FullRandomFlow(benchmark::State& state) {
  const int kSize = 800;
  for (auto _ : state) {
    FullRandomFlow<Graph>(SolveMaxFlow, kSize, kSize, 4000549, 3239512);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kSize);
}

BENCHMARK_TEMPLATE(BM_FullRandomAssignment, StarGraph);
BENCHMARK_TEMPLATE(BM_FullRandomAssignment, util::ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_FullRandomAssignment, util::ReverseArcStaticGraph<>);
BENCHMARK_TEMPLATE(BM_FullRandomAssignment, util::ReverseArcMixedGraph<>);
BENCHMARK_TEMPLATE(BM_PartialRandomFlow, StarGraph);
BENCHMARK_TEMPLATE(BM_PartialRandomFlow, util::ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_PartialRandomFlow, util::ReverseArcStaticGraph<>);
BENCHMARK_TEMPLATE(BM_PartialRandomFlow, util::ReverseArcMixedGraph<>);

// One iteration of each of the following tests is slow.
BENCHMARK_TEMPLATE(BM_FullRandomFlow, StarGraph);
BENCHMARK_TEMPLATE(BM_FullRandomFlow, util::ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_FullRandomFlow, util::ReverseArcStaticGraph<>);
BENCHMARK_TEMPLATE(BM_FullRandomFlow, util::ReverseArcMixedGraph<>);
BENCHMARK_TEMPLATE(BM_PartialRandomAssignment, StarGraph);
BENCHMARK_TEMPLATE(BM_PartialRandomAssignment, util::ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_PartialRandomAssignment, util::ReverseArcStaticGraph<>);
BENCHMARK_TEMPLATE(BM_PartialRandomAssignment, util::ReverseArcMixedGraph<>);

#undef LP_AND_FLOW_TEST
#undef LP_ONLY_TEST
#undef FLOW_ONLY_TEST
#undef FLOW_ONLY_TEST_SG

// ----------------------------------------------------------
// PriorityQueueWithRestrictedPush tests.
// ----------------------------------------------------------

TEST(PriorityQueueWithRestrictedPushTest, BasicBehavior) {
  PriorityQueueWithRestrictedPush<std::string, int> queue;
  EXPECT_TRUE(queue.IsEmpty());
  queue.Push("A", 1);
  queue.Push("B", 0);
  queue.Push("C", 2);
  queue.Push("D", 10);
  queue.Push("E", 9);
  EXPECT_EQ("D", queue.Pop());
  EXPECT_EQ("E", queue.Pop());
  EXPECT_EQ("C", queue.Pop());
  EXPECT_EQ("A", queue.Pop());
  EXPECT_EQ("B", queue.Pop());
  EXPECT_TRUE(queue.IsEmpty());
  queue.Push("A", 1);
  queue.Push("B", 0);
  EXPECT_FALSE(queue.IsEmpty());
  queue.Clear();
  EXPECT_TRUE(queue.IsEmpty());
}

TEST(PriorityQueueWithRestrictedPushTest, BasicBehaviorWithMixedPushPop) {
  PriorityQueueWithRestrictedPush<std::string, int> queue;
  EXPECT_TRUE(queue.IsEmpty());
  queue.Push("A", 1);
  queue.Push("B", 0);
  queue.Push("C", 2);
  EXPECT_EQ("C", queue.Pop());
  EXPECT_EQ("A", queue.Pop());
  queue.Push("D", 1);
  queue.Push("E", 0);
  EXPECT_EQ("D", queue.Pop());
  EXPECT_EQ("E", queue.Pop());
  EXPECT_EQ("B", queue.Pop());
  EXPECT_TRUE(queue.IsEmpty());
  queue.Push("E", 1);
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ("E", queue.Pop());
  EXPECT_TRUE(queue.IsEmpty());
}

TEST(PriorityQueueWithRestrictedPushTest, RandomPushPop) {
  struct ElementWithPriority {
    ElementWithPriority(int _element, int _priority)
        : element(_element), priority(_priority) {}
    bool operator<(const ElementWithPriority& other) const {
      return priority < other.priority;
    }
    int element;
    int priority;
  };
  std::vector<ElementWithPriority> pairs;
  std::mt19937 randomizer(1);
  const int kNumElement = 10000;
  const int kMaxPriority = 10000;  // We want duplicates and gaps.
  for (int i = 0; i < kNumElement; ++i) {
    pairs.push_back(
        ElementWithPriority(i, absl::Uniform(randomizer, 0, kMaxPriority)));
  }
  std::sort(pairs.begin(), pairs.end());

  // Randomly add +1 and push to the queue.
  PriorityQueueWithRestrictedPush<int, int> queue;
  for (int i = 0; i < pairs.size(); ++i) {
    pairs[i].priority += absl::Bernoulli(randomizer, 1.0 / 2) ? 1 : 0;
    queue.Push(pairs[i].element, pairs[i].priority);
  }

  // Stable sort the pairs for checking (the queue order is stable).
  std::stable_sort(pairs.begin(), pairs.end());

  // Random Push() and Pop() with more Pop().
  int current = pairs.size();
  while (!queue.IsEmpty()) {
    EXPECT_GT(current, 0);
    if (absl::Bernoulli(randomizer, 1.0 / 4) && current < pairs.size()) {
      queue.Push(pairs[current].element, pairs[current].priority);
      ++current;
    } else {
      --current;
      EXPECT_EQ(pairs[current].element, queue.Pop());
    }
  }
}

TEST(PriorityQueueWithRestrictedPushDeathTest, DCHECK) {
  // Don't run this test in opt mode.
  if (!DEBUG_MODE) GTEST_SKIP();

  PriorityQueueWithRestrictedPush<std::string, int> queue;
  EXPECT_TRUE(queue.IsEmpty());
  ASSERT_DEATH(queue.Pop(), "");
  queue.Push("A", 10);
  queue.Push("B", 9);
  ASSERT_DEATH(queue.Push("C", 4), "");
  ASSERT_DEATH(queue.Push("C", 8), "");
}

}  // namespace
}  // namespace operations_research
