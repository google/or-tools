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
#include "ortools/base/logging.h"
#include "ortools/constraint_solver/routing.h"

namespace operations_research {
class DataProblem {
 private:
  std::vector<std::vector<int>> locations_;

 public:
  DataProblem() {
    locations_ = {{4, 4}, {2, 0}, {8, 0}, {0, 1}, {1, 1}, {5, 2},
                  {7, 2}, {3, 3}, {6, 3}, {5, 5}, {8, 5}, {1, 6},
                  {2, 6}, {3, 7}, {6, 7}, {0, 8}, {7, 8}};

    // Compute locations in meters using the block dimension defined as follow
    // Manhattan average block: 750ft x 264ft -> 228m x 80m
    // here we use: 114m x 80m city block
    // src: https://nyti.ms/2GDoRIe "NY Times: Know Your distance"
    std::array<int, 2> cityBlock = {228 / 2, 80};
    for (auto& i : locations_) {
      i[0] = i[0] * cityBlock[0];
      i[1] = i[1] * cityBlock[1];
    }
  }

  std::size_t GetVehicleNumber() const { return 4; }
  const std::vector<std::vector<int>>& GetLocations() const {
    return locations_;
  }
  RoutingModel::NodeIndex GetDepot() const { return RoutingModel::kFirstNode; }
};

/*! @brief Manhattan distance implemented as a callback.
 * @details It uses an array of positions and
 * computes the Manhattan distance between the two positions of two different
 * indices.*/
class ManhattanDistance : public RoutingModel::NodeEvaluator2 {
 private:
  std::vector<std::vector<int64>> distances_;

 public:
  ManhattanDistance(const DataProblem& data) {
    // Precompute distance between location to have distance callback in O(1)
    distances_ = std::vector<std::vector<int64>>(
        data.GetLocations().size(),
        std::vector<int64>(data.GetLocations().size(), 0LL));
    for (std::size_t fromNode = 0; fromNode < data.GetLocations().size();
         fromNode++) {
      for (std::size_t toNode = 0; toNode < data.GetLocations().size();
           toNode++) {
        if (fromNode != toNode)
          distances_[fromNode][toNode] =
              std::abs(data.GetLocations()[toNode][0] -
                       data.GetLocations()[fromNode][0]) +
              std::abs(data.GetLocations()[toNode][1] -
                       data.GetLocations()[fromNode][1]);
      }
    }
  }

  bool IsRepeatable() const override { return true; }

  //! @brief Returns the manhattan distance between the two nodes.
  int64 Run(RoutingModel::NodeIndex FromNode,
            RoutingModel::NodeIndex ToNode) override {
    return distances_[FromNode.value()][ToNode.value()];
  }
};

//! @brief Add distance Dimension.
//! @param[in] data Data of the problem.
//! @param[in, out] routing Routing solver used.
static void AddDistanceDimension(const DataProblem& data,
                                 RoutingModel* routing) {
  std::string distance("Distance");
  routing->AddDimension(new ManhattanDistance(data),
                        0,     // null slack
                        3000,  // maximum distance per vehicle
                        true,  // start cumul to zero
                        distance);
  RoutingDimension* distanceDimension = routing->GetMutableDimension(distance);
  // Try to minimize the max distance among vehicles.
  // /!\ It doesn't mean the standard deviation is minimized
  distanceDimension->SetGlobalSpanCostCoefficient(100);
}

//! @brief Print the solution
//! @param[in] data Data of the problem.
//! @param[in] routing Routing solver used.
//! @param[in] solution Solution found by the solver.
void PrintSolution(const DataProblem& data, const RoutingModel& routing,
                   const Assignment& solution) {
  LOG(INFO) << "Objective: " << solution.ObjectiveValue();
  // Inspect solution.
  for (int i = 0; i < data.GetVehicleNumber(); ++i) {
    int64 index = routing.Start(i);
    LOG(INFO) << "Route for Vehicle " << i << ":";
    int64 distance = 0LL;
    std::stringstream route;
    while (routing.IsEnd(index) == false) {
      route << routing.IndexToNode(index).value() << " -> ";
      int64 previous_index = index;
      index = solution.Value(routing.NextVar(index));
      distance += const_cast<RoutingModel&>(routing).GetArcCostForVehicle(
          previous_index, index, i);
    }
    LOG(INFO) << route.str() << routing.IndexToNode(index).value();
    LOG(INFO) << "Distance of the route: " << distance << "m";
  }
  LOG(INFO) << "";
  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Problem solved in " << routing.solver()->wall_time() << "ms";
}

void Solve() {
  // Instantiate the data problem.
  DataProblem data;

  // Create Routing Model
  RoutingModel routing(data.GetLocations().size(), data.GetVehicleNumber(),
                       data.GetDepot());

  // Define weight of each edge
  ManhattanDistance distance(data);
  routing.SetArcCostEvaluatorOfAllVehicles(
      NewPermanentCallback(&distance, &ManhattanDistance::Run));
  AddDistanceDimension(data, &routing);

  // Setting first solution heuristic (cheapest addition).
  auto searchParameters = RoutingModel::DefaultSearchParameters();
  searchParameters.set_first_solution_strategy(
      FirstSolutionStrategy::PATH_CHEAPEST_ARC);

  const Assignment* solution = routing.SolveWithParameters(searchParameters);
  PrintSolution(data, routing, *solution);
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;
  operations_research::Solve();
  return EXIT_SUCCESS;
}
