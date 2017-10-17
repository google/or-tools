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

//
// Capacitated Vehicle Routing Problem with Time Windows and Breaks.
// A description of the Capacitated Vehicle Routing Problem with Time Windows
// can be found here:
// http://en.wikipedia.org/wiki/Vehicle_routing_problem.
// The variant which is tackled by this model includes a capacity dimension,
// time windows and optional orders, with a penalty cost if orders are not
// performed. For the sake of simplicty, orders are randomly located and
// distances are computed using the Manhattan distance. Distances are assumed
// to be in meters and times in seconds.
// This variant also includes vehicle breaks which must happen during the day
// with two alternate breaks schemes: either a long break in the middle of the
// day or two smaller ones which can be taken during a longer period of the day.

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

using operations_research::RoutingModel;
using operations_research::RoutingSearchParameters;
using operations_research::LocationContainer;
using operations_research::RandomDemand;
using operations_research::ServiceTimePlusTransition;
using operations_research::GetSeed;
using operations_research::ACMRandom;


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
      operations_research::FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION);
  parameters.mutable_local_search_operators()->set_use_path_lns(false);
  parameters.mutable_local_search_operators()->set_use_inactive_lns(false);

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
  operations_research::RoutingDimension* const time_dimension =
      routing.GetMutableDimension(kTime);

  // Adding time windows.
  ACMRandom randomizer(GetSeed(FLAGS_vrp_use_deterministic_random_seed));
  const int64 kTWDuration = 5 * 3600;
  for (int order = 1; order < routing.nodes(); ++order) {
    const int64 start = randomizer.Uniform(kHorizon - kTWDuration);
    time_dimension->CumulVar(order)->SetRange(start, start + kTWDuration);
    routing.AddToAssignment(time_dimension->SlackVar(order));
  }

  // Minimize time variables.
  for (int i = 0; i < routing.Size(); ++i) {
    routing.AddVariableMinimizedByFinalizer(time_dimension->CumulVar(i));
  }
  for (int j = 0; j < FLAGS_vrp_vehicles; ++j) {
    routing.AddVariableMinimizedByFinalizer(
        time_dimension->CumulVar(routing.Start(j)));
    routing.AddVariableMinimizedByFinalizer(
        time_dimension->CumulVar(routing.End(j)));
  }

  // Adding vehicle breaks:
  // - 40min breaks between 11:00am and 1:00pm
  // or
  // - 2 x 30min breaks between 10:00am and 3:00pm, at least 1h apart
  const std::vector<std::vector<int>> break_data = {
      {/*start_min*/ 11, /*start_max*/ 13, /*duration*/ 2400},
      {/*start_min*/ 10, /*start_max*/ 15, /*duration*/ 1800},
      {/*start_min*/ 10, /*start_max*/ 15, /*duration*/ 1800}};
  operations_research::Solver* const solver = routing.solver();
  for (int vehicle = 0; vehicle < FLAGS_vrp_vehicles; ++vehicle) {
    std::vector<operations_research::IntervalVar*> breaks;
    for (int i = 0; i < break_data.size(); ++i) {
      operations_research::IntervalVar* const break_interval =
          solver->MakeFixedDurationIntervalVar(
              break_data[i][0] * 3600, break_data[i][1] * 3600,
              break_data[i][2], true,
              StrCat("Break ", i, " on vehicle ", vehicle));
      breaks.push_back(break_interval);
    }
    // break1 performed iff break2 performed
    solver->AddConstraint(solver->MakeEquality(breaks[1]->PerformedExpr(),
                                               breaks[2]->PerformedExpr()));
    // break2 start 1h after break1.
    solver->AddConstraint(solver->MakeIntervalVarRelationWithDelay(
        breaks[2], operations_research::Solver::STARTS_AFTER_END, breaks[1],
        3600));
    // break0 performed iff break2 unperformed
    solver->AddConstraint(solver->MakeNonEquality(breaks[0]->PerformedExpr(),
                                                  breaks[2]->PerformedExpr()));

    time_dimension->SetBreakIntervalsOfVehicle(std::move(breaks), vehicle);
  }

  // Adding penalty costs to allow skipping orders.
  const int64 kPenalty = 10000000;
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
    LOG(INFO) << "Breaks: ";
    for (const auto& break_interval :
         solution->IntervalVarContainer().elements()) {
      if (break_interval.PerformedValue() == 1) {
        LOG(INFO) << break_interval.Var()->name() << " "
                  << break_interval.DebugString();
      } else {
        LOG(INFO) << break_interval.Var()->name() << " unperformed";
      }
    }
    DisplayPlan(routing, *solution, false, 0, 0,
                routing.GetDimensionOrDie(kCapacity),
                routing.GetDimensionOrDie(kTime));
  } else {
    LOG(INFO) << "No solution found.";
  }
  return 0;
}
