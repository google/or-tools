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

// Vehicle Routing Problem with Breaks.:
// A description of the Vehicle Routing Problem can be found here:
// http://en.wikipedia.org/wiki/Vehicle_routing_problem.
// This variant also includes vehicle breaks which must happen during the day
// with two alternate breaks schemes: either a long break in the middle of the
// day or two smaller ones which can be taken during a longer period of the day.

// [START program]
// [START import]
#include <cstdint>
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"
// [END import]

namespace operations_research {
// [START data_model]
struct DataModel {
  const std::vector<std::vector<int64_t>> time_matrix{
      {0, 27, 38, 34, 29, 13, 25, 9, 15, 9, 26, 25, 19, 17, 23, 38, 33},
      {27, 0, 34, 15, 9, 25, 36, 17, 34, 37, 54, 29, 24, 33, 50, 43, 60},
      {38, 34, 0, 49, 43, 25, 13, 40, 23, 37, 20, 63, 58, 56, 39, 77, 37},
      {34, 15, 49, 0, 5, 32, 43, 25, 42, 44, 61, 25, 31, 41, 58, 28, 67},
      {29, 9, 43, 5, 0, 26, 38, 19, 36, 38, 55, 20, 25, 35, 52, 33, 62},
      {13, 25, 25, 32, 26, 0, 11, 15, 9, 12, 29, 38, 33, 31, 25, 52, 35},
      {25, 36, 13, 43, 38, 11, 0, 26, 9, 23, 17, 50, 44, 42, 25, 63, 24},
      {9, 17, 40, 25, 19, 15, 26, 0, 17, 19, 36, 23, 17, 16, 33, 37, 42},
      {15, 34, 23, 42, 36, 9, 9, 17, 0, 13, 19, 40, 34, 33, 16, 54, 25},
      {9, 37, 37, 44, 38, 12, 23, 19, 13, 0, 17, 26, 21, 19, 13, 40, 23},
      {26, 54, 20, 61, 55, 29, 17, 36, 19, 17, 0, 43, 38, 36, 19, 57, 17},
      {25, 29, 63, 25, 20, 38, 50, 23, 40, 26, 43, 0, 5, 15, 32, 13, 42},
      {19, 24, 58, 31, 25, 33, 44, 17, 34, 21, 38, 5, 0, 9, 26, 19, 36},
      {17, 33, 56, 41, 35, 31, 42, 16, 33, 19, 36, 15, 9, 0, 17, 21, 26},
      {23, 50, 39, 58, 52, 25, 25, 33, 16, 13, 19, 32, 26, 17, 0, 38, 9},
      {38, 43, 77, 28, 33, 52, 63, 37, 54, 40, 57, 13, 19, 21, 38, 0, 39},
      {33, 60, 37, 67, 62, 35, 24, 42, 25, 23, 17, 42, 36, 26, 9, 39, 0},
  };
  const std::vector<int64_t> service_time{
      0, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
  };
  const int num_vehicles = 4;
  const RoutingIndexManager::NodeIndex depot{0};
};
// [END data_model]

//! @brief Print the solution.
//! @param[in] data Data of the problem.
//! @param[in] manager Index manager used.
//! @param[in] routing Routing solver used.
//! @param[in] solution Solution found by the solver.
// [START solution_printer]
void PrintSolution(const RoutingIndexManager& manager,
                   const RoutingModel& routing, const Assignment& solution) {
  LOG(INFO) << "Objective: " << solution.ObjectiveValue();

  LOG(INFO) << "Breaks:";
  const Assignment::IntervalContainer& intervals =
      solution.IntervalVarContainer();
  for (const IntervalVarElement& break_interval : intervals.elements()) {
    if (break_interval.PerformedValue()) {
      LOG(INFO) << break_interval.Var()->name() << " "
                << break_interval.DebugString();
    } else {
      LOG(INFO) << break_interval.Var()->name() << ": Unperformed";
    }
  }

  const RoutingDimension& time_dimension = routing.GetDimensionOrDie("Time");
  int64_t total_time{0};
  for (int vehicle_id = 0; vehicle_id < manager.num_vehicles(); ++vehicle_id) {
    LOG(INFO) << "Route for Vehicle " << vehicle_id << ":";
    int64_t index = routing.Start(vehicle_id);
    std::stringstream route;
    while (routing.IsEnd(index) == false) {
      const IntVar* time_var = time_dimension.CumulVar(index);
      route << manager.IndexToNode(index).value() << " Time("
            << solution.Value(time_var) << ") -> ";
      index = solution.Value(routing.NextVar(index));
    }
    const IntVar* time_var = time_dimension.CumulVar(index);
    route << manager.IndexToNode(index).value() << " Time("
          << solution.Value(time_var) << ")";
    LOG(INFO) << route.str();
    LOG(INFO) << "Time of the route: " << solution.Value(time_var) << "min";
    total_time += solution.Value(time_var);
  }
  LOG(INFO) << "Total time of all routes: " << total_time << "min";
  LOG(INFO) << "";
  LOG(INFO) << "Problem solved in " << routing.solver()->wall_time() << "ms";
}
// [END solution_printer]

void VrpBreaks() {
  // Instantiate the data problem.
  // [START data]
  DataModel data;
  // [END data]

  // Create Routing Index Manager
  // [START index_manager]
  RoutingIndexManager manager(data.time_matrix.size(), data.num_vehicles,
                              data.depot);
  // [END index_manager]

  // Create Routing Model.
  // [START routing_model]
  RoutingModel routing(manager);
  // [END routing_model]

  // Create and register a transit callback.
  // [START transit_callback]
  const int transit_callback_index = routing.RegisterTransitCallback(
      [&data, &manager](int64_t from_index, int64_t to_index) -> int64_t {
        // Convert from routing variable Index to distance matrix NodeIndex.
        int from_node = manager.IndexToNode(from_index).value();
        int to_node = manager.IndexToNode(to_index).value();
        return data.time_matrix[from_node][to_node] +
               data.service_time[from_node];
      });
  // [END transit_callback]

  // Define cost of each arc.
  // [START arc_cost]
  routing.SetArcCostEvaluatorOfAllVehicles(transit_callback_index);
  // [END arc_cost]

  // Add Time constraint.
  // [START time_constraint]
  routing.AddDimension(transit_callback_index,
                       10,    // needed optional waiting time to place break
                       180,   // maximum time per vehicle
                       true,  // Force start cumul to zero.
                       "Time");
  RoutingDimension* time_dimension = routing.GetMutableDimension("Time");
  time_dimension->SetGlobalSpanCostCoefficient(10);
  // [END time_constraint]

  // Add Breaks
  std::vector<int64_t> service_times(routing.Size());
  for (int index = 0; index < routing.Size(); index++) {
    const RoutingIndexManager::NodeIndex node = manager.IndexToNode(index);
    service_times[index] = data.service_time[node.value()];
  }

  Solver* const solver = routing.solver();
  for (int vehicle = 0; vehicle < manager.num_vehicles(); ++vehicle) {
    std::vector<IntervalVar*> break_intervals;
    IntervalVar* const break_interval = solver->MakeFixedDurationIntervalVar(
        50,     // start min
        60,     // start max
        10,     // duration: 10min
        false,  // optional: no
        absl::StrCat("Break for vehicle ", vehicle));
    break_intervals.push_back(break_interval);

    time_dimension->SetBreakIntervalsOfVehicle(break_intervals, vehicle,
                                               service_times);
  }

  // Setting first solution heuristic.
  // [START parameters]
  RoutingSearchParameters searchParameters = DefaultRoutingSearchParameters();
  searchParameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);
  // [END parameters]

  // Solve the problem.
  // [START solve]
  const Assignment* solution = routing.SolveWithParameters(searchParameters);
  // [END solve]

  // Print solution on console.
  // [START print_solution]
  if (solution != nullptr) {
    PrintSolution(manager, routing, *solution);
  } else {
    LOG(INFO) << "No solution found.";
  }
  // [END print_solution]
}
}  // namespace operations_research

int main(int /*argc*/, char** argv) {
  operations_research::VrpBreaks();
  return EXIT_SUCCESS;
}
// [END program]
