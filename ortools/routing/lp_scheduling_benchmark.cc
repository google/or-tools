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
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "google/protobuf/duration.pb.h"
#include "ortools/base/types.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/lp_scheduling.h"
#include "ortools/routing/lp_scheduling_test_utils.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"
#include "ortools/util/piecewise_linear_function_test_utils.h"
#include "ortools/util/random_engine.h"

namespace operations_research::routing {
namespace {

constexpr double kLowerBoundCompressedTravelFactor = 0.7;
constexpr double kUpperBoundTravelFactor = 3.0;
constexpr int64_t kCostRef = 1 << 12;

void BuildTestModel(int kNodes, int64_t cumul_max, int64_t span_cost,
                    int64_t slack_cost, int time_limit_seconds,
                    std::unique_ptr<Model>& model,
                    std::unique_ptr<LocalDimensionCumulOptimizer>& optimizer) {
  const int kVehicles = 1;
  const IndexManager manager(kNodes, kVehicles, NodeIndex(0));

  model = std::make_unique<Model>(manager);
  // Add time dimension.
  const std::string kDimName = "time";
  model->AddConstantDimensionWithSlack(
      1, /*capacity*/ 10'000, /*slack_max*/ 10'000,
      /*fix_start_cumul_to_zero*/ false, kDimName);
  Dimension* const time = model->GetMutableDimension(kDimName);
  time->SetSpanCostCoefficientForVehicle(span_cost, 0);
  time->SetSlackCostCoefficientForVehicle(slack_cost, 0);
  time->CumulVar(kNodes)->SetMax(cumul_max);

  // HACK(user): This permits to set a time limit.
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.mutable_time_limit()->set_seconds(time_limit_seconds);
  model->SolveFromAssignmentWithParameters(nullptr, parameters, nullptr);

  optimizer = std::make_unique<LocalDimensionCumulOptimizer>(
      time, RoutingSearchParameters::SCHEDULING_CP_SAT, nullptr);
}

DimensionSchedulingStatus ComputeRouteCumuls(
    std::unique_ptr<LocalDimensionCumulOptimizer>& optimizer,
    Model::RouteDimensionTravelInfo& travel_info,
    std::vector<int64_t>* optimal_cumuls,
    std::vector<int64_t>* optimal_breaks = nullptr,
    int64_t* optimal_cost_without_fixed_transits = nullptr) {
  // Builds the next_accessor such as i -> i+1
  const int kNodes = travel_info.transition_info.size();
  std::vector<int64_t> nexts;
  for (int node = 0; node < kNodes; ++node) nexts.push_back(1 + node);
  const auto next_accessor = [&nexts](int index) { return nexts[index]; };

  return optimizer->ComputeRouteCumulsAndCostWithoutFixedTransits(
      /*vehicle=*/0, /*solve_duration_ratio=*/1.0, next_accessor, &travel_info,
      optimal_cumuls, optimal_breaks, optimal_cost_without_fixed_transits);
}

std::vector<int64_t> FillRandomTrafficInfo(
    random_engine_t random, int kNodes, int64_t dT, int nb_segments,
    Model::RouteDimensionTravelInfo& travel_info,
    double compression_factor = 0.8, double play = 0.25,
    int n_points_lin_cost = 30) {
  const std::vector<int64_t> initial_cumuls =
      RandomDistribution(dT, 1 + kNodes, random);
  const std::vector<int64_t> new_cumuls = RandomDistribution(
      std::ceil(dT / compression_factor), 1 + kNodes, random);
  std::mt19937 eng(absl::Uniform(random, kint32min, kint32max));
  travel_info.transition_info.resize(kNodes);

  std::vector<int64_t> static_travel_durations(kNodes);
  for (int k = 0; k < kNodes; ++k) {
    Model::RouteDimensionTravelInfo::TransitionInfo transition;
    const int64_t initial_dt = initial_cumuls[k + 1] - initial_cumuls[k];
    const int64_t expanded_dt = new_cumuls[k + 1] - new_cumuls[k];
    const int64_t ref_dt = std::min(initial_dt, expanded_dt);

    // Pre travel is a binomial variable of mean kPreFactor * initial_dt, such
    // that it is in [0, initial_dt * (1 - kPostFactor - kMinTravelFactor)].
    constexpr double kPreFactor = 0.15;
    constexpr double kPostFactor = 0.15;
    constexpr double kMinTravelFactor = 0.4;

    // Pre travel
    const double mult_pre = 1 - kPostFactor - kMinTravelFactor;
    std::binomial_distribution<int64_t> binomial_pre(ref_dt * mult_pre,
                                                     kPreFactor / mult_pre);
    transition.pre_travel_transit_value = binomial_pre(eng);

    // Post travel
    const double mult_post = 1 - kMinTravelFactor;
    const int64_t n = ref_dt * mult_post - transition.pre_travel_transit_value;
    std::binomial_distribution<int64_t> binomial_post(
        n, kPreFactor * ref_dt / static_cast<double>(n));
    transition.post_travel_transit_value = binomial_post(eng);

    // Travel
    const int64_t static_travel = initial_dt -
                                  transition.pre_travel_transit_value -
                                  transition.post_travel_transit_value;
    static_travel_durations[k] = static_travel;
    const int64_t avg_traffic_travel = expanded_dt -
                                       transition.pre_travel_transit_value -
                                       transition.post_travel_transit_value;
    const int64_t min_traffic_travel =
        std::max((1 - play) * avg_traffic_travel,
                 kLowerBoundCompressedTravelFactor * static_travel);
    const int64_t max_traffic_travel =
        std::max<int64_t>(1 + min_traffic_travel,
                          std::min((1 + play) * avg_traffic_travel,
                                   kUpperBoundTravelFactor * static_travel));
    transition.travel_start_dependent_travel =
        RandomPiecewiseLinearFunctionFixedNbSegments(
            0, dT, min_traffic_travel, max_traffic_travel, nb_segments, random);

    travel_info.transition_info[k] = std::move(transition);
  }
  PopulateTravelInfoCompressionCost(
      travel_info, kLowerBoundCompressedTravelFactor, kUpperBoundTravelFactor,
      kCostRef, static_travel_durations, n_points_lin_cost);
  return static_travel_durations;
}

void BM_TrafficScheduling(benchmark::State& state) {
  // Parameters:
  const int kNodes = state.range(0);
  const int kSegments = state.range(1);
  const int kSpanCost = state.range(2);
  const int kTravelPerHourCost = state.range(3);

  random_engine_t random;
  for (auto s : state) {
    Model::RouteDimensionTravelInfo travel_info;
    const int64_t dT = absl::Uniform(random, 20 * kNodes, 100 * kNodes);
    travel_info.travel_cost_coefficient = kTravelPerHourCost;
    const std::vector<int64_t> static_travel_durations =
        FillRandomTrafficInfo(random, kNodes, dT, kSegments, travel_info,
                              /*compression_factor=*/0.8, /*play=*/0.2);

    // Find an optimal scheduling.
    std::unique_ptr<Model> model;
    std::unique_ptr<LocalDimensionCumulOptimizer> optimizer;
    BuildTestModel(kNodes, /*cumul_max=*/dT, kSpanCost, kSpanCost,
                   /*time_limit_seconds=*/30, model, optimizer);
    std::vector<int64_t> optimal_cumuls;
    CHECK_EQ(ComputeRouteCumuls(optimizer, travel_info, &optimal_cumuls),
             DimensionSchedulingStatus::OPTIMAL);
  }
}
BENCHMARK(BM_TrafficScheduling)
    ->Args({20, 3, 0, 10})
    ->Args({20, 4, 0, 10})
    ->Args({20, 6, 0, 10});

}  // namespace
}  // namespace operations_research::routing
