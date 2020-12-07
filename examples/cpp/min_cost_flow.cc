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

#include "ortools/graph/min_cost_flow.h"

#include "ortools/base/logging.h"

namespace operations_research {
struct Arc {
  std::pair<NodeIndex, NodeIndex> nodes;
  FlowQuantity capacity;
  FlowQuantity unit_cost;
};

void SolveMinCostFlow() {
  // Define supply of each node.
  const std::vector<std::pair<NodeIndex, FlowQuantity> > supplies = {
      {0, 20}, {1, 0}, {2, 0}, {3, -5}, {4, -15}};

  // Define each arc
  // Can't use std::tuple<NodeIndex, NodeIndex, FlowQuantity>
  // Initialization list is not working on std:tuple cf. N4387
  // Arc are stored as {{begin_node, end_node}, capacity}
  const std::vector<Arc> arcs = {
      {{0, 1}, 15, 4}, {{0, 2}, 8, 4},  {{1, 2}, 20, 2},
      {{1, 3}, 4, 2},  {{1, 4}, 10, 6}, {{2, 3}, 15, 1},
      {{2, 4}, 4, 3},  {{3, 4}, 20, 2}, {{4, 2}, 5, 3}};

  StarGraph graph(supplies.size(), arcs.size());
  MinCostFlow min_cost_flow(&graph);
  for (const auto& it : arcs) {
    ArcIndex arc = graph.AddArc(it.nodes.first, it.nodes.second);
    min_cost_flow.SetArcCapacity(arc, it.capacity);
    min_cost_flow.SetArcUnitCost(arc, it.unit_cost);
  }
  for (const auto& it : supplies) {
    min_cost_flow.SetNodeSupply(it.first, it.second);
  }

  LOG(INFO) << "Solving min cost flow with: " << graph.num_nodes()
            << " nodes, and " << graph.num_arcs() << " arcs.";

  // Find the maximum flow between node 0 and node 4.
  min_cost_flow.Solve();
  if (MinCostFlow::OPTIMAL != min_cost_flow.status()) {
    LOG(FATAL) << "Solving the max flow is not optimal!";
  }
  FlowQuantity total_flow_cost = min_cost_flow.GetOptimalCost();
  LOG(INFO) << "Minimum cost flow: " << total_flow_cost;
  LOG(INFO) << "";
  LOG(INFO) << "Arc   : Flow / Capacity / Cost";
  for (int i = 0; i < arcs.size(); ++i) {
    LOG(INFO) << graph.Tail(i) << " -> " << graph.Head(i) << ": "
              << min_cost_flow.Flow(i) << " / " << min_cost_flow.Capacity(i)
              << " / " << min_cost_flow.UnitCost(i);
  }
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetFlag(&FLAGS_logtostderr, 1);
  operations_research::SolveMinCostFlow();
  return EXIT_SUCCESS;
}
