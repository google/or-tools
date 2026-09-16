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

#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/generic_max_flow.h"
#include "ortools/graph_base/flow_graph.h"
#include "ortools/graph_base/graph.h"

namespace operations_research {
namespace {

typedef int64_t FlowQuantity;

template <typename Graph>
void AddSourceAndSink(const typename Graph::NodeIndex num_tails,
                      const typename Graph::NodeIndex num_heads,
                      typename Graph::Builder& graph_builder) {
  const typename Graph::NodeIndex source = num_tails + num_heads;
  const typename Graph::NodeIndex sink = num_tails + num_heads + 1;
  for (typename Graph::NodeIndex tail = 0; tail < num_tails; ++tail) {
    graph_builder.AddArc(source, tail);
  }
  for (typename Graph::NodeIndex head = 0; head < num_heads; ++head) {
    graph_builder.AddArc(num_tails + head, sink);
  }
}

template <typename Graph>
void GenerateCompleteGraphWithSourceAndSink(
    const typename Graph::NodeIndex num_tails,
    const typename Graph::NodeIndex num_heads,
    typename Graph::Builder& graph_builder) {
  const typename Graph::NodeIndex num_nodes = num_tails + num_heads + 2;
  const typename Graph::ArcIndex num_arcs =
      num_tails * num_heads + num_tails + num_heads;
  graph_builder.Reserve(num_nodes, num_arcs);
  for (typename Graph::NodeIndex tail = 0; tail < num_tails; ++tail) {
    for (typename Graph::NodeIndex head = 0; head < num_heads; ++head) {
      graph_builder.AddArc(tail, head + num_tails);
    }
  }
  AddSourceAndSink<Graph>(num_tails, num_heads, graph_builder);
}

// Generate a bipartite graph where each right node is connected to `degree`
// random nodes on the left.
template <typename Graph>
void GeneratePartialRandomGraph(absl::BitGenRef random,
                                const typename Graph::NodeIndex num_tails,
                                const typename Graph::NodeIndex num_heads,
                                const typename Graph::NodeIndex degree,
                                typename Graph::Builder& graph_builder) {
  const typename Graph::NodeIndex num_nodes = num_tails + num_heads + 2;
  const typename Graph::ArcIndex num_arcs =
      num_tails * degree + num_tails + num_heads;
  graph_builder.Reserve(num_nodes, num_arcs);
  for (typename Graph::NodeIndex tail = 0; tail < num_tails; ++tail) {
    for (typename Graph::NodeIndex d = 0; d < degree; ++d) {
      const typename Graph::NodeIndex head =
          absl::Uniform(random, 0, num_heads);
      graph_builder.AddArc(tail, head + num_tails);
    }
  }
  AddSourceAndSink<Graph>(num_tails, num_heads, graph_builder);
}

template <typename Graph>
void GenerateRandomArcValuations(absl::BitGenRef random,
                                 const typename Graph::ArcIndex num_arcs,
                                 const int64_t max_range,
                                 std::vector<int64_t>* arc_valuation) {
  arc_valuation->resize(num_arcs);
  for (typename Graph::ArcIndex arc = 0; arc < num_arcs; ++arc) {
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
  CHECK(max_flow->Solve());
  CHECK_EQ(GenericMaxFlow<Graph>::OPTIMAL, max_flow->status());
  const Graph* graph = max_flow->graph();
  for (typename Graph::ArcIndex arc = 0; arc < graph->num_arcs(); ++arc) {
    const typename Graph::ArcIndex opposite_arc = graph->OppositeArc(arc);
    CHECK_EQ(max_flow->Flow(arc), -max_flow->Flow(opposite_arc));
    if (max_flow->Flow(arc) > 0) {
      CHECK_LE(max_flow->Flow(arc), max_flow->Capacity(arc));
    } else {
      CHECK_LE(0, max_flow->Flow(opposite_arc));
      CHECK_LE(max_flow->Flow(opposite_arc), max_flow->Capacity(opposite_arc));
    }
  }
  return max_flow->GetOptimalFlow();
}

template <typename Graph>
struct MaxFlowSolver {
  typedef FlowQuantity (*Solver)(GenericMaxFlow<Graph>* max_flow);
};

template <typename Graph>
FlowQuantity FullAssignment(typename MaxFlowSolver<Graph>::Solver f,
                            typename Graph::NodeIndex num_tails,
                            typename Graph::NodeIndex num_heads) {
  typename Graph::Builder graph_builder;
  GenerateCompleteGraphWithSourceAndSink<Graph>(num_tails, num_heads,
                                                graph_builder);
  const auto graph = std::move(graph_builder).Build(nullptr);
  std::vector<int64_t> arc_capacity(graph->num_arcs(), 1);
  std::unique_ptr<GenericMaxFlow<Graph>> max_flow(new GenericMaxFlow<Graph>(
      graph.get(), graph->num_nodes() - 2, graph->num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());

  return f(max_flow.get());
}

template <typename Graph>
FlowQuantity PartialRandomAssignment(typename MaxFlowSolver<Graph>::Solver f,
                                     typename Graph::NodeIndex num_tails,
                                     typename Graph::NodeIndex num_heads) {
  std::mt19937 random(0);

  const typename Graph::NodeIndex kDegree = 3;
  typename Graph::Builder graph_builder;
  GeneratePartialRandomGraph<Graph>(random, num_tails, num_heads, kDegree,
                                    graph_builder);
  std::vector<int64_t> arc_capacity(graph_builder.num_arcs(), 1);

  std::vector<int> permutation;
  const auto graph = std::move(graph_builder).Build(&permutation);
  arc_capacity.resize(graph->num_arcs(), 0);  // In case Build() adds more arcs.
  util::Permute(permutation, &arc_capacity);

  std::unique_ptr<GenericMaxFlow<Graph>> max_flow(new GenericMaxFlow<Graph>(
      graph.get(), graph->num_nodes() - 2, graph->num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());

  return f(max_flow.get());
}

template <typename Graph>
FlowQuantity PartialRandomFlow(typename MaxFlowSolver<Graph>::Solver f,
                               typename Graph::NodeIndex num_tails,
                               typename Graph::NodeIndex num_heads) {
  std::mt19937 random(0);

  const typename Graph::NodeIndex kDegree = 10;
  const FlowQuantity kCapacityRange = 10000;
  typename Graph::Builder graph_builder;
  GeneratePartialRandomGraph<Graph>(random, num_tails, num_heads, kDegree,
                                    graph_builder);
  std::vector<int64_t> arc_capacity(graph_builder.num_arcs());
  GenerateRandomArcValuations<Graph>(random, graph_builder.num_arcs(),
                                     kCapacityRange, &arc_capacity);

  std::vector<typename Graph::ArcIndex> permutation;
  const auto graph = std::move(graph_builder).Build(&permutation);
  arc_capacity.resize(graph->num_arcs(), 0);  // In case Build() adds more arcs.
  util::Permute(permutation, &arc_capacity);

  std::unique_ptr<GenericMaxFlow<Graph>> max_flow(new GenericMaxFlow<Graph>(
      graph.get(), graph->num_nodes() - 2, graph->num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());

  return f(max_flow.get());
}

template <typename Graph>
FlowQuantity FullRandomFlow(typename MaxFlowSolver<Graph>::Solver f,
                            typename Graph::NodeIndex num_tails,
                            typename Graph::NodeIndex num_heads) {
  std::mt19937 random(0);

  const FlowQuantity kCapacityRange = 10000;
  typename Graph::Builder graph_builder;
  GenerateCompleteGraphWithSourceAndSink<Graph>(num_tails, num_heads,
                                                graph_builder);
  std::vector<int64_t> arc_capacity(graph_builder.num_arcs());
  GenerateRandomArcValuations<Graph>(random, graph_builder.num_arcs(),
                                     kCapacityRange, &arc_capacity);

  std::vector<typename Graph::ArcIndex> permutation;
  const auto graph = std::move(graph_builder).Build(&permutation);
  arc_capacity.resize(graph->num_arcs(), 0);  // In case Build() adds more arcs.
  util::Permute(permutation, &arc_capacity);

  std::unique_ptr<GenericMaxFlow<Graph>> max_flow(new GenericMaxFlow<Graph>(
      graph.get(), graph->num_nodes() - 2, graph->num_nodes() - 1));
  SetUpNetworkData(arc_capacity, max_flow.get());

  return f(max_flow.get());
}

template <typename Graph>
static void BM_FullRandomAssignment(benchmark::State& state) {
  const int kSize = 3000;
  for (auto _ : state) {
    // In a complete graph we should always reach the maximum flow.
    const auto flow = FullAssignment<Graph>(SolveMaxFlow, kSize, kSize);
    CHECK_EQ(flow, kSize);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kSize);
}

template <typename Graph>
static void BM_PartialRandomAssignment(benchmark::State& state) {
  const int kSize = 10100;
  for (auto _ : state) {
    const auto flow =
        PartialRandomAssignment<Graph>(SolveMaxFlow, kSize, kSize);
    CHECK_EQ(flow, 9512);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kSize);
}

template <typename Graph>
static void BM_PartialRandomFlow(benchmark::State& state) {
  const int kSize = 800;
  for (auto _ : state) {
    const auto flow = PartialRandomFlow<Graph>(SolveMaxFlow, kSize, kSize);
    CHECK_EQ(flow, 3939172);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) * kSize);
}

template <typename Graph>
static void BM_FullRandomFlow(benchmark::State& state) {
  const int kSize = 800;
  for (auto _ : state) {
    const auto flow = FullRandomFlow<Graph>(SolveMaxFlow, kSize, kSize);
    CHECK_EQ(flow, 3952652);
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

}  // namespace
}  // namespace operations_research
