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
// From Taha 'Introduction to Operations Research', example 6.4-2."""
// [START import]
#include <cstdint>

#include "ortools/graph/max_flow.h"
// [END import]

namespace operations_research {
// MaxFlow simple interface example.
void SimpleMaxFlowProgram() {
  // [START solver]
  // Instantiate a SimpleMaxFlow solver.
  SimpleMaxFlow max_flow;
  // [END solver]

  // [START data]
  // Define three parallel arrays: start_nodes, end_nodes, and the capacities
  // between each pair. For instance, the arc from node 0 to node 1 has a
  // capacity of 20.
  std::vector<int64_t> start_nodes = {0, 0, 0, 1, 1, 2, 2, 3, 3};
  std::vector<int64_t> end_nodes = {1, 2, 3, 2, 4, 3, 4, 2, 4};
  std::vector<int64_t> capacities = {20, 30, 10, 40, 30, 10, 20, 5, 20};
  // [END data]

  // [START constraints]
  // Add each arc.
  for (int i = 0; i < start_nodes.size(); ++i) {
    max_flow.AddArcWithCapacity(start_nodes[i], end_nodes[i], capacities[i]);
  }
  // [END constraints]

  // [START solve]
  // Find the maximum flow between node 0 and node 4.
  int status = max_flow.Solve(0, 4);
  // [END solve]

  // [START print_solution]
  if (status == MaxFlow::OPTIMAL) {
    LOG(INFO) << "Max flow: " << max_flow.OptimalFlow();
    LOG(INFO) << "";
    LOG(INFO) << "  Arc    Flow / Capacity";
    for (std::size_t i = 0; i < max_flow.NumArcs(); ++i) {
      LOG(INFO) << max_flow.Tail(i) << " -> " << max_flow.Head(i) << "  "
                << max_flow.Flow(i) << "  / " << max_flow.Capacity(i);
    }
  } else {
    LOG(INFO) << "Solving the max flow problem failed. Solver status: "
              << status;
  }
  // [END print_solution]
}

}  // namespace operations_research

int main() {
  operations_research::SimpleMaxFlowProgram();
  return EXIT_SUCCESS;
}
// [END program]
