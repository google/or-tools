// Copyright 2010-2014 Google
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
// Capacitated Vehicle Routing Problem with Disjoint Time Windows (and optional
// orders).
// A description of the problem can be found here:
// http://en.wikipedia.org/wiki/Vehicle_routing_problem.
// The variant which is tackled by this model includes a capacity dimension,
// disjoint time windows and optional orders, with a penalty cost if orders are
// not performed. For the sake of simplicty, orders are randomly located and
// distances are computed using the Manhattan distance. Distances are assumed
// to be in meters and times in seconds.

#include <vector>

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "constraint_solver/routing.h"
#include "constraint_solver/routing_flags.h"
#include "cpp/cvrptw_lib.h"

using operations_research::RoutingModel;
using operations_research::RoutingSearchParameters;
using operations_research::LocationContainer;
using operations_research::RandomDemand;
using operations_research::ServiceTimePlusTransition;
using operations_research::GetSeed;
using operations_research::ACMRandom;


DEFINE_int32(vrp_orders, 100, "Nodes in the problem.");
DEFINE_int32(vrp_vehicles, 20, "Size of Traveling Salesman Problem instance.");
DEFINE_int32(vrp_windows, 5, "Number of disjoint windows per node.");
DEFINE_bool(vrp_use_deterministic_random_seed, false,
            "Use deterministic random seeds.");
DEFINE_bool(vrp_use_same_vehicle_costs, false,
            "Use same vehicle costs in the routing model");

const char* kTime = "Time";
const char* kCapacity = "Capacity";
const int64 kMaxNodesPerGroup = 10;
const int64 kSameVehicleCost = 1000;

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  CHECK_LT(0, FLAGS_vrp_orders) << "Specify an instance size greater than 0.";
  CHECK_LT(0, FLAGS_vrp_vehicles) << "Specify a non-null vehicle fleet size.";
  // VRP of size FLAGS_vrp_size.
  // Nodes are indexed from 0 to FLAGS_vrp_orders, the starts and ends of
  // the routes are at node 0.
  const RoutingModel::NodeIndex kDepot(0);
  RoutingModel routing(FLAGS_vrp_orders + 1, FLAGS_vrp_vehicles);
  routing.SetDepot(kDepot);
  RoutingSearchParameters parameters =
      operations_research::BuildSearchParametersFromFlags();
  parameters.set_first_solution_strategy(
      operations_research::FirstSolutionStrategy::PATH_CHEAPEST_ARC);
  parameters.mutable_local_search_operators()->set_use_path_lns(false);

  // Setting up locations.
  const int64 kXMax = 100000;
  const int64 kYMax = 100000;
  const int64 kSpeed = 10;
  LocationContainer locations(kSpeed, FLAGS_vrp_use_deterministic_random_seed);
  for (int location = 0; location <= FLAGS_vrp_orders; ++location) {
    locations.AddRandomLocation(kXMax, kYMax);
  }

  // Setting the cost function.
  routing.SetArcCostEvaluatorOfAllVehicles(
      NewPermanentCallback(&locations, &LocationContainer::ManhattanDistance));

  // Adding capacity dimension constraints.
  const int64 kVehicleCapacity = 40;
  const int64 kNullCapacitySlack = 0;
  RandomDemand demand(routing.nodes(), kDepot,
                      FLAGS_vrp_use_deterministic_random_seed);
  demand.Initialize();
  routing.AddDimension(NewPermanentCallback(&demand, &RandomDemand::Demand),
                       kNullCapacitySlack, kVehicleCapacity,
                       /*fix_start_cumul_to_zero=*/true, kCapacity);

  // Adding time dimension constraints.
  const int64 kTimePerDemandUnit = 300;
  const int64 kHorizon = 24 * 3600;
  ServiceTimePlusTransition time(
      kTimePerDemandUnit, NewPermanentCallback(&demand, &RandomDemand::Demand),
      NewPermanentCallback(&locations, &LocationContainer::ManhattanTime));
  routing.AddDimension(
      NewPermanentCallback(&time, &ServiceTimePlusTransition::Compute),
      kHorizon, kHorizon, /*fix_start_cumul_to_zero=*/false, kTime);
  const operations_research::RoutingDimension& time_dimension =
      routing.GetDimensionOrDie(kTime);

  // Adding disjoint time windows.
  operations_research::Solver* solver = routing.solver();
  ACMRandom randomizer(GetSeed(FLAGS_vrp_use_deterministic_random_seed));
  for (int order = 1; order < routing.nodes(); ++order) {
    std::vector<int64> forbid_points(2 * FLAGS_vrp_windows, 0);
    for (int i = 0; i < forbid_points.size(); ++i) {
      forbid_points[i] = randomizer.Uniform(kHorizon);
    }
    std::sort(forbid_points.begin(), forbid_points.end());
    std::vector<int64> forbid_starts(1, 0);
    std::vector<int64> forbid_ends;
    for (int i = 0; i < forbid_points.size(); i += 2) {
      forbid_ends.push_back(forbid_points[i]);
      forbid_starts.push_back(forbid_points[i + 1]);
    }
    forbid_ends.push_back(kHorizon);
    solver->AddConstraint(solver->MakeNotMemberCt(
        time_dimension.CumulVar(order), forbid_starts, forbid_ends));
  }

  // Adding penalty costs to allow skipping orders.
  const int64 kPenalty = 10000000;
  const RoutingModel::NodeIndex kFirstNodeAfterDepot(1);
  for (RoutingModel::NodeIndex order = kFirstNodeAfterDepot;
       order < routing.nodes(); ++order) {
    std::vector<RoutingModel::NodeIndex> orders(1, order);
    routing.AddDisjunction(orders, kPenalty);
  }

  // Adding same vehicle constraint costs for consecutive nodes.
  if (FLAGS_vrp_use_same_vehicle_costs) {
    std::vector<RoutingModel::NodeIndex> group;
    for (RoutingModel::NodeIndex order = kFirstNodeAfterDepot;
         order < routing.nodes(); ++order) {
      group.push_back(order);
      if (group.size() == kMaxNodesPerGroup) {
        routing.AddSoftSameVehicleConstraint(group, kSameVehicleCost);
        group.clear();
      }
    }
    if (!group.empty()) {
      routing.AddSoftSameVehicleConstraint(group, kSameVehicleCost);
    }
  }

  // Solve, returns a solution if any (owned by RoutingModel).
  const operations_research::Assignment* solution =
      routing.SolveWithParameters(parameters);
  if (solution != NULL) {
    DisplayPlan(routing, *solution, FLAGS_vrp_use_same_vehicle_costs,
                kMaxNodesPerGroup, kSameVehicleCost,
                routing.GetDimensionOrDie(kCapacity),
                routing.GetDimensionOrDie(kTime));
  } else {
    LOG(INFO) << "No solution found.";
  }
  return 0;
}
