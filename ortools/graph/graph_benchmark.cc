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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/graph.h"

namespace util {
namespace {

DEFINE_STRONG_INT_TYPE(StrongArcId, int32_t);

template <typename Graph>
class BMGraphBuilder : public Graph::Builder {
  using Base = Graph::Builder;
  using Base::Base;
};

template <typename GraphType, bool reserve>
static void BM_RandomArcs(benchmark::State& state) {
  const int kRandomSeed = 0;
  const int kNodes = 10 * 1000 * 1000;
  const int kArcs = 5 * kNodes;
  int items_processed = 0;
  for (auto _ : state) {
    BMGraphBuilder<GraphType> builder;
    if (reserve) {
      builder.Reserve(kNodes, kArcs);
    }
    std::mt19937 randomizer(kRandomSeed);
    for (int i = 0; i < kArcs; ++i) {
      builder.AddArc(absl::Uniform<int32_t>(randomizer, 0, kNodes),
                     absl::Uniform<int32_t>(randomizer, 0, kNodes));
    }
    const auto graph = std::move(builder).Build(nullptr);
    items_processed += kArcs;
  }
  state.SetItemsProcessed(items_processed);
}

template <typename GraphType, bool reserve>
static void BM_OrderedArcs(benchmark::State& state) {
  const int kRandomSeed = 0;
  const int kNodes = 10 * 1000 * 1000;
  const int kDegree = 5;
  const int kArcs = kDegree * kNodes;
  int items_processed = 0;
  for (auto _ : state) {
    BMGraphBuilder<GraphType> builder;
    if (reserve) {
      builder.Reserve(kNodes, kArcs);
    }
    std::mt19937 randomizer(kRandomSeed);
    for (int i = 0; i < kNodes; ++i) {
      for (int j = 0; j < kDegree; ++j) {
        builder.AddArc(i, absl::Uniform<int32_t>(randomizer, 0, kNodes));
      }
    }
    const auto graph = std::move(builder).Build(nullptr);
    items_processed += kArcs;
  }
  state.SetItemsProcessed(items_processed);
}

// This is just here to get some timing on the AddArc() function to see how the
// GraphType building time is split between the AddArc() calls and the actual
// Build() call. It is not useful for all type of graph.
template <typename GraphType, bool reserve>
static void BM_RandomArcsBeforeBuild(benchmark::State& state) {
  const int kRandomSeed = 0;
  const int kNodes = 10 * 1000 * 1000;
  const int kArcs = 5 * kNodes;
  int items_processed = 0;
  for (auto _ : state) {
    BMGraphBuilder<GraphType> builder;
    if (reserve) {
      builder.Reserve(kNodes, kArcs);
    }
    std::mt19937 randomizer(kRandomSeed);
    for (int i = 0; i < kArcs; ++i) {
      builder.AddArc(absl::Uniform<int32_t>(randomizer, 0, kNodes),
                     absl::Uniform<int32_t>(randomizer, 0, kNodes));
    }
    items_processed += kArcs;
  }
  state.SetItemsProcessed(items_processed);
}

// A basic vector<vector<>> graph implementation that many people uses. It is
// quite slower and use more memory than a static graph, except maybe during
// construction.
using VectorVectorGraph = std::vector<std::vector<int32_t>>;

template <>
class BMGraphBuilder<VectorVectorGraph> {
 public:
  BMGraphBuilder() : graph_(std::make_unique<VectorVectorGraph>()) {}

  void Reserve(int32_t num_nodes, int32_t num_arcs) {
    graph_->resize(num_nodes);
  }

  void AddArc(int32_t tail, int32_t head) { (*graph_)[tail].push_back(head); }

  std::unique_ptr<VectorVectorGraph> Build(std::nullptr_t) && {
    return std::move(graph_);
  }

