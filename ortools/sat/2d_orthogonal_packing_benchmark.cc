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
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/sat/2d_orthogonal_packing.h"
#include "ortools/sat/2d_orthogonal_packing_testing.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {
namespace {

using DetectorStatus = OrthogonalPackingResult::Status;

struct OppProblem {
  std::vector<IntegerValue> items_x_sizes;
  std::vector<IntegerValue> items_y_sizes;
  std::pair<IntegerValue, IntegerValue> bb_sizes;
};

OppProblem CreateFeasibleOppProblem(absl::BitGenRef random, int max_size) {
  std::vector<RectangleInRange> problem = MakeItemsFromRectangles(
      GenerateNonConflictingRectangles(
          absl::Uniform(random, max_size - 1, max_size), random),
      0, random);

  OppProblem result;
  Rectangle bounding_box;
  bounding_box = {.x_min = std::numeric_limits<IntegerValue>::max(),
                  .x_max = std::numeric_limits<IntegerValue>::min(),
                  .y_min = std::numeric_limits<IntegerValue>::max(),
                  .y_max = std::numeric_limits<IntegerValue>::min()};
  std::vector<IntegerValue>& items_x_sizes = result.items_x_sizes;
  std::vector<IntegerValue>& items_y_sizes = result.items_y_sizes;
  for (const auto& item : problem) {
    items_x_sizes.push_back(item.x_size);
    items_y_sizes.push_back(item.y_size);

    bounding_box.x_min = std::min(bounding_box.x_min, item.bounding_area.x_min);
    bounding_box.x_max = std::max(bounding_box.x_max, item.bounding_area.x_max);
    bounding_box.y_min = std::min(bounding_box.y_min, item.bounding_area.y_min);
    bounding_box.y_max = std::max(bounding_box.y_max, item.bounding_area.y_max);
  }
  result.bb_sizes = {bounding_box.SizeX(), bounding_box.SizeY()};
  return result;
}

void BM_OrthogonalPackingInfeasibilityDetector(benchmark::State& state) {
  absl::BitGen random;
  SharedStatistics stats;
  OrthogonalPackingInfeasibilityDetector opp_solver(random, &stats);
  std::vector<OppProblem> problems;
  for (int i = 0; i < 10; ++i) {
    problems.push_back(CreateFeasibleOppProblem(random, state.range(0)));
  }
  int index = 0;
  for (auto s : state) {
    const auto& problem = problems[index];
    CHECK(opp_solver
              .TestFeasibility(problem.items_x_sizes, problem.items_y_sizes,
                               problem.bb_sizes)
              .GetResult() != DetectorStatus::INFEASIBLE);
    ++index;
    if (index == 10) {
      index = 0;
    }
  }
}

BENCHMARK(BM_OrthogonalPackingInfeasibilityDetector)
    ->Arg(5)
    ->Arg(10)
    ->Arg(20)
    ->Arg(30)
    ->Arg(40)
    ->Arg(80)
    ->Arg(100)
    ->Arg(200)
    ->Arg(1000)
    ->Arg(10000);

}  // namespace
}  // namespace sat
}  // namespace operations_research
