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

#include "ortools/graph/generic_max_flow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <random>
#include <string>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/graph/flow_graph.h"
#include "ortools/graph/graph.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {
namespace {

using ::testing::ContainerEq;
using ::testing::WhenSorted;

using FlowQuantity = int64_t;

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
  graph.Build(&permutation);

  GenericMaxFlow<Graph> max_flow(&graph, 0, num_nodes - 1);
  for (typename Graph::ArcIndex arc = 0; arc < num_arcs; ++arc) {
    const int image = arc < permutation.size() ? permutation[arc] : arc;

    max_flow.SetArcCapacity(image, capacity[arc]);
    EXPECT_EQ(max_flow.Capacity(image), capacity[arc]);
  }
  EXPECT_TRUE(max_flow.Solve());
  if (max_flow.status() == GenericMaxFlow<Graph>::OPTIMAL) {
    const typename GenericMaxFlow<Graph>::FlowSumType total_flow =
        max_flow.GetOptimalFlow();
    EXPECT_EQ(expected_total_flow, total_flow);
    for (int arc = 0; arc < num_arcs; ++arc) {
      const int image = arc < permutation.size() ? permutation[arc] : arc;

      EXPECT_EQ(expected_flow[arc], max_flow.Flow(image)) << " arc = " << arc;
    }
  }

  // Tests the min-cut functions.
  if (expected_source_min_cut != nullptr) {
    std::vector<typename Graph::NodeIndex> cut;
    max_flow.GetSourceSideMinCut(&cut);
    std::sort(cut.begin(), cut.end());
    EXPECT_THAT(*expected_source_min_cut, WhenSorted(ContainerEq(cut)));
  }
  if (expected_sink_min_cut != nullptr) {
    std::vector<typename Graph::NodeIndex> cut;
    max_flow.GetSinkSideMinCut(&cut);
    std::sort(cut.begin(), cut.end());
    EXPECT_THAT(*expected_sink_min_cut, WhenSorted(ContainerEq(cut)));
  }

  return max_flow.status();
}

template <typename Graph>
class GenericMaxFlowTest : public ::testing::Test {};

typedef ::testing::Types<util::FlowGraph<>, util::ReverseArcListGraph<>,
                         util::ReverseArcStaticGraph<>>
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
  using FlowQuantity = typename GenericMaxFlow<TypeParam>::FlowSumType;
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
  using FlowQuantity = typename GenericMaxFlow<TypeParam>::FlowSumType;
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

// Using a custom type should allow to verify that we didn't forget a conversion
// somewhere.
//
// TODO(user): Unfortunately, there is no open-source version of strong int
// supporting uint16_t...
struct StrongUint16 {
  constexpr StrongUint16() : v(0) {}
  constexpr StrongUint16(int v) : v(v) {}

  explicit operator int64_t() const { return static_cast<int64_t>(v); }

  StrongUint16 operator-() const { return StrongUint16(-v); }
  void operator+=(StrongUint16 o) { v += o.v; }
  void operator-=(StrongUint16 o) { v -= o.v; }

  uint16_t v;
};
std::ostream& operator<<(std::ostream& os, StrongUint16 a) { return os << a.v; }
constexpr StrongUint16 operator+(StrongUint16 a, StrongUint16 b) {
  return StrongUint16(a.v + b.v);
}
constexpr StrongUint16 operator-(StrongUint16 a, StrongUint16 b) {
  return StrongUint16(a.v - b.v);
}
constexpr bool operator<=(StrongUint16 a, StrongUint16 b) { return a.v <= b.v; }
constexpr bool operator>=(StrongUint16 a, StrongUint16 b) { return a.v >= b.v; }
constexpr bool operator>(StrongUint16 a, StrongUint16 b) { return a.v > b.v; }
constexpr bool operator<(StrongUint16 a, StrongUint16 b) { return a.v < b.v; }
constexpr bool operator==(StrongUint16 a, StrongUint16 b) { return a.v == b.v; }