 private:
  std::unique_ptr<VectorVectorGraph> graph_;
};

#define RESERVE true
#define NO_RESERVE false
BENCHMARK_TEMPLATE2(BM_RandomArcsBeforeBuild, StaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcsBeforeBuild, StaticGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcsBeforeBuild, ReverseArcStaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcsBeforeBuild, ReverseArcStaticGraph<>,
                    NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ListGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ListGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, StaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, StaticGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, VectorVectorGraph, RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ReverseArcListGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ReverseArcListGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ReverseArcStaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_OrderedArcs, ReverseArcStaticGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ListGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ListGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, StaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, StaticGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, VectorVectorGraph, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ReverseArcListGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ReverseArcListGraph<>, NO_RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ReverseArcStaticGraph<>, RESERVE);
BENCHMARK_TEMPLATE2(BM_RandomArcs, ReverseArcStaticGraph<>, NO_RESERVE);
#undef RESERVE
#undef NO_RESERVE

template <typename GraphType>
GraphType BuildGraphForIterationsBenchmarks() {
  const int kRandomSeed = 0;
  const int kNodes = 10 * 1000 * 1000;
  const int kArcs = 5 * kNodes;
  typename GraphType::Builder builder;
  builder.Reserve(kNodes, kArcs);
  std::mt19937 randomizer(kRandomSeed);
  for (int i = 0; i < kArcs; ++i) {
    builder.AddArc(absl::Uniform<int32_t>(randomizer, 0, kNodes),
                   absl::Uniform<int32_t>(randomizer, 0, kNodes));
  }
  return std::move(builder).BuildGraph(nullptr);
}

template <typename GraphType>
static void BM_OutgoingIterations(benchmark::State& state) {
  const auto graph = BuildGraphForIterationsBenchmarks<GraphType>();
  int64_t num_arcs = 0;
  int64_t some_work = 0;
  for (auto _ : state) {
    for (int node = 0; node < graph.num_nodes(); ++node) {
      for (const int arc : graph.OutgoingArcs(node)) {
        some_work += graph.Head(arc);
        ++num_arcs;
      }
    }
  }
  CHECK_GT(some_work, 0);
  state.SetItemsProcessed(num_arcs);
}

BENCHMARK_TEMPLATE(BM_OutgoingIterations, ListGraph<>);
BENCHMARK_TEMPLATE(BM_OutgoingIterations, StaticGraph<>);
BENCHMARK_TEMPLATE(BM_OutgoingIterations, ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_OutgoingIterations, ReverseArcStaticGraph<>);

template <typename GraphType>
static void BM_IncomingIterations(benchmark::State& state) {
  const auto graph = BuildGraphForIterationsBenchmarks<GraphType>();
  int64_t num_arcs = 0;
  int64_t some_work = 0;
  for (auto _ : state) {
    for (int node = 0; node < graph.num_nodes(); ++node) {
      for (const int arc : graph.IncomingArcs(node)) {
        some_work += graph.Tail(arc);
        ++num_arcs;
      }
    }
  }
  CHECK_GT(some_work, 0);
  state.SetItemsProcessed(num_arcs);
}

BENCHMARK_TEMPLATE(BM_IncomingIterations, ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_IncomingIterations, ReverseArcStaticGraph<>);

template <typename GraphType>
static void BM_OppositeIncomingIterations(benchmark::State& state) {
  const auto graph = BuildGraphForIterationsBenchmarks<GraphType>();
  int64_t num_arcs = 0;
  int64_t some_work = 0;
  for (auto _ : state) {
    for (int node = 0; node < graph.num_nodes(); ++node) {
      for (const int arc : graph.OppositeIncomingArcs(node)) {
        some_work += graph.Head(arc);
        ++num_arcs;
      }
    }
  }
  CHECK_GT(some_work, 0);
  state.SetItemsProcessed(num_arcs);
}

BENCHMARK_TEMPLATE(BM_OppositeIncomingIterations, ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_OppositeIncomingIterations, ReverseArcStaticGraph<>);

template <typename GraphType>
static void BM_OutgoingOrOppositeIncomingIterations(benchmark::State& state) {
  const auto graph = BuildGraphForIterationsBenchmarks<GraphType>();
  int64_t num_arcs = 0;
  for (auto _ : state) {
    for (int node = 0; node < graph.num_nodes(); ++node) {
      for (const int arc : graph.OutgoingOrOppositeIncomingArcs(node)) {
        auto head = graph.Head(arc);
        benchmark::DoNotOptimize(head);
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * num_arcs);
}

BENCHMARK_TEMPLATE(BM_OutgoingOrOppositeIncomingIterations,
                   ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_OutgoingOrOppositeIncomingIterations,
                   ReverseArcStaticGraph<>);

// It's bit sad, but having two loops to iterate over opposite incoming and
// outgoing arcs is much faster than using `OutgoingOrOppositeIncomingArcs`.
template <typename GraphType>
static void BM_OutgoingOrOppositeIncomingIterationsTwoLoops(
    benchmark::State& state) {
  const auto graph = BuildGraphForIterationsBenchmarks<GraphType>();
  for (auto _ : state) {
    for (int node = 0; node < graph.num_nodes(); ++node) {
      const auto work = [&graph](int arc) {
        auto head = graph.Head(arc);
        benchmark::DoNotOptimize(head);
      };
      for (const int arc : graph.OppositeIncomingArcs(node)) {
        work(arc);
      }
      for (const int arc : graph.OutgoingArcs(node)) {
        work(arc);
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * graph.num_arcs());
}
BENCHMARK_TEMPLATE(BM_OutgoingOrOppositeIncomingIterationsTwoLoops,
                   ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_OutgoingOrOppositeIncomingIterationsTwoLoops,
                   ReverseArcStaticGraph<>);

template <typename GraphType>
static void BM_IntegralTypeCopy(benchmark::State& state) {
  const auto graph = BuildGraphForIterationsBenchmarks<GraphType>();

  for (auto s : state) {
    GraphType copied = typename GraphType::Builder().BuildGraph(nullptr);
    benchmark::DoNotOptimize(copied = GraphType(graph));
  }
}

BENCHMARK_TEMPLATE(BM_IntegralTypeCopy, ListGraph<>);
BENCHMARK_TEMPLATE(BM_IntegralTypeCopy, StaticGraph<>);
BENCHMARK_TEMPLATE(BM_IntegralTypeCopy, ReverseArcListGraph<>);
BENCHMARK_TEMPLATE(BM_IntegralTypeCopy, ReverseArcStaticGraph<>);

template <typename GraphType>
void TailHeadBenchmark(benchmark::State& state, const GraphType& graph) {
  // Prevent constant folding. Weird construct due to b/284459966.
  auto* graph_ptr = &graph;
  benchmark::DoNotOptimize(graph_ptr);
  for (auto s : state) {
    const auto num_arcs = graph.num_arcs();
    for (int arc = 0; arc < num_arcs; ++arc) {
      auto head = graph.Head(arc);
      auto tail = graph.Tail(arc);
      benchmark::DoNotOptimize(head);
      benchmark::DoNotOptimize(tail);
    }
  }
}

template <typename IndexT>
static void BM_CompleteGraphTailHead(benchmark::State& state) {
  constexpr int kNumNodes = 100;
  CompleteGraph<IndexT, IndexT> graph(kNumNodes);
  TailHeadBenchmark(state, graph);
}
BENCHMARK_TEMPLATE(BM_CompleteGraphTailHead, int32_t);
BENCHMARK_TEMPLATE(BM_CompleteGraphTailHead, int16_t);

template <typename IndexT>
static void BM_CompleteBipartiteGraphTailHead(benchmark::State& state) {
  constexpr int kNumLeft = 100;
  CompleteBipartiteGraph<IndexT, IndexT> graph(kNumLeft, kNumLeft);
  TailHeadBenchmark(state, graph);
}
BENCHMARK_TEMPLATE(BM_CompleteBipartiteGraphTailHead, int32_t);
BENCHMARK_TEMPLATE(BM_CompleteBipartiteGraphTailHead, int16_t);

template <typename ArrayT, typename IndexT>
void BM_Permute(benchmark::State& state) {
  const int size = state.range(0);
  ArrayT array(size);

  std::vector<IndexT> permutation(size);
  absl::c_iota(permutation, IndexT(0));

  for (const auto s : state) {
    util::Permute(permutation, &array);
    benchmark::DoNotOptimize(array);
    benchmark::DoNotOptimize(permutation);
  }
}
BENCHMARK(BM_Permute<util_intops::StrongVector<StrongArcId, int>, StrongArcId>)
    ->Arg(128);
BENCHMARK(BM_Permute<std::vector<int>, int>)->Arg(128);
BENCHMARK(BM_Permute<std::vector<bool>, int>)->Arg(128);

template <typename T, bool kReserve>
static void BM_SVectorGrow(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    internal::SVector<int32_t, T> v;
    if constexpr (kReserve) {
      v.reserve(n);
    }
    for (int32_t i = 0; i < n; ++i) {
      v.grow(T(~i), T(i));
    }
    benchmark::DoNotOptimize(v.data());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE2(BM_SVectorGrow, int32_t, false)->Range(1 << 10, 1 << 20);
BENCHMARK_TEMPLATE2(BM_SVectorGrow, int32_t, true)->Range(1 << 10, 1 << 20);
BENCHMARK_TEMPLATE2(BM_SVectorGrow, int64_t, false)->Range(1 << 10, 1 << 20);
BENCHMARK_TEMPLATE2(BM_SVectorGrow, int64_t, true)->Range(1 << 10, 1 << 20);

template <typename T>
static void BM_SVectorResize(benchmark::State& state) {
  const int n = state.range(0);
  for (auto _ : state) {
    internal::SVector<int32_t, T> v;
    v.resize(n);
    benchmark::DoNotOptimize(v.data());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_SVectorResize, int32_t)->Range(1 << 10, 1 << 20);
BENCHMARK_TEMPLATE(BM_SVectorResize, int64_t)->Range(1 << 10, 1 << 20);

template <typename T>
static void BM_SVectorCopy(benchmark::State& state) {
  const int n = state.range(0);
  internal::SVector<int32_t, T> src;
  src.resize(n);
  for (int32_t i = -n; i < n; ++i) {
    src[i] = T(i);
  }
  for (auto _ : state) {
    internal::SVector<int32_t, T> copy(src);
    benchmark::DoNotOptimize(copy.data());
    benchmark::ClobberMemory();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_SVectorCopy, int32_t)->Range(1 << 10, 1 << 20);
BENCHMARK_TEMPLATE(BM_SVectorCopy, int64_t)->Range(1 << 10, 1 << 20);

template <typename T>
static void BM_SVectorLookup(benchmark::State& state) {
  const int n = state.range(0);
  internal::SVector<int32_t, T> v;
  v.resize(n);
  for (int32_t i = 0; i < n; ++i) {
    v[~i] = T(i);
    v[i] = T(i + 1);
  }
  for (auto _ : state) {
    T sum = 0;
    for (int32_t i = 0; i < n; ++i) {
      sum += v[i] + v[~i];
    }
    benchmark::DoNotOptimize(sum);
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK_TEMPLATE(BM_SVectorLookup, int32_t)->Range(1 << 10, 1 << 20);
BENCHMARK_TEMPLATE(BM_SVectorLookup, int64_t)->Range(1 << 10, 1 << 20);

}  // namespace
}  // namespace util
