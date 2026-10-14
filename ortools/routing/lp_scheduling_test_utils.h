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

#ifndef ORTOOLS_ROUTING_LP_SCHEDULING_TEST_UTILS_H_
#define ORTOOLS_ROUTING_LP_SCHEDULING_TEST_UTILS_H_

#include <cstdint>
#include <vector>

#include "ortools/routing/routing.h"
#include "ortools/util/random_engine.h"

namespace operations_research::routing {

// Creates and returns an increasing sequence of 'num_points' integers randomly
// distributed in the range [0, x_max], where the first element is 0 and the
// last element equal to x_max.
std::vector<int64_t> RandomDistribution(int64_t x_max, int num_points,
                                        random_engine_t& random);

// Populates the travel_info's transition info based on the given compression
// costs and travel durations.
// 'num_points_lin_cost' represents the (max) number of points used to model
// the linear compression cost for each transition.
void PopulateTravelInfoCompressionCost(
    Model::RouteDimensionTravelInfo& travel_info,
    double lower_bound_compressed_travel_factor,
    double upper_bound_travel_factor, double potential_cost,
    std::vector<int64_t> static_travel_durations, int num_points_lin_cost = 30);

}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_LP_SCHEDULING_TEST_UTILS_H_
