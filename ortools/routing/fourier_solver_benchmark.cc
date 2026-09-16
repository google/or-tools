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

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "benchmark/benchmark.h"
#include "ortools/routing/fourier_solver.h"

namespace operations_research::routing {
namespace {

void BM_MaxLinearExpressionEvaluator_Evaluate(benchmark::State& bench_state) {
  const int num_variables = bench_state.range(0);
  const int num_constraints = bench_state.range(1);
  // Make constraints.
  std::vector<std::vector<double>> rows;
  for (int i = 0; i < num_constraints; ++i) {
    std::vector<double> row;
    row.reserve(num_variables);
    for (int j = 0; j < num_variables; ++j) {
      row.push_back(i + 1);
    }
    rows.push_back(std::move(row));
  }
  MaxLinearExpressionEvaluator evaluator(rows);
  std::vector<double> values(num_variables, 0.0);
  absl::c_iota(values, 1);
  int64_t num_items = 0;
  for (auto _ : bench_state) {
    benchmark::DoNotOptimize(evaluator.Evaluate(values));
    ++num_items;
  }
  bench_state.SetItemsProcessed(num_items);
  bench_state.SetBytesProcessed(num_items * num_constraints * num_variables *
                                sizeof(double));
}

BENCHMARK(BM_MaxLinearExpressionEvaluator_Evaluate)
    ->RangePair(1, 1 << 8, 1, 1 << 8);

}  // namespace
}  // namespace operations_research::routing
