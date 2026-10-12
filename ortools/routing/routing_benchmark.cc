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
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/base/types.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"

namespace operations_research::routing {
namespace {

// Benchmarks for GetArcCost* methods, which are on the critical path during an
// optimization. Since they're called via a callback and thus can't be inlined
// in the solver code, we do the same here.

int64_t MatrixLookupWithCoefficient(const IndexManager& manager,
                                    absl::Span<const int64_t> matrix,
                                    int row_size, int64_t coefficient,
                                    int64_t i, int64_t j) {
  return coefficient * matrix[manager.IndexToNode(i).value() *
                                  static_cast<int64_t>(row_size) +
                              manager.IndexToNode(j).value()];
}

void BM_GetArcCostForVehicle(benchmark::State& state) {
  const int generate_many_cost_cache_hits = state.range(0);
  const int with_vehicle_costs = state.range(1);
  const int use_vehicle_class_cost_callback = state.range(2);
  const int num_dimensions = state.range(3);

  absl::BitGen random;
  const int kNumNodes = 1000;
  const int kNumVehicles = 100;
  const int kNumVehicleClasses = 10;
  std::vector<std::pair<NodeIndex, NodeIndex>> vehicle_start_and_end;
  for (int i = 0; i < kNumVehicles; ++i) {
    vehicle_start_and_end.push_back({NodeIndex(2 * i), NodeIndex(2 * i + 1)});
  }
  IndexManager manager(kNumNodes, kNumVehicles, vehicle_start_and_end);
  Model model(manager);
  std::vector<int64_t> cost_matrix;
  bool has_vehicle_with_no_cost_callback = true;
  std::set<int> callbacks_used;
  if (with_vehicle_costs) {
    cost_matrix.assign(kNumNodes * kNumNodes, -1);
    for (int i = 0; i < kNumNodes * kNumNodes; ++i) {
      cost_matrix[i] = absl::Uniform<int64_t>(random, 0, 1000);
    }
    std::vector<int> vehicle_costs;
    for (int v = 0; v < kNumVehicleClasses; ++v) {
      vehicle_costs.push_back(model.RegisterTransitCallback(
          absl::bind_front(MatrixLookupWithCoefficient, manager, cost_matrix,
                           /*row_size=*/kNumNodes,
                           /*coefficient=*/v)));
    }
    has_vehicle_with_no_cost_callback = false;
    for (int i = 0; i < kNumVehicles; ++i) {
      // Leave half the vehicles with no cost callback.
      if (absl::Bernoulli(random, 1.0 / 2)) {
        has_vehicle_with_no_cost_callback = true;
        continue;
      }
      const int index = absl::Uniform<int>(random, 0, kNumVehicleClasses);
      callbacks_used.insert(index);
      model.SetArcCostEvaluatorOfVehicle(vehicle_costs[index], i);
    }
  }
  std::vector<std::vector<int64_t>> dimension_cost_matrices(num_dimensions);
  for (int dim = 0; dim < num_dimensions; ++dim) {
    dimension_cost_matrices[dim].assign(kNumNodes * kNumNodes, -1);
    for (int i = 0; i < kNumNodes * kNumNodes; ++i) {
      dimension_cost_matrices[dim][i] = absl::Uniform<int64_t>(random, 0, 1000);
    }
    const std::string dimension_name = absl::StrCat("Dimension #", dim);
    const int callback = model.RegisterTransitCallback(absl::bind_front(
        MatrixLookupWithCoefficient, manager, dimension_cost_matrices[dim],
        /*row_size=*/kNumNodes,
        /*coefficient=*/1));
    model.AddDimension(callback,
                       /*slack_max=*/kint64max,
                       /*capacity=*/kint64max,
                       /*fix_start_cumul_to_zero=*/false,
                       /*name=*/dimension_name);
    model.GetMutableDimension(dimension_name)
        ->SetSpanCostCoefficientForAllVehicles(1.0);
  }
  model.CloseModel();
  CHECK_EQ(
      callbacks_used.size() +
          (num_dimensions > 0 && has_vehicle_with_no_cost_callback ? 1 : 0),
      model.GetNonZeroCostClassesCount());
  // Set up the list of arcs to look up. By default, make it very cache
  // friendly.
  int kNumRepetitions = 10;
  int kNumArcs = 100000;
  struct Arc {
    int from;
    int to;
    int vehicle_or_class;
  };
  std::vector<Arc> arcs(kNumArcs);
  for (int i = 0; i < kNumNodes;) {
    const int from = absl::Uniform<int>(random, 0, kNumNodes);
    int to = absl::Uniform<int>(random, 0, kNumNodes);
    // Artificially inflate the ratio of self-arcs to a fixed ratio.
    if (absl::Bernoulli(random, 1.0 / 100)) {
      to = from;
    } else if (from == to) {
      continue;
    }
    arcs[i] = {
        from,
        to,
        // Make the vehicle 'unknown' (i.e. -1) iff it's a self-arc.
        (from == to) ? -1
                     : absl::Uniform<int>(random, 0,
                                          use_vehicle_class_cost_callback
                                              ? kNumVehicleClasses
                                              : kNumVehicles),
    };
    for (int j = 1; j < kNumRepetitions; ++j) arcs[i + j] = arcs[i];
    i += kNumRepetitions;
  }
  if (!generate_many_cost_cache_hits) {
    std::shuffle(arcs.begin(), arcs.end(), random);
  }
  std::function<int64_t(int64_t, int64_t, int64_t)> cost_callback =
      use_vehicle_class_cost_callback
          ? absl::bind_front(&Model::GetArcCostForClass, &model)
          : absl::bind_front(&Model::GetArcCostForVehicle, &model);

  int64_t total_cost = 0;
  int i = 0;
  for (auto _ : state) {
    if (++i == kNumArcs) i = 0;  // We don't care that we start from 1.
    const Arc& arc = arcs[i];
    total_cost += cost_callback(arc.from, arc.to, arc.vehicle_or_class);
  }
}

BENCHMARK(BM_GetArcCostForVehicle)
    // Argument order:
    //   generate_many_cost_cache_hits
    //   with_vehicle_costs
    //   use_vehicle_class_cost_callback
    //   num_dimensions
    ->Args({1, 0, 0, 0})
    ->Args({1, 0, 0, 1})
    ->Args({1, 0, 0, 2})
    ->Args({0, 0, 0, 0})
    ->Args({0, 0, 0, 1})
    ->Args({0, 0, 0, 2})
    ->Args({1, 1, 0, 0})
    ->Args({1, 1, 0, 1})
    ->Args({1, 1, 0, 2})
    ->Args({0, 1, 0, 0})
    ->Args({0, 1, 0, 1})
    ->Args({0, 1, 0, 2})
    ->Args({1, 1, 1, 0})
    ->Args({1, 1, 1, 1})
    ->Args({1, 1, 1, 2})
    ->Args({0, 1, 1, 0})
    ->Args({0, 1, 1, 1})
    ->Args({0, 1, 1, 2});

}  // namespace
}  // namespace operations_research::routing
