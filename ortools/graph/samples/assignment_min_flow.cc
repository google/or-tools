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
#include <cstdint>

#include "ortools/graph/min_cost_flow.h"
// [END import]

namespace operations_research {
// MinCostFlow simple interface example.
void AssignmentMinFlow() {
  // [START solver]
  // Instantiate a SimpleMinCostFlow solver.
  SimpleMinCostFlow min_cost_flow;
  // [END solver]

  // [START data]
  // Define four parallel arrays: sources, destinations, capacities,
  // and unit costs between each pair. For instance, the arc from node 0
  // to node 1 has a capacity of 15.
  const std::vector<int64_t> start_nodes = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                            3, 3, 3, 3, 4, 4, 4, 4, 5, 6, 7, 8};
  const std::vector<int64_t> end_nodes = {1, 2, 3, 4, 5, 6, 7, 8, 5, 6, 7, 8,
                                          5, 6, 7, 8, 5, 6, 7, 8, 9, 9, 9, 9};
  const std::vector<int64_t> capacities = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                                           1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  const std::vector<int64_t> unit_costs = {0,  0,   0,  0,   90,  76, 75, 70,
                                           35, 85,  55, 65,  125, 95, 90, 105,
                                           45, 110, 95, 115, 0,   0,  0,  0};

  const int64_t source = 0;
  const int64_t sink = 9;
  const int64_t tasks = 4;
  // Define an array of supplies at each node.
  const std::vector<int64_t> supplies = {tasks, 0, 0, 0, 0, 0, 0, 0, 0, -tasks};
  // [END data]

  // [START constraints]
  // Add each arc.
  for (int i = 0; i < start_nodes.size(); ++i) {
    int arc = min_cost_flow.AddArcWithCapacityAndUnitCost(
        start_nodes[i], end_nodes[i], capacities[i], unit_costs[i]);
    if (arc != i) LOG(FATAL) << "Internal error";
  }

  // Add node supplies.
  for (int i = 0; i < supplies.size(); ++i) {
    min_cost_flow.SetNodeSupply(i, supplies[i]);
  }
  // [END constraints]

  // [START solve]
  // Find the min cost flow.
  int status = min_cost_flow.Solve();
  // [END solve]

  // [START print_solution]
  if (status == MinCostFlow::OPTIMAL) {
    LOG(INFO) << "Total cost: " << min_cost_flow.OptimalCost();
    LOG(INFO) << "";
    for (std::size_t i = 0; i < min_cost_flow.NumArcs(); ++i) {
      // Can ignore arcs leading out of source or into sink.
      if (min_cost_flow.Tail(i) != source && min_cost_flow.Head(i) != sink) {
        // Arcs in the solution have a flow value of 1. Their start and end
        // nodes give an assignment of worker to task.
        if (min_cost_flow.Flow(i) > 0) {
          LOG(INFO) << "Worker " << min_cost_flow.Tail(i)
                    << " assigned to task " << min_cost_flow.Head(i)
                    << " Cost: " << min_cost_flow.UnitCost(i);
        }
      }
    }
  } else {
    LOG(INFO) << "Solving the min cost flow problem failed.";
    LOG(INFO) << "Solver status: " << status;
  }
  // [END print_solution]
}

}  // namespace operations_research

int main() {
  operations_research::AssignmentMinFlow();
  return EXIT_SUCCESS;
}
// [END program]
