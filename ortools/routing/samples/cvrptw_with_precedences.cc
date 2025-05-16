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

//
// Capacitated Vehicle Routing Problem with Time Windows (and optional orders).
// A description of the problem can be found here:
// http://en.wikipedia.org/wiki/Vehicle_routing_problem.
// The variant which is tackled by this model includes a capacity dimension,
// time windows and optional orders, with a penalty cost if orders are not
// performed. For the sake of simplicty, orders are randomly located and
// distances are computed using the Manhattan distance. Distances are assumed
// to be in meters and times in seconds.

#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/flags/flag.h"
#include "absl/log/globals.h"
#include "absl/random/random.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/graph/graph_builder.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/parsers/cvrptw_lib.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"

using operations_research::Assignment;
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

ABSL_FLAG(int, vrp_orders, 100, "Nodes in the problem.");
ABSL_FLAG(int, vrp_vehicles, 20,
          "Size of Traveling Salesman Problem instance.");
ABSL_FLAG(bool, vrp_use_deterministic_random_seed, false,
          "Use deterministic random seeds.");
ABSL_FLAG(bool, vrp_use_same_vehicle_costs, false,
          "Use same vehicle costs in the routing model");
ABSL_FLAG(std::string, routing_search_parameters, "",
          "Text proto RoutingSearchParameters (possibly partial) that will "
          "override the DefaultRoutingSearchParameters()");
ABSL_FLAG(int, vrp_precedences, 5,
          "Number of precedence indices. Precedences will be chosen "
          "randomly with the constraint that they don't form cycles.");
ABSL_FLAG(int64_t, vrp_precedence_offset, 100,
          "The offset that applies to the precedences. For each pair linked "
          "by a precedence constraint, pair.second can only start after the "
          "start of pair.first + offset.");

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
  const RoutingIndexManager::NodeIndex kDepot(0);
  RoutingIndexManager manager(absl::GetFlag(FLAGS_vrp_orders) + 1,
                              absl::GetFlag(FLAGS_vrp_vehicles), kDepot);
  RoutingModel routing(manager);

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
  RoutingDimension* time_dimension = routing.GetMutableDimension(kTime);

  // Adding time windows.
  std::mt19937 randomizer(
      GetSeed(absl::GetFlag(FLAGS_vrp_use_deterministic_random_seed)));
  const int64_t kTWDuration = 5 * 3600;
  for (int order = 1; order < manager.num_nodes(); ++order) {
    const int64_t start =
        absl::Uniform<int32_t>(randomizer, 0, kHorizon - kTWDuration);
    time_dimension->CumulVar(order)->SetRange(start, start + kTWDuration);
  }

  // Adding penalty costs to allow skipping orders.
  const int64_t kPenalty = 10000000;
  const RoutingIndexManager::NodeIndex kFirstNodeAfterDepot(1);
  for (RoutingIndexManager::NodeIndex order = kFirstNodeAfterDepot;
       order < manager.num_nodes(); ++order) {
    std::vector<int64_t> orders(1, manager.NodeToIndex(order));
    routing.AddDisjunction(orders, kPenalty);
  }

  // Adding same vehicle constraint costs for consecutive nodes.
  if (absl::GetFlag(FLAGS_vrp_use_same_vehicle_costs)) {
    std::vector<int64_t> group;
    for (RoutingIndexManager::NodeIndex order = kFirstNodeAfterDepot;
         order < manager.num_nodes(); ++order) {
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

  // If the flag is > 0, we create a DAG with random edges representing
  // precedences. If it is not possible to meet the precedence constraints, for
  // instance if the generated time window are incompatible, we expect one of
  // the underlying orders to be skipped.
  if (absl::GetFlag(FLAGS_vrp_precedences) > 0) {
    // Randomly select edges in a graph that will act as precedences.
    std::vector<std::pair<int, int>> precedences;
    GraphBuilder::RandomEdges(
        GraphBuilder::DISALLOW_ALL_CYCLES, absl::GetFlag(FLAGS_vrp_orders),
        absl::GetFlag(FLAGS_vrp_precedences), randomizer, &precedences);

    LOG(INFO) << "Adding precedences: ";
    for (const std::pair<int, int>& precedence : precedences) {
      LOG(INFO) << precedence.first << " -> " << precedence.second;
      time_dimension->AddNodePrecedence(
          {precedence.first, precedence.second,
           absl::GetFlag(FLAGS_vrp_precedence_offset)});
    }
  }
  // Solve, returns a solution if any (owned by RoutingModel).
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::GetFlag(FLAGS_routing_search_parameters), &parameters));
  const Assignment* solution = routing.SolveWithParameters(parameters);
  if (solution != nullptr) {
    DisplayPlan(manager, routing, *solution,
                absl::GetFlag(FLAGS_vrp_use_same_vehicle_costs),
                kMaxNodesPerGroup, kSameVehicleCost, {kCapacity, kTime});
  } else {
    LOG(INFO) << "No solution found.";
  }

  return 0;
}
