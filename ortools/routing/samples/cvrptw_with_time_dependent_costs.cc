// Copyright 2010-2024 Google LLC
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

// This example is very similar to cvrptw.cc, but distances are time dependent.
// The function RandomStepFunction is used to add random noise to each transit.

#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/random/random.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/parsers/cvrptw_lib.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"
#include "ortools/util/step_function.h"

using operations_research::Assignment;
using operations_research::StepFunction;
using operations_research::routing::DefaultRoutingSearchParameters;
using operations_research::routing::GetSeed;
using operations_research::routing::LocationContainer;
using operations_research::routing::RandomDemand;
using operations_research::routing::RoutingDimension;
using operations_research::routing::RoutingIndexManager;
using operations_research::routing::RoutingModel;
using operations_research::routing::RoutingNodeIndex;
using operations_research::routing::RoutingSearchParameters;
using operations_research::routing::ServiceTimePlusTransition;

ABSL_FLAG(int, vrp_orders, 25, "Nodes in the problem.");
ABSL_FLAG(int, vrp_vehicles, 10,
          "Size of Traveling Salesman Problem instance.");
ABSL_FLAG(bool, vrp_use_deterministic_random_seed, false,
          "Use deterministic random seeds.");
ABSL_FLAG(std::string, routing_search_parameters, "",
          "Text proto RoutingSearchParameters (possibly partial) that will "
          "override the DefaultRoutingSearchParameters()");

static const char kTime[] = "Time";
static const char kCapacity[] = "Capacity";
static const char kTimeDependentCost[] = "TimeDependentCost";

// This class implements the Pólya urn stochastic process, for more information:
// https://en.wikipedia.org/wiki/P%C3%B3lya_urn_model
// Basically, the polya urn is a martingale that converges almost surely to a
// uniform random variable over [0, 1]. It is questionable if it's realistic to
// model traffic deviations with this process, but traffic is hard to model in
// general.
class PolyaUrn {
 public:
  PolyaUrn(int red_balls, int blue_balls, int seed)
      : red_balls_(red_balls),
        all_balls_(red_balls + blue_balls),
        generator_(seed) {
    CHECK_LT(0, red_balls_);
    CHECK_LT(red_balls_, all_balls_);
  }
  // Every call to Next moves the process one step forward and returns the
  // current value.
  double Next() {
    CHECK_LT(0, red_balls_);
    CHECK_LT(red_balls_, all_balls_);

    const double return_value = static_cast<double>(red_balls_) / all_balls_;
    red_balls_ += (absl::Uniform(generator_, 0, all_balls_) < red_balls_);
    all_balls_ += 1;

    CHECK_LT(0, return_value);
    CHECK_LT(return_value, 1);
    return return_value - 0.5;
  }

 private:
  int red_balls_;
  int all_balls_;
  std::mt19937 generator_;
};

// Creates a random histogram over the interval [0, interval_end) using the urn.
StepFunction RandomStepFunction(int64_t mean, int64_t step_size,
                                int64_t interval_end, int seed) {
  PolyaUrn random_generator(1, 1, seed);
  StepFunction result;
  for (int64_t step = 0; step < interval_end; step += step_size) {
    result.AddStepToEnd(step, 2 * mean * random_generator.Next() - mean);
  }
  result.AddStepToEnd(interval_end, 0);
  return result;
}

class TrafficTransitionEvaluator {
 public:
  TrafficTransitionEvaluator(const LocationContainer& distance_evaluator,
                             int64_t max_time)
      : distance_evaluator_(distance_evaluator), max_time_(max_time) {}

  RoutingModel::StateDependentTransit Run(const RoutingIndexManager& manager,
                                          int64_t from_index,
                                          int64_t to_index) {
    const RoutingIndexManager::NodeIndex from = manager.IndexToNode(from_index);
    const RoutingIndexManager::NodeIndex to = manager.IndexToNode(to_index);
    static const int magic_number = 0xfe3498aa;
    const int64_t seed =
        (from.value() ^ magic_number) * (to.value() ^ (~magic_number));
    const int64_t distance = distance_evaluator_.ManhattanDistance(from, to);
    const int64_t mean_deviation = sqrt(distance);
    const StepFunction deviation =
        RandomStepFunction(mean_deviation, sqrt(max_time_), max_time_, seed);
    const std::function<int64_t(int64_t)> travel_time =
        [distance, &deviation](int64_t time) -> int64_t {
      return distance + deviation.GetValue(time);
    };
    return RoutingModel::MakeStateDependentTransit(travel_time, 0, max_time_);
    // Now the local variables deviation and travel_time are going to be
    // destroyed, but MakeStateDependentTransit does not store either and it
    // uses its own caches.
  }

