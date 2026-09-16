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
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/sat/2d_packing_brute_force.h"
#include "ortools/sat/integer_base.h"

namespace operations_research {
namespace sat {
namespace {

struct OppProblem {
  std::vector<IntegerValue> items_x_sizes;
  std::vector<IntegerValue> items_y_sizes;
  std::pair<IntegerValue, IntegerValue> bb_sizes;
};

OppProblem CreateRandomOppProblem(absl::BitGenRef random, int num_items) {
  OppProblem result;
  std::vector<IntegerValue>& items_x_sizes = result.items_x_sizes;
  std::vector<IntegerValue>& items_y_sizes = result.items_y_sizes;
  const int size = 300;
  items_x_sizes.clear();
  items_y_sizes.clear();
  IntegerValue area = 0;
  for (int i = 0; i < num_items; ++i) {
    const IntegerValue x_size = absl::Uniform(random, 1, size);
    const IntegerValue y_size = absl::Uniform(random, 1, size);
    items_x_sizes.push_back(x_size);
    items_y_sizes.push_back(y_size);
    area += x_size * y_size;
  }
  const IntegerValue box_x_size =
      absl::Uniform(random, size, static_cast<int64_t>(sqrt(num_items) * size));
  const IntegerValue box_y_size =
      std::max(IntegerValue(size), (area + box_x_size - 1) / box_x_size);
  result.bb_sizes = {box_x_size, box_y_size};
  return result;
}

void BM_BruteForceOrthogonalPacking(benchmark::State& state) {
  absl::BitGen random;
  static constexpr int kNumProblems = 100;
  std::vector<OppProblem> problems;
  const bool feasible = state.range(1);
  while (problems.size() < kNumProblems) {
    OppProblem problem = CreateRandomOppProblem(random, state.range(0));
    if ((BruteForceOrthogonalPacking(problem.items_x_sizes,
                                     problem.items_y_sizes, problem.bb_sizes,
                                     problem.items_x_sizes.size())
             .status == BruteForceResult::Status::kFoundSolution) == feasible) {
      problems.push_back(problem);
    }
  }
  int index = 0;
  for (auto s : state) {
    const auto& problem = problems[index];
    BruteForceOrthogonalPacking(problem.items_x_sizes, problem.items_y_sizes,
                                problem.bb_sizes, problem.items_x_sizes.size());
    ++index;
    if (index == problems.size()) {
      index = 0;
    }
  }
}

BENCHMARK(BM_BruteForceOrthogonalPacking)
    ->ArgPair(3, false)
    ->ArgPair(4, false)
    ->ArgPair(5, false)
    ->ArgPair(6, false)
    ->ArgPair(7, false)
    ->ArgPair(8, false)
    ->ArgPair(9, false)
    ->ArgPair(3, true)
    ->ArgPair(4, true)
    ->ArgPair(5, true)
    ->ArgPair(6, true)
    ->ArgPair(7, true)
    ->ArgPair(8, true)
    ->ArgPair(9, true);

}  // namespace
}  // namespace sat
}  // namespace operations_research
