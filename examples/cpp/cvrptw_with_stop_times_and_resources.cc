// Copyright 2010-2017 Google
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

// Capacitated Vehicle Routing Problem with Time Windows, fixed stop times and
// capacitated resources. A stop is defined as consecutive nodes at the same
// location.
// This is an extension to the model in cvrptw.cc so refer to that file for
// more information on the common part of the model. The model implemented here
// limits the number of vehicles which can simultaneously leave or enter a node
// to one.

#include <vector>

#include "ortools/base/callback.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/join.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_flags.h"
#include "examples/cpp/cvrptw_lib.h"
#include "ortools/base/random.h"

using operations_research::IntervalVar;
using operations_research::RoutingModel;
using operations_research::RoutingSearchParameters;
using operations_research::LocationContainer;
using operations_research::RandomDemand;
using operations_research::GetSeed;
using operations_research::StopServiceTimePlusTransition;
using operations_research::ACMRandom;
using operations_research::StringAppendF;
using operations_research::StringPrintf;

DEFINE_int32(vrp_stops, 25, "Stop locations in the problem.");
DEFINE_int32(vrp_orders_per_stop, 5, "Nodes for each stop.");
DEFINE_int32(vrp_vehicles, 20, "Size of Traveling Salesman Problem instance.");
DEFINE_bool(vrp_use_deterministic_random_seed, false,
            "Use deterministic random seeds.");

const char* kTime = "Time";
const char* kCapacity = "Capacity";

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  CHECK_LT(0, FLAGS_vrp_stops) << "Specify an instance size greater than 0.";
  CHECK_LT(0, FLAGS_vrp_orders_per_stop)
      << "Specify an instance size greater than 0.";
  CHECK_LT(0, FLAGS_vrp_vehicles) << "Specify a non-null vehicle fleet size.";
  const int vrp_orders = FLAGS_vrp_stops * FLAGS_vrp_orders_per_stop;
  // Nodes are indexed from 0 to vrp_orders, the starts and ends of the routes
  // are at node 0.
  const RoutingModel::NodeIndex kDepot(0);
  RoutingModel routing(vrp_orders + 1, FLAGS_vrp_vehicles, kDepot);
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
  for (int stop = 0; stop <= FLAGS_vrp_stops; ++stop) {
    const int num_orders = stop == 0 ? 1 : FLAGS_vrp_orders_per_stop;
    locations.AddRandomLocation(kXMax, kYMax, num_orders);
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
  const int64 kStopTime = 300;
  const int64 kHorizon = 24 * 3600;
  StopServiceTimePlusTransition time(
      kStopTime, locations,
      NewPermanentCallback(&locations, &LocationContainer::ManhattanTime));
  routing.AddDimension(
      NewPermanentCallback(&time, &StopServiceTimePlusTransition::Compute),
      kHorizon, kHorizon, /*fix_start_cumul_to_zero=*/false, kTime);

  // Adding time windows, for the sake of simplicty same for each stop.
  ACMRandom randomizer(GetSeed(FLAGS_vrp_use_deterministic_random_seed));
  const int64 kTWDuration = 5 * 3600;
  for (int stop = 0; stop < FLAGS_vrp_stops; ++stop) {
    const int64 start = randomizer.Uniform(kHorizon - kTWDuration);
    for (int stop_order = 0; stop_order < FLAGS_vrp_orders_per_stop;
         ++stop_order) {
      const int order = stop * FLAGS_vrp_orders_per_stop + stop_order + 1;
      routing.CumulVar(order, kTime)->SetRange(start, start + kTWDuration);
    }
  }

  // Adding resource constraints at order locations.
  operations_research::Solver* const solver = routing.solver();
  std::vector<IntervalVar*> intervals;
  for (int stop = 0; stop < FLAGS_vrp_stops; ++stop) {
    std::vector<IntervalVar*> stop_intervals;
    for (int stop_order = 0; stop_order < FLAGS_vrp_orders_per_stop;
         ++stop_order) {
      const int order = stop * FLAGS_vrp_orders_per_stop + stop_order + 1;
      IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
          0, kHorizon, kStopTime, true, StrCat("Order", order));
      intervals.push_back(interval);
      stop_intervals.push_back(interval);
      // Link order and interval.
      operations_research::IntVar* const order_start =
          routing.CumulVar(order, kTime);
      solver->AddConstraint(
          solver->MakeIsEqualCt(interval->SafeStartExpr(0), order_start,
                                interval->PerformedExpr()->Var()));
      // Make interval performed iff corresponding order has service time.
      // An order has no service time iff it is at the same location as the
      // next order on the route.
      operations_research::IntVar* const is_null_duration =
          solver->MakeElement([&locations, order](int64 index) {
                  return locations.SameLocationFromIndex(order, index);
                }, routing.NextVar(order))->Var();
      solver->AddConstraint(
          solver->MakeNonEquality(interval->PerformedExpr(), is_null_duration));
      routing.AddIntervalToAssignment(interval);
      // We are minimizing route durations by minimizing route ends; so we can
      // maximize order starts to pack them together.
      routing.AddVariableMaximizedByFinalizer(order_start);
    }
    // Only one order can happen at the same time at a given location.
    std::vector<int64> location_usage(stop_intervals.size(), 1);
    solver->AddConstraint(solver->MakeCumulative(
        stop_intervals, location_usage, 1, StrCat("Client", stop)));
  }
  // Minimizing route duration.
  for (int vehicle = 0; vehicle < routing.vehicles(); ++vehicle) {
    routing.AddVariableMinimizedByFinalizer(
        routing.CumulVar(routing.End(vehicle), kTime));
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
    LOG(INFO) << "Stop intervals:";
    for (IntervalVar* const interval : intervals) {
      if (solution->PerformedValue(interval)) {
        LOG(INFO) << interval->name() << ": " << solution->StartValue(interval);
      }
    }
  } else {
    LOG(INFO) << "No solution found.";
  }
  return 0;
}
