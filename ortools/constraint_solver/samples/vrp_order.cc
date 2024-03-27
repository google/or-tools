// Copyright 2010-2018 Google LLC
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

// [START program]
// [START import]
#include <string>
#include <vector>
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"
// [END import]

namespace operations_research {
// [START data_model]
struct DataModel {
  const std::vector<std::vector<int64>> time_matrix{
      {0, 6, 9, 8, 7, 3, 6, 2, 3, 2, 6, 6, 4, 4, 5, 9, 7},
      {6, 0, 8, 3, 2, 6, 8, 4, 8, 8, 13, 7, 5, 8, 12, 10, 14},
      {9, 8, 0, 11, 10, 6, 3, 9, 5, 8, 4, 15, 14, 13, 9, 18, 9},
      {8, 3, 11, 0, 1, 7, 10, 6, 10, 10, 14, 6, 7, 9, 14, 6, 16},
      {7, 2, 10, 1, 0, 6, 9, 4, 8, 9, 13, 4, 6, 8, 12, 8, 14},
      {3, 6, 6, 7, 6, 0, 2, 3, 2, 2, 7, 9, 7, 7, 6, 12, 8},
      {6, 8, 3, 10, 9, 2, 0, 6, 2, 5, 4, 12, 10, 10, 6, 15, 5},
      {2, 4, 9, 6, 4, 3, 6, 0, 4, 4, 8, 5, 4, 3, 7, 8, 10},
      {3, 8, 5, 10, 8, 2, 2, 4, 0, 3, 4, 9, 8, 7, 3, 13, 6},
      {2, 8, 8, 10, 9, 2, 5, 4, 3, 0, 4, 6, 5, 4, 3, 9, 5},
      {6, 13, 4, 14, 13, 7, 4, 8, 4, 4, 0, 10, 9, 8, 4, 13, 4},
      {6, 7, 15, 6, 4, 9, 12, 5, 9, 6, 10, 0, 1, 3, 7, 3, 10},
      {4, 5, 14, 7, 6, 7, 10, 4, 8, 5, 9, 1, 0, 2, 6, 4, 8},
      {4, 8, 13, 9, 8, 7, 10, 3, 7, 4, 8, 3, 2, 0, 4, 5, 6},
      {5, 12, 9, 14, 12, 6, 6, 7, 3, 3, 4, 7, 6, 4, 0, 9, 2},
      {9, 10, 18, 6, 8, 12, 15, 8, 13, 9, 13, 3, 4, 5, 9, 0, 9},
      {7, 14, 9, 16, 14, 8, 5, 10, 6, 5, 4, 10, 8, 6, 2, 9, 0},
  };
  const std::vector<int64> order_relation{
    1, 2, 5, 6, 8, 10, 13, 11
  };
  const int num_vehicles = 4;
  const RoutingIndexManager::NodeIndex depot{16};
};
// [END data_model]

// [START solution_printer]
//! @brief Print the solution.
//! @param[in] data Data of the problem.
//! @param[in] manager Index manager used.
//! @param[in] routing Routing solver used.
//! @param[in] solution Solution found by the solver.
void PrintSolution(const DataModel& data, const RoutingIndexManager& manager,
                   const RoutingModel& routing, const Assignment& solution) {
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie("Time");
  int64 total_time{0};
  for (int vehicle_id = 0; vehicle_id < data.num_vehicles; ++vehicle_id) {
    int64 index = routing.Start(vehicle_id);
    LOG(INFO) << "Route for vehicle " << vehicle_id << ":";
    std::ostringstream route;
    while (routing.IsEnd(index) == false) {
      auto time_var = time_dimension.CumulVar(index);
      route << manager.IndexToNode(index).value() << " Time("
            << solution.Min(time_var) << ", " << solution.Max(time_var)
            << ") -> ";
      index = solution.Value(routing.NextVar(index));
    }
    auto time_var = time_dimension.CumulVar(index);
    LOG(INFO) << route.str() << manager.IndexToNode(index).value() << " Time("
              << solution.Min(time_var) << ", " << solution.Max(time_var)
              << ")";
    LOG(INFO) << "Time of the route: " << solution.Min(time_var) << "min";
    total_time += solution.Min(time_var);
  }
  LOG(INFO) << "Total time of all routes: " << total_time << "min";
  LOG(INFO) << "";
  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Problem solved in " << routing.solver()->wall_time() << "ms";
}
// [END solution_printer]

void VrpOrder() {
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
  std::vector<int> time_evaluators;
  for (int vehicle_index = 0; vehicle_index < data.num_vehicles;
       ++vehicle_index) {
    time_evaluators.push_back(
      routing.RegisterTransitCallback(
        [&data, &manager](int64 from_index, int64 to_index) -> int64 {
      // Convert from routing variable Index to time matrix NodeIndex.
      auto from_node = manager.IndexToNode(from_index).value();
      auto to_node = manager.IndexToNode(to_index).value();
      return data.time_matrix[from_node][to_node];
      }));
  }
  // [END transit_callback]

  // Define cost of each arc.
  // [START arc_cost]
  std::string time{"Time"};
  routing.AddDimensionWithVehicleTransits(time_evaluators,
                       int64{100},               // allow waiting time
                       int64{100},               // maximum time per vehicle
                       false,  // Don't force start cumul to zero
                       time);
  for (int vehicle_index = 0; vehicle_index < data.num_vehicles;
       ++vehicle_index) {
    routing.GetMutableDimension(time)->SetSpanCostCoefficientForVehicle(
        1, vehicle_index);
  }
  // [END arc_cost]

  // Create the order expected for a part of the nodes to visit
  // [START order constraint]
  Solver* solver = routing.solver();
  std::vector<int64> previous_indices;
  int previous_index = data.order_relation.at(0);
  previous_indices.push_back(previous_index);
  for (std::size_t link_index = 1; link_index < data.order_relation.size();
           ++link_index) {
    int current_index = data.order_relation.at(link_index);
    routing.AddPickupAndDelivery(previous_index, current_index);
    IntVar* const previous_active_var = routing.ActiveVar(previous_index);
    IntVar* const active_var          = routing.ActiveVar(current_index);

    IntVar* const previous_vehicle_var = routing.VehicleVar(previous_index);
    IntVar* const vehicle_var          = routing.VehicleVar(current_index);
    routing.NextVar(current_index)->RemoveValues(previous_indices);

    solver->AddConstraint(solver->MakeLessOrEqual(active_var, previous_active_var));
    IntExpr* const isConstraintActive =
        solver->MakeProd(previous_active_var, active_var);
    solver->AddConstraint(solver->MakeEquality(
        solver->MakeProd(isConstraintActive, previous_vehicle_var),
        solver->MakeProd(isConstraintActive, vehicle_var)));
    previous_indices.push_back(current_index);
    previous_index = current_index;
  }
  // [END order constraint]

  // Setting first solution heuristic.
  // [START parameters]
  RoutingSearchParameters searchParameters = DefaultRoutingSearchParameters();
  searchParameters.set_local_search_metaheuristic(
        LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH);
  searchParameters.mutable_time_limit()->set_seconds(1);
  // [END parameters]

  Assignment* assignment = routing.solver()->MakeAssignment();
  routing.CloseModelWithParameters(searchParameters);

  // Generate a route associated to the order relation
  // [START assignement]
  IntVar* previous_var = NULL;// = routing.NextVar(routing.Start(0));
  for (int order_index: data.order_relation) {
    IntVar* next_var = routing.NextVar(order_index);
    if (previous_var != NULL) {
      assignment->Add(previous_var);
      assignment->SetValue(previous_var, order_index);
    }
    previous_var = next_var;
  }
  // [END assignement]

  // The assignment must be valid to solve starting from it
  if (routing.solver()->CheckAssignment(assignment)) {
    // Solve the problem.
    // [START solve]
    const Assignment *solution =
        routing.SolveFromAssignmentWithParameters(assignment, searchParameters);
    // [END solve]
    // Print solution on console.
    // [START print_solution]
    PrintSolution(data, manager, routing, *solution);
    // [END print_solution]
  } else {
    LOG(INFO) << "Unfeasible initial solution";
  }

}
}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::VrpOrder();
  return EXIT_SUCCESS;
}
// [END program]
