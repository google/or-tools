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

// [START program]
// [START import]
#include <cmath>
#include <cstdint>

#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"
// [END import]

namespace operations_research {

void SimpleRoutingProgram() {
  // Instantiate the data problem.
  // [START data]
  int num_location = 5;
  int num_vehicles = 1;
  RoutingIndexManager::NodeIndex depot{0};
  // [END data]

  // Create Routing Index Manager
  // [START index_manager]
  RoutingIndexManager manager(num_location, num_vehicles, depot);
  // [END index_manager]

  // Create Routing Model.
  // [START routing_model]
  RoutingModel routing(manager);
  // [END routing_model]

  // Define cost of each arc.
  // [START arc_cost]
  int distance_call_index = routing.RegisterTransitCallback(
      [&manager](int64_t from_index, int64_t to_index) -> int64_t {
        // Convert from routing variable Index to user NodeIndex.
        auto from_node = manager.IndexToNode(from_index).value();
        auto to_node = manager.IndexToNode(to_index).value();
        return std::abs(to_node - from_node);
      });
  routing.SetArcCostEvaluatorOfAllVehicles(distance_call_index);
  // [END arc_cost]

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
  LOG(INFO) << "Objective: " << solution->ObjectiveValue();
  // Inspect solution.
  int64_t index = routing.Start(0);
  LOG(INFO) << "Route for Vehicle 0:";
  int64_t route_distance{0};
  std::ostringstream route;
  while (routing.IsEnd(index) == false) {
    route << manager.IndexToNode(index).value() << " -> ";
    int64_t previous_index = index;
    index = solution->Value(routing.NextVar(index));
    route_distance +=
        routing.GetArcCostForVehicle(previous_index, index, int64_t{0});
  }
  LOG(INFO) << route.str() << manager.IndexToNode(index).value();
  LOG(INFO) << "Distance of the route: " << route_distance << "m";
  // [END print_solution]
}

}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::SimpleRoutingProgram();
  return EXIT_SUCCESS;
}
// [END program]
