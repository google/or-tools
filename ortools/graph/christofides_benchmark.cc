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

#include <cmath>
#include <cstdlib>
#include <vector>

#include "absl/log/check.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/christofides.h"

namespace operations_research {
namespace {

// Benchmark for the Christofides algorithm on a 'size' by 'size' grid of nodes.
template <bool use_minimal_matching>
void BM_ChristofidesPathSolver(benchmark::State& state) {
  int size = state.range(0);
  const int num_nodes = size * size;
  std::vector<std::vector<int>> costs(num_nodes);
  for (int i = 0; i < num_nodes; ++i) {
    const int x_i = i / size;
    const int y_i = i % size;
    costs[i].resize(num_nodes, 0);
    for (int j = 0; j < num_nodes; ++j) {
      const int x_j = j / size;
      const int y_j = j % size;
      costs[i][j] = std::abs(x_i - x_j) + std::abs(y_i - y_j);
    }
  }
  auto cost = [&costs](int i, int j) { return costs[i][j]; };
  // TODO(user) MSVC v19.41 can't convert lambda to std::function.
#if defined(_MSC_VER)
  using Cost = std::function<int(int, int)>;
#else
  using Cost = decltype(cost);
#endif
  using MatchingAlgorithm =
      typename ChristofidesPathSolver<int, int, int, Cost>::MatchingAlgorithm;
  for (auto _ : state) {
    ChristofidesPathSolver<int, int, int, Cost> chris_solver(num_nodes, cost);
    if (use_minimal_matching) {
      chris_solver.SetMatchingAlgorithm(
          MatchingAlgorithm::MINIMAL_WEIGHT_MATCHING);
    } else {
      chris_solver.SetMatchingAlgorithm(
          MatchingAlgorithm::MINIMUM_WEIGHT_MATCHING);
    }
    auto result = chris_solver.TravelingSalesmanCost();
    CHECK_OK(result.status());
    CHECK_GT(result.value(), 0);
  }
}

BENCHMARK_TEMPLATE(BM_ChristofidesPathSolver, true)->Range(2, 1 << 5);
BENCHMARK_TEMPLATE(BM_ChristofidesPathSolver, false)->Range(2, 1 << 5);

}  // namespace
}  // namespace operations_research
