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
      {288, 149}, {288, 129}, {270, 133}, {256, 141}, {256, 157}, {246, 157},
      {236, 169}, {228, 169}, {228, 161}, {220, 169}, {212, 169}, {204, 169},
      {196, 169}, {188, 169}, {196, 161}, {188, 145}, {172, 145}, {164, 145},
      {156, 145}, {148, 145}, {140, 145}, {148, 169}, {164, 169}, {172, 169},
      {156, 169}, {140, 169}, {132, 169}, {124, 169}, {116, 161}, {104, 153},
      {104, 161}, {104, 169}, {90, 165},  {80, 157},  {64, 157},  {64, 165},
      {56, 169},  {56, 161},  {56, 153},  {56, 145},  {56, 137},  {56, 129},
      {56, 121},  {40, 121},  {40, 129},  {40, 137},  {40, 145},  {40, 153},
      {40, 161},  {40, 169},  {32, 169},  {32, 161},  {32, 153},  {32, 145},
      {32, 137},  {32, 129},  {32, 121},  {32, 113},  {40, 113},  {56, 113},
      {56, 105},  {48, 99},   {40, 99},   {32, 97},   {32, 89},   {24, 89},
      {16, 97},   {16, 109},  {8, 109},   {8, 97},    {8, 89},    {8, 81},
      {8, 73},    {8, 65},    {8, 57},    {16, 57},   {8, 49},    {8, 41},
      {24, 45},   {32, 41},   {32, 49},   {32, 57},   {32, 65},   {32, 73},
      {32, 81},   {40, 83},   {40, 73},   {40, 63},   {40, 51},   {44, 43},
      {44, 35},   {44, 27},   {32, 25},   {24, 25},   {16, 25},   {16, 17},
      {24, 17},   {32, 17},   {44, 11},   {56, 9},    {56, 17},   {56, 25},
      {56, 33},   {56, 41},   {64, 41},   {72, 41},   {72, 49},   {56, 49},
      {48, 51},   {56, 57},   {56, 65},   {48, 63},   {48, 73},   {56, 73},
      {56, 81},   {48, 83},   {56, 89},   {56, 97},   {104, 97},  {104, 105},
      {104, 113}, {104, 121}, {104, 129}, {104, 137}, {104, 145}, {116, 145},
      {124, 145}, {132, 145}, {132, 137}, {140, 137}, {148, 137}, {156, 137},
      {164, 137}, {172, 125}, {172, 117}, {172, 109}, {172, 101}, {172, 93},
      {172, 85},  {180, 85},  {180, 77},  {180, 69},  {180, 61},  {180, 53},
      {172, 53},  {172, 61},  {172, 69},  {172, 77},  {164, 81},  {148, 85},
      {124, 85},  {124, 93},  {124, 109}, {124, 125}, {124, 117}, {124, 101},
      {104, 89},  {104, 81},  {104, 73},  {104, 65},  {104, 49},  {104, 41},
      {104, 33},  {104, 25},  {104, 17},  {92, 9},    {80, 9},    {72, 9},
      {64, 21},   {72, 25},   {80, 25},   {80, 25},   {80, 41},   {88, 49},
      {104, 57},  {124, 69},  {124, 77},  {132, 81},  {140, 65},  {132, 61},
      {124, 61},  {124, 53},  {124, 45},  {124, 37},  {124, 29},  {132, 21},
      {124, 21},  {120, 9},   {128, 9},   {136, 9},   {148, 9},   {162, 9},
      {156, 25},  {172, 21},  {180, 21},  {180, 29},  {172, 29},  {172, 37},
      {172, 45},  {180, 45},  {180, 37},  {188, 41},  {196, 49},  {204, 57},
      {212, 65},  {220, 73},  {228, 69},  {228, 77},  {236, 77},  {236, 69},
      {236, 61},  {228, 61},  {228, 53},  {236, 53},  {236, 45},  {228, 45},
      {228, 37},  {236, 37},  {236, 29},  {228, 29},  {228, 21},  {236, 21},
      {252, 21},  {260, 29},  {260, 37},  {260, 45},  {260, 53},  {260, 61},
      {260, 69},  {260, 77},  {276, 77},  {276, 69},  {276, 61},  {276, 53},
      {284, 53},  {284, 61},  {284, 69},  {284, 77},  {284, 85},  {284, 93},
      {284, 101}, {288, 109}, {280, 109}, {276, 101}, {276, 93},  {276, 85},
      {268, 97},  {260, 109}, {252, 101}, {260, 93},  {260, 85},  {236, 85},
      {228, 85},  {228, 93},  {236, 93},  {236, 101}, {228, 101}, {228, 109},
      {228, 117}, {228, 125}, {220, 125}, {212, 117}, {204, 109}, {196, 101},
      {188, 93},  {180, 93},  {180, 101}, {180, 109}, {180, 117}, {180, 125},
      {196, 145}, {204, 145}, {212, 145}, {220, 145}, {228, 145}, {236, 145},
      {246, 141}, {252, 125}, {260, 129}, {280, 133},
  };
  const int num_vehicles = 1;
  const RoutingIndexManager::NodeIndex depot{0};
};
// [END data_model]

// [START distance_matrix]
// @brief Generate distance matrix.
std::vector<std::vector<int64_t>> ComputeEuclideanDistanceMatrix(
    const std::vector<std::vector<int>>& locations) {
  std::vector<std::vector<int64_t>> distances =
      std::vector<std::vector<int64_t>>(
          locations.size(), std::vector<int64_t>(locations.size(), int64_t{0}));
  for (int fromNode = 0; fromNode < locations.size(); fromNode++) {
    for (int toNode = 0; toNode < locations.size(); toNode++) {
      if (fromNode != toNode)
        distances[fromNode][toNode] = static_cast<int64_t>(
            std::hypot((locations[toNode][0] - locations[fromNode][0]),
                       (locations[toNode][1] - locations[fromNode][1])));
    }
  }
  return distances;
}
// [END distance_matrix]

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
  LOG(INFO) << "Route:";
  int64_t distance{0};
  std::stringstream route;
  while (routing.IsEnd(index) == false) {
    route << manager.IndexToNode(index).value() << " -> ";
    int64_t previous_index = index;
    index = solution.Value(routing.NextVar(index));
    distance += routing.GetArcCostForVehicle(previous_index, index, int64_t{0});
  }
  LOG(INFO) << route.str() << manager.IndexToNode(index).value();
  LOG(INFO) << "Route distance: " << distance << "miles";
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

  // [START transit_callback]
  const auto distance_matrix = ComputeEuclideanDistanceMatrix(data.locations);
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
