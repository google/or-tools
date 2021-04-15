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
#include <vector>

#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"
// [END import]

namespace operations_research {
// [START data_model]
struct DataModel {
  const std::vector<std::vector<int>> locations{
      {4, 4}, {2, 0}, {8, 0}, {0, 1}, {1, 1}, {5, 2}, {7, 2}, {3, 3}, {6, 3},
      {5, 5}, {8, 5}, {1, 6}, {2, 6}, {3, 7}, {6, 7}, {0, 8}, {7, 8},
  };
  const int num_vehicles = 1;
  const RoutingIndexManager::NodeIndex depot{0};
  DataModel() {
    // Convert locations in meters using a city block dimension of 114m x 80m.
    for (auto& it : const_cast<std::vector<std::vector<int>>&>(locations)) {
      it[0] *= 114;
      it[1] *= 80;
    }
  }
};
// [END data_model]

// [START manhattan_distance_matrix]
/*! @brief Generate Manhattan distance matrix.
 * @details It uses the data.locations to computes the Manhattan distance
 * between the two positions of two different indices.*/
std::vector<std::vector<int64_t>> GenerateManhattanDistanceMatrix(
    const std::vector<std::vector<int>>& locations) {
  std::vector<std::vector<int64_t>> distances =
      std::vector<std::vector<int64_t>>(
          locations.size(), std::vector<int64_t>(locations.size(), int64_t{0}));
  for (int fromNode = 0; fromNode < locations.size(); fromNode++) {
    for (int toNode = 0; toNode < locations.size(); toNode++) {
      if (fromNode != toNode)
        distances[fromNode][toNode] =
            int64_t{std::abs(locations[toNode][0] - locations[fromNode][0]) +
                    std::abs(locations[toNode][1] - locations[fromNode][1])};
    }
  }
  return distances;
}
// [END manhattan_distance_matrix]

// [START solution_printer]
//! @brief Print the solution
//! @param[in] manager Index manager used.
//! @param[in] routing Routing solver used.
//! @param[in] solution Solution found by the solver.
void PrintSolution(const RoutingIndexManager& manager,
                   const RoutingModel& routing, const Assignment& solution) {
  LOG(INFO) << "Objective: " << solution.ObjectiveValue();
  // Inspect solution.
  int64_t index = routing.Start(0);
  LOG(INFO) << "Route for Vehicle 0:";
  int64_t distance{0};
  std::stringstream route;
  while (routing.IsEnd(index) == false) {
    route << manager.IndexToNode(index).value() << " -> ";
    int64_t previous_index = index;
    index = solution.Value(routing.NextVar(index));
    distance += routing.GetArcCostForVehicle(previous_index, index, int64_t{0});
  }
  LOG(INFO) << route.str() << manager.IndexToNode(index).value();
  LOG(INFO) << "Distance of the route: " << distance << "m";
  LOG(INFO) << "";
  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Problem solved in " << routing.solver()->wall_time() << "ms";
}
// [END solution_printer]

void Tsp() {
  // Instantiate the data problem.
  // [START data]
  DataModel data;
  // [END data]

  // Create Routing Index Manager
  // [START index_manager]
  RoutingIndexManager manager(data.locations.size(), data.num_vehicles,
                              data.depot);
  // [END index_manager]

  // Create Routing Model.
  // [START routing_model]
  RoutingModel routing(manager);
  // [END routing_model]

  // Create and register a transit callback.
  // [START transit_callback]
  const auto distance_matrix = GenerateManhattanDistanceMatrix(data.locations);
  const int transit_callback_index = routing.RegisterTransitCallback(
      [&distance_matrix, &manager](int64_t from_index,
                                   int64_t to_index) -> int64_t {
        // Convert from routing variable Index to distance matrix NodeIndex.
        auto from_node = manager.IndexToNode(from_index).value();
        auto to_node = manager.IndexToNode(to_index).value();
        return distance_matrix[from_node][to_node];
      });
  // [END transit_callback]

  // Define cost of each arc.
  // [START arc_cost]
  routing.SetArcCostEvaluatorOfAllVehicles(transit_callback_index);
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
  PrintSolution(manager, routing, *solution);
  // [END print_solution]
}

}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::Tsp();
  return EXIT_SUCCESS;
}
// [END program]
