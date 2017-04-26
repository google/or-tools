// Copyright 2010-2014 Google
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


#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/graph/ebert_graph.h"
#include "ortools/graph/max_flow.h"
#include "ortools/graph/min_cost_flow.h"

namespace operations_research {

// ----- Min Cost Flow -----

// Test on a 4x4 matrix. Example taken from
// http://www.ee.oulu.fi/~mpa/matreng/eem1_2-1.htm
void MinCostFlowOn4x4Matrix() {
  LOG(INFO) << "Min Cost Flow on 4x4 Matrix";
  const int kNumSources = 4;
  const int kNumTargets = 4;
  const CostValue kCost[kNumSources][kNumTargets] = {{90, 75, 75, 80},
                                                     {35, 85, 55, 65},
                                                     {125, 95, 90, 105},
                                                     {45, 110, 95, 115}};
  const CostValue kExpectedCost = 275;
  StarGraph graph(kNumSources + kNumTargets, kNumSources * kNumTargets);
  MinCostFlow min_cost_flow(&graph);
  for (NodeIndex source = 0; source < kNumSources; ++source) {
    for (NodeIndex target = 0; target < kNumTargets; ++target) {
      ArcIndex arc = graph.AddArc(source, kNumSources + target);
      min_cost_flow.SetArcUnitCost(arc, kCost[source][target]);
      min_cost_flow.SetArcCapacity(arc, 1);
    }
  }
  for (NodeIndex source = 0; source < kNumSources; ++source) {
    min_cost_flow.SetNodeSupply(source, 1);
  }
  for (NodeIndex target = 0; target < kNumTargets; ++target) {
    min_cost_flow.SetNodeSupply(kNumSources + target, -1);
  }
  CHECK(min_cost_flow.Solve());
  CHECK_EQ(MinCostFlow::OPTIMAL, min_cost_flow.status());
  CostValue total_flow_cost = min_cost_flow.GetOptimalCost();
  CHECK_EQ(kExpectedCost, total_flow_cost);
}

// ----- Max Flow -----

void MaxFeasibleFlow() {
  LOG(INFO) << "Max Feasible Flow";
  const int kNumNodes = 6;
  const int kNumArcs = 9;
  const NodeIndex kTail[kNumArcs] = {0, 0, 0, 0, 1, 2, 3, 3, 4};
  const NodeIndex kHead[kNumArcs] = {1, 2, 3, 4, 3, 4, 4, 5, 5};
  const FlowQuantity kCapacity[kNumArcs] = {5, 8, 5, 3, 4, 5, 6, 6, 4};
  const FlowQuantity kExpectedFlow[kNumArcs] = {1, 1, 5, 3, 1, 1, 0, 6, 4};
  const FlowQuantity kExpectedTotalFlow = 10;
  StarGraph graph(kNumNodes, kNumArcs);
  MaxFlow max_flow(&graph, 0, kNumNodes - 1);
  for (int i = 0; i < kNumArcs; ++i) {
    ArcIndex arc = graph.AddArc(kTail[i], kHead[i]);
    max_flow.SetArcCapacity(arc, kCapacity[i]);
  }
  CHECK(max_flow.Solve());
  CHECK_EQ(MaxFlow::OPTIMAL, max_flow.status());
  FlowQuantity total_flow = max_flow.GetOptimalFlow();
  CHECK_EQ(total_flow, kExpectedTotalFlow);
  for (int i = 0; i < kNumArcs; ++i) {
    CHECK_EQ(kExpectedFlow[i], max_flow.Flow(i)) << " i = " << i;
  }
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  operations_research::MinCostFlowOn4x4Matrix();
  operations_research::MaxFeasibleFlow();
  return 0;
}
