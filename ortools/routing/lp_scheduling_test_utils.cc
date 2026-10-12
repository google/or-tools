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

#include "ortools/routing/lp_scheduling_test_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "ortools/base/types.h"
#include "ortools/routing/routing.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/random_engine.h"

namespace operations_research::routing {

std::vector<int64_t> RandomDistribution(int64_t x_max, int num_points,
                                        random_engine_t& random) {
  DCHECK_GE(num_points, 2);
  DCHECK_LE(num_points, x_max + 1);
  std::vector<int64_t> points(num_points);
  points.back() = x_max;

  // The length of each interval L, is a random variable defined by:
  // L = 1 + B(x_left - k_left, p)
  // with x_left the remaining space left to cover in the interval.
  // and k_left the number of segments Left
  // p = (x_left / k_left - 1) / (x_left - k_left)
  // This ensures that E(L) = x_left / k_left with L >= 1 and L <= x_left -
  // k_left + 1
  std::mt19937 eng(absl::Uniform(random, kint32min, kint32max));
  int x_last = 0;
  for (int i = 1; i < num_points - 1; ++i) {
    const int64_t x_left = x_max - x_last;
    const int64_t k_left = num_points - i;
    const double p =
        x_left == k_left
            ? 0
            : (x_left / static_cast<double>(k_left) - 1) / (x_left - k_left);
    std::binomial_distribution<int64_t> binomial(x_left - k_left, p);
    const int64_t dx = 1 + binomial(eng);
    x_last += dx;
    points[i] = x_last;
  }
  DCHECK_EQ(points.back(), x_max);
  return points;
}

namespace {
using TransitionInfo = Model::RouteDimensionTravelInfo::TransitionInfo;

std::vector<int64_t> GetAverageTravelDurations(
    Model::RouteDimensionTravelInfo& travel_info) {
  const std::vector<TransitionInfo>& transition_info =
      travel_info.transition_info;
  std::vector<int64_t> avg_travel_durations(transition_info.size());

  for (int i = 0; i < transition_info.size(); ++i) {
    // Gets the average travel duration.
    int64_t sum_avg = 0;
    const FloatSlopePiecewiseLinearFunction& pwl =
        transition_info[i].travel_start_dependent_travel;
    const auto& x_anchors = pwl.x_anchors();
    const auto& y_anchors = pwl.y_anchors();
    DCHECK_EQ(x_anchors.size(), y_anchors.size());
    const int n_points = x_anchors.size();
    DCHECK_GE(n_points, 2);
    DCHECK_LT(x_anchors.front(), x_anchors.back());
    for (int j = 0; j < n_points - 1; ++j) {
      sum_avg += (x_anchors[j + 1] - x_anchors[j]) *
                 (y_anchors[j] + y_anchors[j + 1]) / 2;
    }
    const int64_t avg_travel_duration =
        sum_avg / (x_anchors.back() - x_anchors.front());
    avg_travel_durations[i] = avg_travel_duration;
  }
  return avg_travel_durations;
}
}  // namespace

void PopulateTravelInfoCompressionCost(
    Model::RouteDimensionTravelInfo& travel_info,
    double lower_bound_compressed_travel_factor,
    double upper_bound_travel_factor, double potential_cost,
    std::vector<int64_t> static_travel_durations, int num_points_lin_cost) {
  DCHECK_GE(lower_bound_compressed_travel_factor, 0);
  DCHECK_GE(upper_bound_travel_factor, 0);
  DCHECK_GE(potential_cost, 0);
  DCHECK_GE(num_points_lin_cost, 2);
  std::vector<TransitionInfo>& transition_info = travel_info.transition_info;
  if (static_travel_durations.size() < transition_info.size()) {
    static_travel_durations = GetAverageTravelDurations(travel_info);
  }

  for (int i = 0; i < transition_info.size(); ++i) {
    auto& transition = transition_info[i];
    const int64_t static_travel_duration = static_travel_durations[i];
    const int64_t upper_bound_travel =
        upper_bound_travel_factor * static_travel_duration;
    const int64_t lower_bound_compressed_travel =
        lower_bound_compressed_travel_factor * static_travel_duration;
    transition.compressed_travel_value_lower_bound =
        lower_bound_compressed_travel;
    transition.travel_value_upper_bound = upper_bound_travel;

    // Creates the compression error.
    // The intervals are uniform, this is just a demo.
    const int64_t compression_breakpoint = static_travel_duration;
    DCHECK_GT(compression_breakpoint, 0);
    const int actual_n_points_lin_cost =
        std::min<int64_t>(compression_breakpoint + 1, num_points_lin_cost);
    absl::InlinedVector<int64_t, 8> cost_x_anchors;
    absl::InlinedVector<int64_t, 8> cost_y_anchors;
    cost_x_anchors.reserve(actual_n_points_lin_cost + 1);
    cost_y_anchors.reserve(actual_n_points_lin_cost + 1);
    const double denom_y = std::pow(compression_breakpoint, 2.0);
    DCHECK_GT(actual_n_points_lin_cost, 1);
    for (int k = 0; k < actual_n_points_lin_cost; ++k) {
      const double x = k / static_cast<double>(actual_n_points_lin_cost - 1);
      const int64_t x_scaled = std::round(x * compression_breakpoint);
      cost_x_anchors.push_back(x_scaled);
      cost_y_anchors.push_back(std::round(static_travel_duration *
                                          potential_cost * x_scaled * x_scaled /
                                          denom_y));
    }

    // Adds an additional segment, just in case.
    cost_x_anchors.push_back(upper_bound_travel);
    cost_y_anchors.push_back(
        static_travel_duration * potential_cost *
        std::pow(
            upper_bound_travel / static_cast<double>(compression_breakpoint),
            2.0));

    transition.travel_compression_cost = FloatSlopePiecewiseLinearFunction(
        std::move(cost_x_anchors), std::move(cost_y_anchors));
  }
}

}  // namespace operations_research::routing
