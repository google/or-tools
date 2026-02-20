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

// Capacitated Vehicle Routing Problem with Disjoint Time Windows (and optional
// orders).
// A description of the problem can be found here:
// http://en.wikipedia.org/wiki/Vehicle_routing_problem.
// The variant which is tackled by this model includes a capacity dimension,
// disjoint time windows and optional orders, with a penalty cost if orders are
// not performed. For the sake of simplicity, orders are randomly located and
// distances are computed using the Manhattan distance. Distances are assumed
// to be in meters and times in seconds.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/random/random.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/init_google.h"
#include "ortools/base/log_severity.h"
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
using operations_research::routing::Dimension;
using operations_research::routing::GetSeed;
using operations_research::routing::IndexManager;
using operations_research::routing::LocationContainer;
using operations_research::routing::Model;
using operations_research::routing::NodeIndex;
using operations_research::routing::RandomDemand;
using operations_research::routing::RoutingSearchParameters;
using operations_research::routing::ServiceTimePlusTransition;

ABSL_FLAG(int, vrp_orders, 100, "Number of nodes in the problem.");
ABSL_FLAG(int, vrp_vehicles, 20, "Number of vehicles in the problem.");
ABSL_FLAG(int, vrp_windows, 5, "Number of disjoint windows per node.");
ABSL_FLAG(bool, vrp_use_deterministic_random_seed, false,
          "Use deterministic random seeds.");
ABSL_FLAG(bool, vrp_use_same_vehicle_costs, false,
          "Use same vehicle costs in the routing model");
ABSL_FLAG(std::string, routing_search_parameters, "",
          "Text proto RoutingSearchParameters (possibly partial) that will "
          "override the DefaultRoutingSearchParameters()");

const char* kTime = "Time";
const char* kCapacity = "Capacity";
const int64_t kMaxNodesPerGroup = 10;
const int64_t kSameVehicleCost = 1000;

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  CHECK_LT(0, absl::GetFlag(FLAGS_vrp_orders))
      << "Specify an instance size greater than 0.";
  CHECK_LT(0, absl::GetFlag(FLAGS_vrp_vehicles))
      << "Specify a non-null vehicle fleet size.";
  // VRP of size absl::GetFlag(FLAGS_vrp_size).
  // Nodes are indexed from 0 to absl::GetFlag(FLAGS_vrp_orders), the starts and
  // ends of the routes are at node 0.
  const NodeIndex kDepot(0);
  IndexManager manager(absl::GetFlag(FLAGS_vrp_orders) + 1,
                       absl::GetFlag(FLAGS_vrp_vehicles), kDepot);
  Model routing(manager);

  // Setting up locations.
  const int64_t kXMax = 100000;
  const int64_t kYMax = 100000;
  const int64_t kSpeed = 10;
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
                                           manager.IndexToNode(j));
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
      [&demand](NodeIndex i, NodeIndex j) { return demand.Demand(i, j); },
      [&locations](NodeIndex i, NodeIndex j) {
        return locations.ManhattanTime(i, j);
      });
  routing.AddDimension(
      routing.RegisterTransitCallback([&time, &manager](int64_t i, int64_t j) {
        return time.Compute(manager.IndexToNode(i), manager.IndexToNode(j));
      }),
      kHorizon, kHorizon, /*fix_start_cumul_to_zero=*/false, kTime);
  const Dimension& time_dimension = routing.GetDimensionOrDie(kTime);

  // Adding disjoint time windows.
  Solver* solver = routing.solver();
  std::mt19937 randomizer(
      GetSeed(absl::GetFlag(FLAGS_vrp_use_deterministic_random_seed)));
  for (int order = 1; order < manager.num_nodes(); ++order) {
    std::vector<int64_t> forbid_points(2 * absl::GetFlag(FLAGS_vrp_windows), 0);
    for (int i = 0; i < forbid_points.size(); ++i) {
      forbid_points[i] = absl::Uniform<int32_t>(randomizer, 0, kHorizon);
    }
    std::sort(forbid_points.begin(), forbid_points.end());
    std::vector<int64_t> forbid_starts(1, 0);
    std::vector<int64_t> forbid_ends;
    for (int i = 0; i < forbid_points.size(); i += 2) {
      forbid_ends.push_back(forbid_points[i]);
      forbid_starts.push_back(forbid_points[i + 1]);
    }
    forbid_ends.push_back(kHorizon);
    solver->AddConstraint(solver->MakeNotMemberCt(
        time_dimension.CumulVar(order), forbid_starts, forbid_ends));
  }

  // Adding penalty costs to allow skipping orders.
  const int64_t kPenalty = 10000000;
  const NodeIndex kFirstNodeAfterDepot(1);
  for (NodeIndex order = kFirstNodeAfterDepot; order < manager.num_nodes();
       ++order) {
    std::vector<int64_t> orders(1, manager.NodeToIndex(order));
    routing.AddDisjunction(orders, kPenalty);
  }

  // Adding same vehicle constraint costs for consecutive nodes.
  if (absl::GetFlag(FLAGS_vrp_use_same_vehicle_costs)) {
    std::vector<int64_t> group;
    for (NodeIndex order = kFirstNodeAfterDepot; order < manager.num_nodes();
         ++order) {
      group.push_back(manager.NodeToIndex(order));
      if (group.size() == kMaxNodesPerGroup) {
        routing.AddSoftSameVehicleConstraint(std::move(group),
                                             kSameVehicleCost);
        group.clear();
      }
    }
    if (!group.empty()) {
      routing.AddSoftSameVehicleConstraint(std::move(group), kSameVehicleCost);
    }
  }

  // Solve, returns a solution if any (owned by Model).
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::GetFlag(FLAGS_routing_search_parameters), &parameters));
  const Assignment* solution = routing.SolveWithParameters(parameters);
  if (solution != nullptr) {
    operations_research::routing::DisplayPlan(
        manager, routing, *solution,
        absl::GetFlag(FLAGS_vrp_use_same_vehicle_costs), kMaxNodesPerGroup,
        kSameVehicleCost, {kCapacity, kTime});
  } else {
    LOG(INFO) << "No solution found.";
  }
  return EXIT_SUCCESS;
}
