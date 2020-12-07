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

#include "ortools/graph/max_flow.h"

#include "ortools/base/logging.h"

namespace operations_research {
void SolveMaxFlow() {
  const int num_nodes = 5;
  // Add each arc
  // Can't use std::tuple<NodeIndex, NodeIndex, FlowQuantity>
  // Initialization list is not working on std:tuple cf. N4387
  // Arc are stored as {{begin_node, end_node}, capacity}
  std::vector<std::pair<std::pair<NodeIndex, NodeIndex>, FlowQuantity> > arcs =
      {{{0, 1}, 20}, {{0, 2}, 30}, {{0, 3}, 10}, {{1, 2}, 40}, {{1, 4}, 30},
       {{2, 3}, 10}, {{2, 4}, 20}, {{3, 2}, 5},  {{3, 4}, 20}};
  StarGraph graph(num_nodes, arcs.size());
  MaxFlow max_flow(&graph, 0, num_nodes - 1);
  for (const auto& it : arcs) {
    ArcIndex arc = graph.AddArc(it.first.first, it.first.second);
    max_flow.SetArcCapacity(arc, it.second);
  }

  LOG(INFO) << "Solving max flow with: " << graph.num_nodes() << " nodes, and "
            << graph.num_arcs() << " arcs.";

  // Find the maximum flow between node 0 and node 4.
  max_flow.Solve();
  if (MaxFlow::OPTIMAL != max_flow.status()) {
    LOG(FATAL) << "Solving the max flow is not optimal!";
  }
  FlowQuantity total_flow = max_flow.GetOptimalFlow();
  LOG(INFO) << "Maximum flow: " << total_flow;
  LOG(INFO) << "";
  LOG(INFO) << " Arc  : Flow / Capacity";
  for (int i = 0; i < arcs.size(); ++i) {
    LOG(INFO) << graph.Tail(i) << " -> " << graph.Head(i) << ": "
              << max_flow.Flow(i) << " / " << max_flow.Capacity(i);
  }
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetFlag(&FLAGS_logtostderr, 1);
  operations_research::SolveMaxFlow();
  return EXIT_SUCCESS;
}
