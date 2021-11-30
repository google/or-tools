// Copyright 2010-2021 Google LLC
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

#include <cstdint>
#include <vector>

#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "examples/cpp/cvrptw_lib.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"

using operations_research::Assignment;
using operations_research::DefaultRoutingSearchParameters;
using operations_research::GetSeed;
using operations_research::IntervalVar;
using operations_research::IntVar;
using operations_research::LocationContainer;
using operations_research::RandomDemand;
using operations_research::RoutingDimension;
using operations_research::RoutingIndexManager;
using operations_research::RoutingModel;
using operations_research::RoutingNodeIndex;
using operations_research::RoutingSearchParameters;
using operations_research::Solver;
using operations_research::StopServiceTimePlusTransition;

ABSL_FLAG(int, vrp_stops, 25, "Stop locations in the problem.");
ABSL_FLAG(int, vrp_orders_per_stop, 5, "Nodes for each stop.");
ABSL_FLAG(int, vrp_vehicles, 20,
          "Size of Traveling Salesman Problem instance.");
ABSL_FLAG(bool, vrp_use_deterministic_random_seed, false,
          "Use deterministic random seeds.");
ABSL_FLAG(std::string, routing_search_parameters, "",
          "Text proto RoutingSearchParameters (possibly partial) that will "
          "override the DefaultRoutingSearchParameters()");

const char* kTime = "Time";
const char* kCapacity = "Capacity";

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);
  CHECK_LT(0, absl::GetFlag(FLAGS_vrp_stops))
      << "Specify an instance size greater than 0.";
  CHECK_LT(0, absl::GetFlag(FLAGS_vrp_orders_per_stop))
      << "Specify an instance size greater than 0.";
  CHECK_LT(0, absl::GetFlag(FLAGS_vrp_vehicles))
      << "Specify a non-null vehicle fleet size.";
  const int vrp_orders =
      absl::GetFlag(FLAGS_vrp_stops) * absl::GetFlag(FLAGS_vrp_orders_per_stop);
  // Nodes are indexed from 0 to vrp_orders, the starts and ends of the routes
  // are at node 0.
  const RoutingIndexManager::NodeIndex kDepot(0);
  RoutingIndexManager manager(vrp_orders + 1, absl::GetFlag(FLAGS_vrp_vehicles),
                              kDepot);
  RoutingModel routing(manager);

  // Setting up locations.
  const int64_t kXMax = 100000;
  const int64_t kYMax = 100000;
  const int64_t kSpeed = 10;
  LocationContainer locations(
      kSpeed, absl::GetFlag(FLAGS_vrp_use_deterministic_random_seed));
  for (int stop = 0; stop <= absl::GetFlag(FLAGS_vrp_stops); ++stop) {
    const int num_orders =
        stop == 0 ? 1 : absl::GetFlag(FLAGS_vrp_orders_per_stop);
    locations.AddRandomLocation(kXMax, kYMax, num_orders);
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
  const int64_t kStopTime = 300;
  const int64_t kHorizon = 24 * 3600;
  StopServiceTimePlusTransition time(
      kStopTime, locations,
      [&locations](RoutingNodeIndex i, RoutingNodeIndex j) {
        return locations.ManhattanTime(i, j);
      });
  routing.AddDimension(
      routing.RegisterTransitCallback([&time, &manager](int64_t i, int64_t j) {
        return time.Compute(manager.IndexToNode(i), manager.IndexToNode(j));
      }),
      kHorizon, kHorizon, /*fix_start_cumul_to_zero=*/false, kTime);
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie(kTime);

  // Adding time windows, for the sake of simplicty same for each stop.
  std::mt19937 randomizer(
      GetSeed(absl::GetFlag(FLAGS_vrp_use_deterministic_random_seed)));
  const int64_t kTWDuration = 5 * 3600;
  for (int stop = 0; stop < absl::GetFlag(FLAGS_vrp_stops); ++stop) {
    const int64_t start =
        absl::Uniform<int32_t>(randomizer, 0, kHorizon - kTWDuration);
    for (int stop_order = 0;
         stop_order < absl::GetFlag(FLAGS_vrp_orders_per_stop); ++stop_order) {
      const int order =
          stop * absl::GetFlag(FLAGS_vrp_orders_per_stop) + stop_order + 1;
      time_dimension.CumulVar(order)->SetRange(start, start + kTWDuration);
    }
  }

  // Adding resource constraints at order locations.
  Solver* const solver = routing.solver();
  std::vector<IntervalVar*> intervals;
  for (int stop = 0; stop < absl::GetFlag(FLAGS_vrp_stops); ++stop) {
    std::vector<IntervalVar*> stop_intervals;
    for (int stop_order = 0;
         stop_order < absl::GetFlag(FLAGS_vrp_orders_per_stop); ++stop_order) {
      const int order =
          stop * absl::GetFlag(FLAGS_vrp_orders_per_stop) + stop_order + 1;
      IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
          0, kHorizon, kStopTime, true, absl::StrCat("Order", order));
      intervals.push_back(interval);
      stop_intervals.push_back(interval);
      // Link order and interval.
      IntVar* const order_start = time_dimension.CumulVar(order);
      solver->AddConstraint(
          solver->MakeIsEqualCt(interval->SafeStartExpr(0), order_start,
                                interval->PerformedExpr()->Var()));
      // Make interval performed iff corresponding order has service time.
      // An order has no service time iff it is at the same location as the
      // next order on the route.
      IntVar* const is_null_duration =
          solver
              ->MakeElement(
                  [&locations, order](int64_t index) {
                    return locations.SameLocationFromIndex(order, index);
                  },
                  routing.NextVar(order))
              ->Var();
      solver->AddConstraint(
          solver->MakeNonEquality(interval->PerformedExpr(), is_null_duration));
      routing.AddIntervalToAssignment(interval);
      // We are minimizing route durations by minimizing route ends; so we can
      // maximize order starts to pack them together.
      routing.AddVariableMaximizedByFinalizer(order_start);
    }
    // Only one order can happen at the same time at a given location.
    std::vector<int64_t> location_usage(stop_intervals.size(), 1);
    solver->AddConstraint(solver->MakeCumulative(
        stop_intervals, location_usage, 1, absl::StrCat("Client", stop)));
  }
  // Minimizing route duration.
  for (int vehicle = 0; vehicle < manager.num_vehicles(); ++vehicle) {
    routing.AddVariableMinimizedByFinalizer(
        time_dimension.CumulVar(routing.End(vehicle)));
  }

  // Adding penalty costs to allow skipping orders.
  const int64_t kPenalty = 100000;
  const RoutingIndexManager::NodeIndex kFirstNodeAfterDepot(1);
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
  return EXIT_SUCCESS;
}
