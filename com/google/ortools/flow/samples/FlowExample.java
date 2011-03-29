// Copyright 2010 Google
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

package com.google.ortools.flow.samples;

import com.google.ortools.flow.StarGraph;
import com.google.ortools.flow.MaxFlow;
import com.google.ortools.flow.MinCostFlow;

/**
 * Sample showing how to model using the flow solver.
 *
 */

public class FlowExample {

  static {
    System.loadLibrary("jniflow");
  }


  private static void solveMinCostFlow() {
    System.out.println("Min Cost Flow Problem");
    final int numSources = 4;
    final int numTargets = 4;
    final int[][] costs = {{90, 75, 75, 80},
                           {35, 85, 55, 65},
                           {125, 95, 90, 105},
                           {45, 110, 95, 115}};
    final int expectedCost = 275;
    StarGraph graph = new StarGraph(numSources + numTargets,
                                    numSources * numTargets);
    MinCostFlow  minCostFlow = new MinCostFlow(graph);
    for (int source = 1; source <= numSources; ++source) {
      for (int target = 1; target <= numTargets; ++target) {
        final long arc = graph.addArc(source, numSources + target);
        minCostFlow.setArcUnitCost(arc, costs[source -  1][target - 1]);
        minCostFlow.setArcCapacity(arc, 1);
      }
    }

    for (int source = 1; source <= numSources; ++source) {
      minCostFlow.setNodeSupply(source, 1);
    }
    for (int target = 1; target <= numTargets; ++target) {
      minCostFlow.setNodeSupply(numSources + target, -1);
    }

    final long totalFlowCost = minCostFlow.computeMinCostFlow();
    System.out.println("total flow = " + totalFlowCost + "/" + expectedCost);
  }

  private static void solveMaxFlow() {
    System.out.println("Max Flow Problem");
    final int numNodes = 6;
    final int numArcs = 9;
    final int[] tails = {1, 1, 1, 1, 2, 3, 4, 4, 5};
    final int[] heads = {2, 3, 4, 5, 4, 5, 5, 6, 6};
    final int[] capacities = {5, 8, 5, 3, 4, 5, 6, 6, 4};
    final int[] expectedFlows = {4, 4, 2, 0, 4, 4, 0, 6, 4};
    final int expectedTotalFlow = 10;
    StarGraph graph = new StarGraph(numNodes, numArcs);
    MaxFlow maxFlow = new MaxFlow(graph, 1, numNodes);
    for (int i = 0; i < numArcs; ++i) {
      final long arc = graph.addArc(tails[i], heads[i]);
      maxFlow.setArcCapacity(arc, capacities[i]);
    }
    final long totalFlow = maxFlow.computeMaxFlow();
    System.out.println("total flow " + totalFlow + "/" + expectedTotalFlow);
    for (int i = 0; i < numArcs; ++i) {
      System.out.println("Arc " + i + ": " + maxFlow.flow(i + 1) + "/" +
                         expectedFlows[i]);
    }
  }

  public static void main(String[] args) throws Exception {
    FlowExample.solveMinCostFlow();
    FlowExample.solveMaxFlow();
  }
}
