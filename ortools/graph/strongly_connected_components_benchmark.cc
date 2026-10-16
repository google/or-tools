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
#include <random>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/strongly_connected_components.h"

namespace {

template <typename SccOutput>
static void SccBenchmark(benchmark::State* state, bool is_graph_regular,
                         SccOutput* output) {
  const int kExpectedNumberOfSCCs = is_graph_regular ? 139118 : 4325018;
  const int kNodes = 10'000'000;
  const int kArcs = 50'000'000;
  const int kRandomSeed = 0;

  std::mt19937 randomizer(kRandomSeed);
  std::vector<std::vector<int>> graph(kNodes);

  const auto GetRandomNode = [&randomizer] {
    return absl::Uniform(randomizer, 0, kNodes);
  };

  // We test 2 kind of random graph depending on the value of is_graph_regular.
  if (is_graph_regular) {
    // Similar degree distribution for each nodes.
    for (int i = 0; i < kArcs; ++i) {
      graph[GetRandomNode()].push_back(GetRandomNode());
    }
  } else {
    // Power law degree distribution.
    struct Arc {
      int from;
      int to;
    };
    std::vector<Arc> arcs;
    arcs.reserve(kArcs);
    const auto GetRandomArc = [&randomizer, &arcs] {
      return arcs[absl::Uniform<size_t>(randomizer, 0, arcs.size())];
    };
    const auto SelectRandomNode = [&randomizer, &arcs] {
      return arcs.size() < 10 || absl::Bernoulli(randomizer, 1.0 / 3);
    };
    for (int i = 0; i < kArcs; ++i) {
      int from = SelectRandomNode() ? GetRandomNode() : GetRandomArc().from;
      int to = SelectRandomNode() ? GetRandomNode() : GetRandomArc().to;
      arcs.push_back({.from = from, .to = to});
      graph[from].push_back(to);
    }
  }

  int edges_processed = 0;
  for (auto _ : *state) {
    output->clear();
    FindStronglyConnectedComponents(kNodes, graph, output);
    CHECK_EQ(kExpectedNumberOfSCCs, output->size());
    edges_processed += kArcs;
  }

  // A byte is an edge here.
  state->SetBytesProcessed(edges_processed);
}

static void BM_RandomVectorVectorRegularGraph(benchmark::State& state) {
  std::vector<std::vector<int>> components;
  SccBenchmark(&state, /*is_graph_regular=*/true, &components);
}

static void BM_RandomVectorVectorPowerLawGraph(benchmark::State& state) {
  std::vector<std::vector<int>> components;
  SccBenchmark(&state, /*is_graph_regular=*/false, &components);
}

static void BM_RandomVectorVectorRegularGraphWithCountOnly(
    benchmark::State& state) {
  SccCounterOutput<int> counter;
  SccBenchmark(&state, /*is_graph_regular=*/true, &counter);
}

static void BM_RandomVectorVectorPowerLawGraphWithCountOnly(
    benchmark::State& state) {
  SccCounterOutput<int> counter;
  SccBenchmark(&state, /*is_graph_regular=*/false, &counter);
}

BENCHMARK(BM_RandomVectorVectorRegularGraph);
BENCHMARK(BM_RandomVectorVectorRegularGraphWithCountOnly);
BENCHMARK(BM_RandomVectorVectorPowerLawGraph);
BENCHMARK(BM_RandomVectorVectorPowerLawGraphWithCountOnly);

}  // namespace
