// Copyright 2018 Google LLC
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

#include <cmath>
#include <vector>
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"

namespace operations_research {
struct DataProblem {
  DataProblem()
      : locations({{4, 4},
                   {2, 0},
                   {8, 0},
                   {0, 1},
                   {1, 1},
                   {5, 2},
                   {7, 2},
                   {3, 3},
                   {6, 3},
                   {5, 5},
                   {8, 5},
                   {1, 6},
                   {2, 6},
                   {3, 7},
                   {6, 7},
                   {0, 8},
                   {7, 8}}),
        num_locations(locations.size()),
        num_vehicles(1),
        depot(0) {
    // Convert locations in meters using a city block dimension of 114m x 80m.
    for (auto& it : locations) {
      const_cast<std::vector<int>&>(it)[0] *= 114;
      const_cast<std::vector<int>&>(it)[1] *= 80;
    }
  }
  const std::vector<std::vector<int>> locations;
  const int num_locations;
  const int num_vehicles;
  const RoutingIndexManager::NodeIndex depot;
};

/*! @brief Manhattan distance implemented as a callback.
 * @details It uses an array of positions and
 * computes the Manhattan distance between the two positions of two different
 * indices.*/
class ManhattanDistance {
 private:
  std::vector<std::vector<int64>> distances_;

 public:
  ManhattanDistance(const DataProblem& data) {
    // Precompute distance between location to have distance callback in O(1)
    distances_ = std::vector<std::vector<int64>>(
        data.num_locations, std::vector<int64>(data.num_locations, 0LL));
    for (int fromNode = 0; fromNode < data.num_locations; fromNode++) {
      for (int toNode = 0; toNode < data.num_locations; toNode++) {
        if (fromNode != toNode)
          distances_[fromNode][toNode] =
              std::abs(data.locations[toNode][0] -
                       data.locations[fromNode][0]) +
              std::abs(data.locations[toNode][1] - data.locations[fromNode][1]);
      }
    }
  }

  //! @brief Returns the manhattan distance between the two nodes.
  int64 operator()(RoutingIndexManager::NodeIndex FromNode,
                   RoutingIndexManager::NodeIndex ToNode) {
    return distances_[FromNode.value()][ToNode.value()];
  }
};

//! @brief Print the solution
//! @param[in] data Data of the problem.
//! @param[in] manager Index manager used.
//! @param[in] routing Routing solver used.
//! @param[in] solution Solution found by the solver.
void PrintSolution(const DataProblem& data, const RoutingIndexManager& manager,
                   const RoutingModel& routing, const Assignment& solution) {
  LOG(INFO) << "Objective: " << solution.ObjectiveValue();
  // Inspect solution.
  int64 index = routing.Start(0);
  LOG(INFO) << "Route for Vehicle 0:";
  int64 distance = 0LL;
  std::stringstream route;
  while (routing.IsEnd(index) == false) {
    route << manager.IndexToNode(index).value() << " -> ";
    int64 previous_index = index;
    index = solution.Value(routing.NextVar(index));
    distance += const_cast<RoutingModel&>(routing).GetArcCostForVehicle(
        previous_index, index, 0LL);
  }
  LOG(INFO) << route.str() << manager.IndexToNode(index).value();
  LOG(INFO) << "Distance of the route: " << distance << "m";
  LOG(INFO) << "";
  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Problem solved in " << routing.solver()->wall_time() << "ms";
}

void Solve() {
  // Instantiate the data problem.
  DataProblem data;

  // Create Routing Index Manager & Routing Model
  RoutingIndexManager manager(data.num_locations, data.num_vehicles,
                              data.depot);
  RoutingModel routing(manager);

  // Define weight of each edge
  ManhattanDistance distance(data);
  const int vehicle_cost = routing.RegisterTransitCallback(
      [&distance, &manager](int64 fromIndex, int64 toIndex) -> int64 {
        return distance(manager.IndexToNode(fromIndex),
                        manager.IndexToNode(toIndex));
      });
  routing.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);

  // Setting first solution heuristic (cheapest addition).
  RoutingSearchParameters searchParameters = DefaultRoutingSearchParameters();
  searchParameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);

  const Assignment* solution = routing.SolveWithParameters(searchParameters);
  PrintSolution(data, manager, routing, *solution);
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;
  operations_research::Solve();
  return 0;
}