 private:
  const LocationContainer& distance_evaluator_;
  const int64_t max_time_;
};

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  CHECK_LT(0, absl::GetFlag(FLAGS_vrp_orders))
      << "Specify an instance size greater than 0.";
  CHECK_LT(0, absl::GetFlag(FLAGS_vrp_vehicles))
      << "Specify a non-null vehicle fleet size.";
  // VRP of size absl::GetFlag(FLAGS_vrp_size).
  // Nodes are indexed from 0 to absl::GetFlag(FLAGS_vrp_orders), the starts and
  // ends of the routes are at node 0.
  static const RoutingIndexManager::NodeIndex kDepot(0);
  static const RoutingIndexManager::NodeIndex kFirstNodeAfterDepot(1);
  RoutingIndexManager manager(absl::GetFlag(FLAGS_vrp_orders) + 1,
                              absl::GetFlag(FLAGS_vrp_vehicles), kDepot);
  RoutingModel routing(manager);

  // Setting up locations.
  const int64_t kXMax = 1000;
  const int64_t kYMax = 1000;
  const int64_t kSpeed = 10;
  LocationContainer locations(
      kSpeed, absl::GetFlag(FLAGS_vrp_use_deterministic_random_seed));
  for (int location = 0; location <= absl::GetFlag(FLAGS_vrp_orders);
       ++location) {
    locations.AddRandomLocation(kXMax, kYMax);
  }

  // Adding capacity dimension constraints.
  const int64_t kVehicleCapacity = 40;
  const int64_t kNullCapacitySlack = 0;
  RandomDemand demand(manager.num_nodes(), kDepot,
                      absl::GetFlag(FLAGS_vrp_use_deterministic_random_seed));
  demand.Initialize();
  routing.AddDimension(routing.RegisterTransitCallback(
                           [&demand, &manager](int64_t i, int64_t j) {
                             return demand.Demand(manager.IndexToNode(i),
                                                  manager.IndexToNode(j));
                           }),
                       kNullCapacitySlack, kVehicleCapacity,
                       /*fix_start_cumul_to_zero=*/true, kCapacity);

  // Adding time dimension constraints.
  const int64_t kTimePerDemandUnit = 3;
  const int64_t kHorizon = 24 * 36;
  ServiceTimePlusTransition time(
      kTimePerDemandUnit,
      [&demand](RoutingNodeIndex i, RoutingNodeIndex j) {
        return demand.Demand(i, j);
      },
      [&locations](RoutingNodeIndex i, RoutingNodeIndex j) {
        return locations.ManhattanTime(i, j);
      });
  routing.AddDimension(
      routing.RegisterTransitCallback([&time, &manager](int64_t i, int64_t j) {
        return time.Compute(manager.IndexToNode(i), manager.IndexToNode(j));
      }),
      kHorizon, kHorizon, /*fix_start_cumul_to_zero=*/true, kTime);

  // Setting the cost function. In fact, we create a time dependent dimension.
  const int64_t max_time = manager.num_nodes() * (kXMax + kYMax) / kSpeed;
  TrafficTransitionEvaluator traffic_evaluator(locations, max_time);
  routing.AddDimensionDependentDimensionWithVehicleCapacity(
      routing.RegisterStateDependentTransitCallback(::absl::bind_front(
          &TrafficTransitionEvaluator::Run, &traffic_evaluator, manager)),
      &routing.GetDimensionOrDie(kTime), kHorizon, kHorizon,
      /*fix_start_cumul_to_zero=*/true, kTimeDependentCost);
  routing.GetMutableDimension(kTimeDependentCost)
      ->SetSpanCostCoefficientForAllVehicles(1);

  // Adding time windows.
  std::mt19937 randomizer(
      GetSeed(absl::GetFlag(FLAGS_vrp_use_deterministic_random_seed)));
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie(kTime);
  const int64_t kTWDuration = 5 * 36;
  for (int order = 1; order < manager.num_nodes(); ++order) {
    const int64_t start =
        absl::Uniform<int32_t>(randomizer, 0, kHorizon - kTWDuration);
    time_dimension.CumulVar(order)->SetRange(start, start + kTWDuration);
  }

  // Adding penalty costs to allow skipping orders.
  const int64_t kPenalty = 10000000;
  for (RoutingIndexManager::NodeIndex order = kFirstNodeAfterDepot;
       order < routing.nodes(); ++order) {
    std::vector<int64_t> orders(1, manager.NodeToIndex(order));
    routing.AddDisjunction(orders, kPenalty);
  }

  // Solve, returns a solution if any (owned by RoutingModel).
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::GetFlag(FLAGS_routing_search_parameters), &parameters));
  const Assignment* solution = routing.SolveWithParameters(parameters);
  if (solution != nullptr) {
    DisplayPlan(manager, routing, *solution, /*use_same_vehicle_costs=*/false,
                /*max_nodes_per_group=*/0, /*same_vehicle_cost=*/0,
                {kCapacity, kTime, kTimeDependentCost});
  } else {
    LOG(INFO) << "No solution found.";
  }
  return 0;
}
