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

#include <vector>

#include "absl/algorithm/container.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/minimum_vertex_cover.h"

namespace operations_research {
namespace {

std::vector<std::vector<int>> MakeCompleteBipartiteGraph(int num_left,
                                                         int num_right) {
  std::vector<int> adjacencies(num_right);
  absl::c_iota(adjacencies, num_left);
  return std::vector<std::vector<int>>(num_left, adjacencies);
}

void BM_CompleteBipartite(benchmark::State& state) {
  const int num_left = state.range(0);
  const int num_right = state.range(1);
  std::vector<std::vector<int>> left_to_right =
      MakeCompleteBipartiteGraph(num_left, num_right);
  for (auto _ : state) {
    BipartiteMinimumVertexCover(left_to_right, num_right);
  }
}

BENCHMARK(BM_CompleteBipartite)
    ->ArgPair(1, 128)
    ->ArgPair(128, 1)
    ->ArgPair(32, 32)
    ->ArgPair(8, 64)
    ->ArgPair(64, 8);

}  // namespace
}  // namespace operations_research
