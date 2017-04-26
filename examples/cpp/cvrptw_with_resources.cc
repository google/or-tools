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

// Capacitated Vehicle Routing Problem with Time Windows and capacitated
// resources.
// This is an extension to the model in cvrptw.cc so refer to that file for
// more information on the common part of the model. The model implemented here
// limits the number of vehicles which can simultaneously leave or enter the
// depot due to limited resources (or capacity) available.
// TODO(user): The current model consumes resources even for vehicles with
// empty routes; fix this when we have an API on the cumulative constraints
// with variable demands.

#include <vector>

#include "ortools/base/callback.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_flags.h"
#include "examples/cpp/cvrptw_lib.h"
#include "ortools/base/random.h"

using operations_research::RoutingModel;
using operations_research::RoutingSearchParameters;
using operations_research::LocationContainer;
using operations_research::RandomDemand;
using operations_research::ServiceTimePlusTransition;
using operations_research::GetSeed;
using operations_research::ACMRandom;
using operations_research::StringAppendF;
using operations_research::StringPrintf;

DEFINE_int32(vrp_orders, 100, "Nodes in the problem.");
DEFINE_int32(vrp_vehicles, 20, "Size of Traveling Salesman Problem instance.");
DEFINE_bool(vrp_use_deterministic_random_seed, false,
            "Use deterministic random seeds.");

const char* kTime = "Time";
const char* kCapacity = "Capacity";

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  CHECK_LT(0, FLAGS_vrp_orders) << "Specify an instance size greater than 0.";
  CHECK_LT(0, FLAGS_vrp_vehicles) << "Specify a non-null vehicle fleet size.";
  // VRP of size FLAGS_vrp_size.
  // Nodes are indexed from 0 to FLAGS_vrp_orders, the starts and ends of
  // the routes are at node 0.
  const RoutingModel::NodeIndex kDepot(0);
  RoutingModel routing(FLAGS_vrp_orders + 1, FLAGS_vrp_vehicles, kDepot);
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

  // Adding time windows.
  ACMRandom randomizer(GetSeed(FLAGS_vrp_use_deterministic_random_seed));
  const int64 kTWDuration = 5 * 3600;
  for (int order = 1; order < routing.nodes(); ++order) {
    const int64 start = randomizer.Uniform(kHorizon - kTWDuration);
    routing.CumulVar(order, kTime)->SetRange(start, start + kTWDuration);
  }

  // Adding resource constraints at the depot (start and end location of
  // routes).
  std::vector<operations_research::IntVar*> start_end_times;
  for (int i = 0; i < FLAGS_vrp_vehicles; ++i) {
    start_end_times.push_back(routing.CumulVar(routing.End(i), kTime));
    start_end_times.push_back(routing.CumulVar(routing.Start(i), kTime));
  }
  // Build corresponding time intervals.
  const int64 kVehicleSetup = 180;
  operations_research::Solver* const solver = routing.solver();
  std::vector<operations_research::IntervalVar*> intervals;
  solver->MakeFixedDurationIntervalVarArray(start_end_times, kVehicleSetup,
                                            "depot_interval", &intervals);
  // Constrain the number of maximum simultaneous intervals at depot.
  const int64 kDepotCapacity = 5;
  std::vector<int64> depot_usage(start_end_times.size(), 1);
  solver->AddConstraint(
      solver->MakeCumulative(intervals, depot_usage, kDepotCapacity, "depot"));
  // Instantiate route start and end times to produce feasible times.
  for (int i = 0; i < start_end_times.size(); ++i) {
    routing.AddVariableMinimizedByFinalizer(start_end_times[i]);
  }

  // Adding penalty costs to allow skipping orders.
  const int64 kPenalty = 100000;
  const RoutingModel::NodeIndex kFirstNodeAfterDepot(1);
  for (RoutingModel::NodeIndex order = kFirstNodeAfterDepot;
       order < routing.nodes(); ++order) {
    std::vector<RoutingModel::NodeIndex> orders(1, order);
    routing.AddDisjunction(orders, kPenalty);
  }

  // Solve, returns a solution if any (owned by RoutingModel).
  const operations_research::Assignment* solution =
      routing.SolveWithParameters(parameters);
  if (solution != nullptr) {
    DisplayPlan(routing, *solution, /*use_same_vehicle_costs=*/false,
                /*max_nodes_per_group=*/0, /*same_vehicle_cost=*/0,
                routing.GetDimensionOrDie(kCapacity),
                routing.GetDimensionOrDie(kTime));
  } else {
    LOG(INFO) << "No solution found.";
  }
  return 0;
}
