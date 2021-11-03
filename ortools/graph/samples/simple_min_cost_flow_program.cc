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
// From Bradley, Hax and Maganti, 'Applied Mathematical Programming', figure 8.1
// [START import]
#include <cstdint>

#include "ortools/graph/min_cost_flow.h"
// [END import]

namespace operations_research {
// MinCostFlow simple interface example.
void SimpleMinCostFlowProgram() {
  // [START solver]
  // Instantiate a SimpleMinCostFlow solver.
  SimpleMinCostFlow min_cost_flow;
  // [END solver]

  // [START data]
  // Define four parallel arrays: sources, destinations, capacities,
  // and unit costs between each pair. For instance, the arc from node 0
  // to node 1 has a capacity of 15.
  std::vector<int64_t> start_nodes = {0, 0, 1, 1, 1, 2, 2, 3, 4};
  std::vector<int64_t> end_nodes = {1, 2, 2, 3, 4, 3, 4, 4, 2};
  std::vector<int64_t> capacities = {15, 8, 20, 4, 10, 15, 4, 20, 5};
  std::vector<int64_t> unit_costs = {4, 4, 2, 2, 6, 1, 3, 2, 3};

  // Define an array of supplies at each node.
  std::vector<int64_t> supplies = {20, 0, 0, -5, -15};
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
    LOG(INFO) << "Minimum cost flow: " << min_cost_flow.OptimalCost();
    LOG(INFO) << "";
    LOG(INFO) << " Arc   Flow / Capacity  Cost";
    for (std::size_t i = 0; i < min_cost_flow.NumArcs(); ++i) {
      int64_t cost = min_cost_flow.Flow(i) * min_cost_flow.UnitCost(i);
      LOG(INFO) << min_cost_flow.Tail(i) << " -> " << min_cost_flow.Head(i)
                << "  " << min_cost_flow.Flow(i) << "  / "
                << min_cost_flow.Capacity(i) << "       " << cost;
    }
  } else {
    LOG(INFO) << "Solving the min cost flow problem failed. Solver status: "
              << status;
  }
  // [END print_solution]
}

}  // namespace operations_research

int main() {
  operations_research::SimpleMinCostFlowProgram();
  return EXIT_SUCCESS;
}
// [END program]
