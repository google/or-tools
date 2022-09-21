// Copyright 2010-2022 Google LLC
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
#include <sstream>
#include <vector>

#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"
// [END import]

namespace operations_research {
// [START data_model]
struct DataModel {
  const std::vector<std::vector<int64_t>> distance_matrix{
      {0, 2451, 713, 1018, 1631, 1374, 2408, 213, 2571, 875, 1420, 2145, 1972},
      {2451, 0, 1745, 1524, 831, 1240, 959, 2596, 403, 1589, 1374, 357, 579},
      {713, 1745, 0, 355, 920, 803, 1737, 851, 1858, 262, 940, 1453, 1260},
      {1018, 1524, 355, 0, 700, 862, 1395, 1123, 1584, 466, 1056, 1280, 987},
      {1631, 831, 920, 700, 0, 663, 1021, 1769, 949, 796, 879, 586, 371},
      {1374, 1240, 803, 862, 663, 0, 1681, 1551, 1765, 547, 225, 887, 999},
      {2408, 959, 1737, 1395, 1021, 1681, 0, 2493, 678, 1724, 1891, 1114, 701},
      {213, 2596, 851, 1123, 1769, 1551, 2493, 0, 2699, 1038, 1605, 2300, 2099},
      {2571, 403, 1858, 1584, 949, 1765, 678, 2699, 0, 1744, 1645, 653, 600},
      {875, 1589, 262, 466, 796, 547, 1724, 1038, 1744, 0, 679, 1272, 1162},
      {1420, 1374, 940, 1056, 879, 225, 1891, 1605, 1645, 679, 0, 1017, 1200},
      {2145, 357, 1453, 1280, 586, 887, 1114, 2300, 653, 1272, 1017, 0, 504},
      {1972, 579, 1260, 987, 371, 999, 701, 2099, 600, 1162, 1200, 504, 0},
  };
  const int num_vehicles = 1;
  const RoutingIndexManager::NodeIndex depot{0};
};
// [END data_model]

// [START solution_printer]
void PrintSolution(
    const std::vector<std::vector<RoutingIndexManager::NodeIndex>>& routes) {
  // Print routes.
  DataModel data;
  int64_t total_distance = 0;
  for (int i = 0; i < routes.size(); ++i) {
    std::vector<RoutingIndexManager::NodeIndex> route = routes[i];
    int64_t route_distance{0};
    std::stringstream route_text;
    LOG(INFO) << "Route for Vehicle " << i << ":";
    route_text << route[0];
    for (int j = 1; j < route.size(); ++j) {
      route_text << " -> " << route[j];
      route_distance +=
          data.distance_matrix[route[j - 1].value()][route[j].value()];
    }
    LOG(INFO) << route_text.str();
    LOG(INFO) << "Distance of the route: " << route_distance << "m";
    total_distance += route_distance;
  }
  LOG(INFO) << "Total distance of all routes: " << total_distance << "m";
}
// [END solution_printer]

void Tsp() {
  // Instantiate the data problem.
  // [START data]
  DataModel data;
  // [END data]

  // Create Routing Index Manager
  // [START index_manager]
  RoutingIndexManager manager(data.distance_matrix.size(), data.num_vehicles,
                              data.depot);
  // [END index_manager]

  // Create Routing Model.
  // [START routing_model]
  RoutingModel routing(manager);
  // [END routing_model]

  // Define cost of each arc.
  // [START arc_cost]
  const int transit_callback_index = routing.RegisterTransitCallback(
      [&data, &manager](int64_t from_index, int64_t to_index) -> int64_t {
        // Convert from routing variable Index to distance matrix NodeIndex.
        auto from_node = manager.IndexToNode(from_index).value();
        auto to_node = manager.IndexToNode(to_index).value();
        return data.distance_matrix[from_node][to_node];
      });
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

  // [START get_routes]
  // Get the routes and convert indices to nodes.
  std::vector<std::vector<RoutingIndexManager::NodeIndex>> routes;
  for (const std::vector<int64_t>& route_indices :
       routing.GetRoutesFromAssignment(*solution)) {
    routes.push_back(manager.IndicesToNodes(route_indices));
  }
  // [END get_routes]

  // Print solution on console.
  PrintSolution(routes);
  // [END print_solution]
}

}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::Tsp();
  return EXIT_SUCCESS;
}
// [END program]
