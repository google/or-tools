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

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/constraint_solver/constraint_solver.h"

ABSL_DECLARE_FLAG(bool, cp_disable_expression_optimization);
ABSL_DECLARE_FLAG(bool, cp_use_element_rmq);

namespace operations_research {
namespace {

class ReduceIndexMax : public DecisionBuilder {
 public:
  ReduceIndexMax(benchmark::State* state, IntVar* index, IntExpr* expr,
                 int64_t min_index, int iterations)
      : state_(state),
        index_(index),
        expr_(expr),
        min_index_(min_index),
        iterations_(iterations) {}
  Decision* Next(Solver* solver) override {
    const int64_t index_max = index_->Max();
    const int64_t domain_size = index_max - min_index_ + 1;
    index_->SetMin(min_index_);
    for (int i = 0; i < iterations_; ++i) {
      const int64_t max_index = index_max - i * domain_size / iterations_;
      state_->ResumeTiming();
      index_->SetMax(max_index);
      CHECK_LE(expr_->Min(), expr_->Max());
      state_->PauseTiming();
    }
    return nullptr;
  }

 private:
  benchmark::State* state_;
  IntVar* const index_;
  IntExpr* const expr_;
  const int64_t min_index_;
  const int iterations_;
};

template <bool use_rmq>
void BM_GetIntExprElementRange(benchmark::State& state) {
  const int num_values = state.range(0);
  static const int kValueRange = 1000000;
  absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
  absl::SetFlag(&FLAGS_cp_use_element_rmq, use_rmq);
  std::mt19937 random(12345);
  Solver solver("IntExprElement");
  std::vector<int64_t> values(num_values);
  for (int i = 0; i < num_values; ++i) {
    values[i] = absl::Uniform(random, 0, kValueRange);
  }
  IntVar* const index = solver.MakeIntVar(0, num_values - 1);
  IntExpr* const expr = solver.MakeElement(values, index);
  const int64_t kBatchIterations = 1024;
  int i = 0;
  while (state.KeepRunningBatch(kBatchIterations)) {
    state.PauseTiming();
    DecisionBuilder* const db = solver.RevAlloc(new ReduceIndexMax(
        &state, index, expr,
        (i++ % kBatchIterations) * num_values / kBatchIterations,
        kBatchIterations));
    solver.Solve(db);
    state.ResumeTiming();
  }
}

BENCHMARK_TEMPLATE(BM_GetIntExprElementRange, true)->Range(4000, 10000000);
BENCHMARK_TEMPLATE(BM_GetIntExprElementRange, false)->Range(4000, 10000000);

}  // namespace
}  // namespace operations_research
