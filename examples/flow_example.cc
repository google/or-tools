// Copyright 2010-2011 Google
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

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "graph/ebert_graph.h"
#include "graph/max_flow.h"
#include "graph/min_cost_flow.h"

namespace operations_research {

// ----- Min Cost Flow -----

// Test on a 4x4 matrix. Example taken from
// http://www.ee.oulu.fi/~mpa/matreng/eem1_2-1.htm
void MinCostFlowOn4x4Matrix() {
  LOG(INFO) << "Min Cost Flow on 4x4 Matrix";
  const int kNumSources = 4;
  const int kNumTargets = 4;
  const CostValue kCost[kNumSources][kNumTargets] = {
    { 90, 75, 75, 80 },
    { 35, 85, 55, 65 },
    { 125, 95, 90, 105 },
    { 45, 110, 95, 115 }
  };
  const CostValue kExpectedCost = 275;
  StarGraph graph(kNumSources + kNumTargets, kNumSources * kNumTargets);
  MinCostFlow min_cost_flow(graph);
  for (NodeIndex source = 1; source <= kNumSources; ++source) {
    for (NodeIndex target = 1; target <= kNumTargets; ++target) {
      ArcIndex arc = graph.AddArc(source, kNumSources + target);
      min_cost_flow.SetArcUnitCost(arc, kCost[source -  1][target - 1]);
      min_cost_flow.SetArcCapacity(arc, 1);
    }
  }
  for (NodeIndex source = 1; source <= kNumSources; ++source) {
    min_cost_flow.SetNodeSupply(source, 1);
  }
  for (NodeIndex target = 1; target <= kNumTargets; ++target) {
    min_cost_flow.SetNodeSupply(kNumSources + target, -1);
  }
  CostValue total_flow_cost = min_cost_flow.ComputeMinCostFlow();
  CHECK_EQ(kExpectedCost, total_flow_cost);
}

// ----- Max Flow -----

void MaxFeasibleFlow() {
  LOG(INFO) << "Max Feasible Flow";
  const int kNumNodes = 6;
  const int kNumArcs = 9;
  const NodeIndex kTail[kNumArcs] = { 1, 1, 1, 1, 2, 3, 4, 4, 5 };
  const NodeIndex kHead[kNumArcs] = { 2, 3, 4, 5, 4, 5, 5, 6, 6 };
  const FlowQuantity kCapacity[kNumArcs] = { 5, 8, 5, 3, 4, 5, 6, 6, 4 };
  const FlowQuantity kExpectedFlow[kNumArcs] = { 4, 4, 2, 0, 4, 4, 0, 6, 4 };
  const FlowQuantity kExpectedTotalFlow = 10;
  StarGraph graph(kNumNodes, kNumArcs);
  MaxFlow max_flow(graph, 1, kNumNodes);
  for (int i = 0; i < kNumArcs; ++i) {
    ArcIndex arc = graph.AddArc(kTail[i], kHead[i]);
    max_flow.SetArcCapacity(arc, kCapacity[i]);
  }
  FlowQuantity total_flow = max_flow.ComputeMaxFlow();
  CHECK_EQ(total_flow, kExpectedTotalFlow);
  for (int i = 0; i < kNumArcs; ++i) {
    CHECK_EQ(kExpectedFlow[i], max_flow.Flow(i + 1)) << " i = " << i;
  }
}
}  // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::MinCostFlowOn4x4Matrix();
  operations_research::MaxFeasibleFlow();
  return 0;
}

