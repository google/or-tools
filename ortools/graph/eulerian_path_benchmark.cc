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

#include "absl/log/check.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/eulerian_path.h"
#include "ortools/graph_base/graph.h"

namespace operations_research {
namespace {

template <typename GraphType>
static void BM_EulerianTourOnGrid(benchmark::State& state) {
  int size = state.range(0);
  const int num_nodes = size * size;
  const int num_edges = 2 * size * (size - 1) + 2 * size - 4;
  util::ReverseArcListGraph<int, int> graph(num_nodes, num_edges);
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      if (j < size - 1) {
        graph.AddArc(i * size + j, i * size + j + 1);
      }
      if (i < size - 1) {
        graph.AddArc(i * size + j, (i + 1) * size + j);
      }
    }
  }
  for (int i = 1; i < size - 1; ++i) {
    graph.AddArc(i * size, i * size + size - 1);
    graph.AddArc(i, (size - 1) * size + i);
  }
  for (auto _ : state) {
    CHECK_EQ(num_edges + 1, BuildEulerianTour(graph).size());
  }
}

BENCHMARK_TEMPLATE(BM_EulerianTourOnGrid, util::ReverseArcStaticGraph<>)
    ->Range(2, 1 << 10);

}  // namespace
}  // namespace operations_research
