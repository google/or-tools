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

#include <random>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/flow_graph.h"

namespace {

void BM_FlowGraph_Build(benchmark::State& state) {
  const bool detect_reverse = state.range(0);
  const bool sort_by_head = state.range(1);
  constexpr int kNumNodes = 1000;
  constexpr int kNumArcs = kNumNodes * kNumNodes / 100;

  // Lets create a random graph with possible self-arc or duplicates.
  std::mt19937 random(123);
  std::vector<std::pair<int, int>> arcs;
  arcs.reserve(kNumArcs);
  for (int i = 0; i < kNumArcs; ++i) {
    arcs.push_back({absl::Uniform<int>(random, 0, kNumNodes),
                    absl::Uniform<int>(random, 0, kNumNodes)});
  }
  for (const auto _ : state) {
    auto graph_builder = util::FlowGraph<>::Builder()
                             .SetDetectReverse(detect_reverse)
                             .SetSortByHead(sort_by_head);
    for (const auto [tail, head] : arcs) {
      graph_builder.AddArc(tail, head);
    }
    const auto graph = std::move(graph_builder).Build(nullptr);
  }
}
BENCHMARK(BM_FlowGraph_Build)
    ->ArgPair(0, 0)
    ->ArgPair(0, 1)
    ->ArgPair(1, 0)
    ->ArgPair(1, 1);

}  // namespace
