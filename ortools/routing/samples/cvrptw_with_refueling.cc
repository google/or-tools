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

// Capacitated Vehicle Routing Problem with Time Windows and refueling
// constraints.
// This is an extension to the model in cvrptw.cc so refer to that file for
// more information on the common part of the model. The model implemented here
// takes into account refueling constraints using a specific dimension: vehicles
// must visit certain nodes (refueling nodes) before the quantity of fuel
// reaches zero. Fuel consumption is proportional to the distance traveled.

#include <cstdint>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
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

using operations_research::Assignment;
using operations_research::Solver;
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

ABSL_FLAG(int, vrp_orders, 20, "Nodes in the problem.");
ABSL_FLAG(int, vrp_vehicles, 4,
          "Size of the Vehicle Routing Problem instance.");
ABSL_FLAG(bool, vrp_use_deterministic_random_seed, false,
          "Use deterministic random seeds.");
ABSL_FLAG(std::string, routing_search_parameters, "",
          "Text proto RoutingSearchParameters (possibly partial) that will "
          "override the DefaultRoutingSearchParameters()");

const char* kTime = "Time";
const char* kCapacity = "Capacity";
const char* kFuel = "Fuel";

// Returns true if node is a refueling node (based on node / refuel node ratio).
bool IsRefuelNode(int64_t node) {
  const int64_t kRefuelNodeRatio = 10;
  return (node % kRefuelNodeRatio == 0);
}

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  CHECK_LT(0, absl::GetFlag(FLAGS_vrp_orders))
      << "Specify an instance size greater than 0.";
  CHECK_LT(0, absl::GetFlag(FLAGS_vrp_vehicles))
      << "Specify a non-null vehicle fleet size.";
  // VRP of size absl::GetFlag(FLAGS_vrp_size).
  // Nodes are indexed from 0 to absl::GetFlag(FLAGS_vrp_orders), the starts and
  // ends of the routes are at node 0.
  const RoutingIndexManager::NodeIndex kDepot(0);
  RoutingIndexManager manager(absl::GetFlag(FLAGS_vrp_orders) + 1,
                              absl::GetFlag(FLAGS_vrp_vehicles), kDepot);
  RoutingModel routing(manager);

  // Setting up locations.
  const int64_t kXMax = 100000;
  const int64_t kYMax = 100000;
  const int64_t kSpeed = 10;
  const int64_t kRefuelCost = 10;
  LocationContainer locations(
      kSpeed, absl::GetFlag(FLAGS_vrp_use_deterministic_random_seed));
  for (int location = 0; location <= absl::GetFlag(FLAGS_vrp_orders);
       ++location) {
    locations.AddRandomLocation(kXMax, kYMax);
  }

  // Setting the cost function.
  const int vehicle_cost = routing.RegisterTransitCallback(
      [&locations, &manager](int64_t i, int64_t j) {
        return locations.ManhattanDistance(manager.IndexToNode(i),
                                           manager.IndexToNode(j)) +
               (IsRefuelNode(i) ? kRefuelCost : 0);
      });
  routing.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

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
  const int64_t kTimePerDemandUnit = 300;
  const int64_t kHorizon = 24 * 3600;
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
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie(kTime);
  // Adding time windows.
  // NOTE(user): This randomized test case is quite sensible to the seed:
  // the generated model can be much easier or harder to solve, depending on
  // the seed. It turns out that most seeds yield pretty slow/bad solver
  // performance: I got good performance for about 10% of the seeds.
  std::mt19937 randomizer(
      144 + GetSeed(absl::GetFlag(FLAGS_vrp_use_deterministic_random_seed)));
  const int64_t kTWDuration = 5 * 3600;
  for (int order = 1; order < manager.num_nodes(); ++order) {
    if (!IsRefuelNode(order)) {
      const int64_t start =
          absl::Uniform<int32_t>(randomizer, 0, kHorizon - kTWDuration);
      time_dimension.CumulVar(order)->SetRange(start, start + kTWDuration);
    }
  }

  // Adding fuel dimension. This dimension consumes a quantity equal to the
  // distance traveled. Only refuel nodes can make the quantity of dimension
  // increase by letting slack variable replenish the fuel.
  const int64_t kFuelCapacity = kXMax + kYMax;
  routing.AddDimension(
      routing.RegisterTransitCallback(
          [&locations, &manager](int64_t i, int64_t j) {
            return locations.NegManhattanDistance(manager.IndexToNode(i),
                                                  manager.IndexToNode(j));
          }),
      kFuelCapacity, kFuelCapacity, /*fix_start_cumul_to_zero=*/false, kFuel);
  const RoutingDimension& fuel_dimension = routing.GetDimensionOrDie(kFuel);
  for (int order = 0; order < routing.Size(); ++order) {
    // Only let slack free for refueling nodes.
    if (!IsRefuelNode(order) || routing.IsStart(order)) {
      fuel_dimension.SlackVar(order)->SetValue(0);
    } else {
      // Ensure that we do not refuel more than the capacity.
      Solver* solver = routing.solver();
      solver->AddConstraint(solver->MakeSumLessOrEqual(
          {fuel_dimension.SlackVar(order), fuel_dimension.CumulVar(order)},
          kFuelCapacity));
      routing.AddToAssignment(fuel_dimension.SlackVar(order));
    }
    // Needed to instantiate fuel quantity at each node. Deciding to refuel as
    // much as possible to minimize the risk of running out of fuel.
    routing.AddVariableMaximizedByFinalizer(fuel_dimension.CumulVar(order));
  }
  for (int vehicle = 0; vehicle < routing.vehicles(); ++vehicle) {
    routing.AddVariableMaximizedByFinalizer(
        fuel_dimension.CumulVar(routing.End(vehicle)));
  }

  // Adding penalty costs to allow skipping orders.
  const int64_t kPenalty = 100000;
  const RoutingIndexManager::NodeIndex kFirstNodeAfterDepot(1);
  for (RoutingIndexManager::NodeIndex order = kFirstNodeAfterDepot;
       order < routing.nodes(); ++order) {
    std::vector<int64_t> orders(1, manager.NodeToIndex(order));
    routing.AddDisjunction(
        orders, IsRefuelNode(manager.NodeToIndex(order)) ? 0 : kPenalty);
  }

  // Solve, returns a solution if any (owned by RoutingModel).
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::GetFlag(FLAGS_routing_search_parameters), &parameters));
  const Assignment* solution = routing.SolveWithParameters(parameters);
  if (solution != nullptr) {
    DisplayPlan(manager, routing, *solution, /*use_same_vehicle_costs=*/false,
                /*max_nodes_per_group=*/0, /*same_vehicle_cost=*/0,
                {kTime, kCapacity, kFuel});
  } else {
    LOG(INFO) << "No solution found.";
  }
  return EXIT_SUCCESS;
}