// This is a bit hacky, but we need to define the overload in the std namespace.
}  // namespace
}  // namespace operations_research
namespace std {
template <>
struct numeric_limits<operations_research::StrongUint16> {
  static constexpr operations_research::StrongUint16 max() {
    return operations_research::StrongUint16(numeric_limits<uint16_t>::max());
  }
};
}  // namespace std
namespace operations_research {
namespace {

TYPED_TEST(GenericMaxFlowTest, SmallFlowTypes) {
  absl::BitGen random;
  const int num_nodes = 1'000;
  const int num_arcs = num_nodes * num_nodes;

  // Lets generate and build a random graph.
  // We should have more than enough arc to make it fully connected.
  TypeParam graph(num_nodes, num_arcs);
  for (int arc = 0; arc < num_arcs; ++arc) {
    graph.AddArc(absl::Uniform(random, 0, num_nodes),
                 absl::Uniform(random, 0, num_nodes));
  }
  graph.Build();

  typedef GenericMaxFlow<TypeParam, StrongUint16, int64_t> MaxFlowA;
  typedef GenericMaxFlow<TypeParam, int64_t, int64_t> MaxFlowB;
  MaxFlowA max_flow_A(&graph, /*source=*/0, /*sink=*/num_nodes - 1);
  MaxFlowB max_flow_B(&graph, /*source=*/0, /*sink=*/num_nodes - 1);
  for (int arc = 0; arc < num_arcs; ++arc) {
    const uint16_t capa =
        absl::Uniform(random, 0, std::numeric_limits<uint16_t>::max() / 2);
    max_flow_A.SetArcCapacity(arc, capa);
    max_flow_B.SetArcCapacity(arc, capa);
  }
  EXPECT_EQ(max_flow_A.Solve(), MaxFlowA::OPTIMAL);
  EXPECT_EQ(max_flow_B.Solve(), MaxFlowB::OPTIMAL);
  EXPECT_EQ(max_flow_A.GetOptimalFlow(), max_flow_B.GetOptimalFlow());
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
void GenerateCompleteGraphWithSourceAndSink(
    const typename Graph::NodeIndex num_tails,
    const typename Graph::NodeIndex num_heads, Graph* graph) {
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

// Generate a bipartite graph where each right node is connected to `degree`
// random nodes on the left.
template <typename Graph>
void GeneratePartialRandomGraph(absl::BitGenRef random,
                                const typename Graph::NodeIndex num_tails,
                                const typename Graph::NodeIndex num_heads,
                                const typename Graph::NodeIndex degree,
                                Graph* graph) {
  const typename Graph::NodeIndex num_nodes = num_tails + num_heads + 2;
  const typename Graph::ArcIndex num_arcs =
      num_tails * degree + num_tails + num_heads;
  graph->Reserve(num_nodes, num_arcs);
  for (typename Graph::NodeIndex tail = 0; tail < num_tails; ++tail) {
    for (typename Graph::NodeIndex d = 0; d < degree; ++d) {
      const typename Graph::NodeIndex head =
          absl::Uniform(random, 0, num_heads);
      graph->AddArc(tail, head + num_tails);
    }
  }
  AddSourceAndSink(num_tails, num_heads, graph);
}

template <typename Graph>
void GenerateRandomArcValuations(absl::BitGenRef random, const Graph& graph,
                                 const int64_t max_range,
                                 std::vector<int64_t>* arc_valuation) {
  const typename Graph::ArcIndex num_arcs = graph.num_arcs();
  arc_valuation->resize(num_arcs);
  for (typename Graph::ArcIndex arc = 0; arc < graph.num_arcs(); ++arc) {
    (*arc_valuation)[arc] = absl::Uniform(random, 0, max_range);
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
    const typename Graph::ArcIndex opposite_arc = graph->OppositeArc(arc);
    EXPECT_EQ(max_flow->Flow(arc), -max_flow->Flow(opposite_arc));
    if (max_flow->Flow(arc) > 0) {
      EXPECT_LE(max_flow->Flow(arc), max_flow->Capacity(arc));
    } else {
      EXPECT_LE(0, max_flow->Flow(opposite_arc));
      EXPECT_LE(max_flow->Flow(opposite_arc), max_flow->Capacity(opposite_arc));
    }
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
  for (const typename Graph::ArcIndex arc : graph->OutgoingArcs(source_index)) {
    objective->SetCoefficient(var[arc], -1.0);
  }
  solver.Solve();
  return static_cast<FlowQuantity>(std::round(-objective->Value()));
}

template <typename Graph>
struct MaxFlowSolver {
  typedef FlowQuantity (*Solver)(GenericMaxFlow<Graph>* max_flow);
};

template <typename Graph>
void FullAssignment(std::optional<FlowQuantity> unused,
                    typename MaxFlowSolver<Graph>::Solver f,
                    typename Graph::NodeIndex num_tails,
                    typename Graph::NodeIndex num_heads) {
  Graph graph;
  GenerateCompleteGraphWithSourceAndSink(num_tails, num_heads, &graph);
  graph.Build();
  std::vector<int64_t> arc_capacity(graph.num_arcs(), 1);
  std::unique_ptr<GenericMaxFlow<Graph>> max_flow(new GenericMaxFlow<Graph>(
      &graph, graph.num_nodes() - 2, graph.num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());

  // In a complete graph we should always reach the maximum flow.
  const FlowQuantity flow = f(max_flow.get());
  EXPECT_EQ(std::min(num_tails, num_heads), flow);
}

template <typename Graph>
void PartialRandomAssignment(std::optional<FlowQuantity> expected_flow,
                             typename MaxFlowSolver<Graph>::Solver f,
                             typename Graph::NodeIndex num_tails,
                             typename Graph::NodeIndex num_heads) {
  absl::BitGen absl_random;
  std::mt19937 mt_random(0);
  absl::BitGenRef random = expected_flow != std::nullopt
                               ? absl::BitGenRef(mt_random)
                               : absl::BitGenRef(absl_random);

  const typename Graph::NodeIndex kDegree = 3;
  Graph graph;
  GeneratePartialRandomGraph(random, num_tails, num_heads, kDegree, &graph);
  std::vector<int64_t> arc_capacity(graph.num_arcs(), 1);

  std::vector<int> permutation;
  graph.Build(&permutation);
  arc_capacity.resize(graph.num_arcs(), 0);
  util::Permute(permutation, &arc_capacity);

  std::unique_ptr<GenericMaxFlow<Graph>> max_flow(new GenericMaxFlow<Graph>(
      &graph, graph.num_nodes() - 2, graph.num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());

  const FlowQuantity flow = f(max_flow.get());
  if (expected_flow != std::nullopt) {
    EXPECT_EQ(*expected_flow, flow);
  } else {
    // Use the LP as reference value.
    const FlowQuantity expected_flow1 = SolveMaxFlowWithLP(max_flow.get());
    EXPECT_EQ(expected_flow1, flow);
  }
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
void PartialRandomFlow(std::optional<FlowQuantity> expected_flow,
                       typename MaxFlowSolver<Graph>::Solver f,
                       typename Graph::NodeIndex num_tails,
                       typename Graph::NodeIndex num_heads) {
  absl::BitGen absl_random;
  std::mt19937 mt_random(0);
  absl::BitGenRef random = expected_flow != std::nullopt
                               ? absl::BitGenRef(mt_random)
                               : absl::BitGenRef(absl_random);

  const typename Graph::NodeIndex kDegree = 10;
  const FlowQuantity kCapacityRange = 10000;
  const FlowQuantity kCapacityDelta = 1000;
  Graph graph;
  GeneratePartialRandomGraph(random, num_tails, num_heads, kDegree, &graph);
  std::vector<int64_t> arc_capacity(graph.num_arcs());
  GenerateRandomArcValuations(random, graph, kCapacityRange, &arc_capacity);

  std::vector<typename Graph::ArcIndex> permutation;
  graph.Build(&permutation);
  arc_capacity.resize(graph.num_arcs(), 0);  // In case Build() adds more arcs.
  util::Permute(permutation, &arc_capacity);

  std::unique_ptr<GenericMaxFlow<Graph>> max_flow(new GenericMaxFlow<Graph>(
      &graph, graph.num_nodes() - 2, graph.num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());

  if (expected_flow != std::nullopt) {
    FlowQuantity flow = f(max_flow.get());  // Just solve once.
    EXPECT_EQ(flow, *expected_flow);
    return;
  }

  const FlowQuantity expected_flow1 = SolveMaxFlowWithLP(max_flow.get());
  FlowQuantity flow = f(max_flow.get());
  EXPECT_EQ(expected_flow1, flow);

  ChangeCapacities(arc_capacity, kCapacityDelta, max_flow.get());

  const FlowQuantity expected_flow2 = SolveMaxFlowWithLP(max_flow.get());
  flow = f(max_flow.get());
  EXPECT_EQ(expected_flow2, flow);

  ChangeCapacities(arc_capacity, 0, max_flow.get());
  flow = f(max_flow.get());
  EXPECT_EQ(expected_flow1, flow);
}

template <typename Graph>
void FullRandomFlow(std::optional<FlowQuantity> expected_flow,
                    typename MaxFlowSolver<Graph>::Solver f,
                    typename Graph::NodeIndex num_tails,
                    typename Graph::NodeIndex num_heads) {
  absl::BitGen absl_random;
  std::mt19937 mt_random(0);
  absl::BitGenRef random = expected_flow != std::nullopt
                               ? absl::BitGenRef(mt_random)
                               : absl::BitGenRef(absl_random);

  const FlowQuantity kCapacityRange = 10000;
  const FlowQuantity kCapacityDelta = 1000;
  Graph graph;
  GenerateCompleteGraphWithSourceAndSink(num_tails, num_heads, &graph);
  std::vector<int64_t> arc_capacity(graph.num_arcs());
  GenerateRandomArcValuations(random, graph, kCapacityRange, &arc_capacity);

  std::vector<typename Graph::ArcIndex> permutation;
  graph.Build(&permutation);
  arc_capacity.resize(graph.num_arcs(), 0);  // In case Build() adds more arcs.
  util::Permute(permutation, &arc_capacity);

  std::unique_ptr<GenericMaxFlow<Graph>> max_flow(new GenericMaxFlow<Graph>(
      &graph, graph.num_nodes() - 2, graph.num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());

  if (expected_flow != std::nullopt) {
    const FlowQuantity flow = f(max_flow.get());  // Just solve once.
    EXPECT_EQ(flow, expected_flow);
    return;
  }

  const FlowQuantity expected_flow1 = SolveMaxFlowWithLP(max_flow.get());
  FlowQuantity flow = f(max_flow.get());
  EXPECT_EQ(expected_flow1, flow);

  ChangeCapacities(arc_capacity, kCapacityDelta, max_flow.get());
  const FlowQuantity expected_flow2 = SolveMaxFlowWithLP(max_flow.get());
  flow = f(max_flow.get());
  EXPECT_EQ(expected_flow2, flow);

  ChangeCapacities(arc_capacity, 0, max_flow.get());
  flow = f(max_flow.get());
  EXPECT_EQ(expected_flow1, flow);
}

#define LP_AND_FLOW_TEST(test_name, size)                                      \
  TEST(MaxFlowStaticGraphTest, test_name##size) {                              \
    test_name<util::ReverseArcStaticGraph<>>(std::nullopt, SolveMaxFlow, size, \
                                             size);                            \
  }                                                                            \
  TEST(MaxFlowListGraphTest, test_name##size) {                                \
    test_name<util::ReverseArcListGraph<>>(std::nullopt, SolveMaxFlow, size,   \
                                           size);                              \
  }                                                                            \
  TEST(MaxFlowNewGraphTest, test_name##size) {                                 \
    test_name<util::FlowGraph<>>(std::nullopt, SolveMaxFlow, size, size);      \
  }

// These are absl::BitGen random test, so they will always work on different
// graphs.
LP_AND_FLOW_TEST(FullAssignment, 300);
LP_AND_FLOW_TEST(PartialRandomAssignment, 100);
LP_AND_FLOW_TEST(PartialRandomAssignment, 1000);
LP_AND_FLOW_TEST(PartialRandomFlow, 400);
LP_AND_FLOW_TEST(FullRandomFlow, 100);

template <typename Graph>
static void BM_FullRandomAssignment(benchmark::State& state) {
  const int kSize = 3000;
  for (auto _ : state) {
    FullAssignment<Graph>(std::nullopt, SolveMaxFlow, kSize, kSize);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kSize);
}

template <typename Graph>
static void BM_PartialRandomAssignment(benchmark::State& state) {
  const int kSize = 10100;
  for (auto _ : state) {
    PartialRandomAssignment<Graph>(9512, SolveMaxFlow, kSize, kSize);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kSize);
}

template <typename Graph>
static void BM_PartialRandomFlow(benchmark::State& state) {
  const int kSize = 800;
  for (auto _ : state) {
    PartialRandomFlow<Graph>(3939172, SolveMaxFlow, kSize, kSize);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kSize);
}

template <typename Graph>
static void BM_FullRandomFlow(benchmark::State& state) {
  const int kSize = 800;
  for (auto _ : state) {
    FullRandomFlow<Graph>(3952652, SolveMaxFlow, kSize, kSize);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kSize);
}

// Note that these benchmark include the graph creation and generation...
BENCHMARK_TEMPLATE(BM_FullRandomAssignment, util::FlowGraph<>);
BENCHMARK_TEMPLATE(BM_FullRandomAssignment, util::ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_FullRandomAssignment, util::ReverseArcStaticGraph<>);

BENCHMARK_TEMPLATE(BM_PartialRandomFlow, util::FlowGraph<>);
BENCHMARK_TEMPLATE(BM_PartialRandomFlow, util::ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_PartialRandomFlow, util::ReverseArcStaticGraph<>);

BENCHMARK_TEMPLATE(BM_FullRandomFlow, util::FlowGraph<>);
BENCHMARK_TEMPLATE(BM_FullRandomFlow, util::ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_FullRandomFlow, util::ReverseArcStaticGraph<>);

BENCHMARK_TEMPLATE(BM_PartialRandomAssignment, util::FlowGraph<>);
BENCHMARK_TEMPLATE(BM_PartialRandomAssignment, util::ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_PartialRandomAssignment, util::ReverseArcStaticGraph<>);

#undef LP_AND_FLOW_TEST

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
