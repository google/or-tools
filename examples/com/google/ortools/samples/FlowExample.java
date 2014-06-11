// Copyright 2010-2012 Google
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

package com.google.ortools.samples;

import com.google.ortools.graph.MaxFlow;
import com.google.ortools.graph.MinCostFlow;
import com.google.ortools.graph.StarGraph;

/**
 * Sample showing how to model using the flow solver.
 *
 */

public class FlowExample {

  static {
    System.loadLibrary("jniortools");
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
    for (int source = 0; source < numSources; ++source) {
      for (int target = 0; target < numTargets; ++target) {
        final int arc = graph.addArc(source, numSources + target);
        minCostFlow.setArcUnitCost(arc, costs[source][target]);
        minCostFlow.setArcCapacity(arc, 1);
      }
    }

    for (int source = 0; source < numSources; ++source) {
      minCostFlow.setNodeSupply(source, 1);
    }
    for (int target = 0; target < numTargets; ++target) {
      minCostFlow.setNodeSupply(numSources + target, -1);
    }
    if (minCostFlow.solve()) {
      final long totalFlowCost = minCostFlow.getOptimalCost();
      System.out.println("total flow = " + totalFlowCost + "/" + expectedCost);
    } else {
      System.out.println("No solution found");
    }
  }

  private static void solveMaxFlow() {
    System.out.println("Max Flow Problem");
    final int numNodes = 6;
    final int numArcs = 9;
    final int[] tails = {0, 0, 0, 0, 1, 2, 3, 3, 4};
    final int[] heads = {1, 2, 3, 4, 3, 4, 4, 5, 5};
    final int[] capacities = {5, 8, 5, 3, 4, 5, 6, 6, 4};
    final int[] expectedFlows = {4, 4, 2, 0, 4, 4, 0, 6, 4};
    final int expectedTotalFlow = 10;
    StarGraph graph = new StarGraph(numNodes, numArcs);
    MaxFlow maxFlow = new MaxFlow(graph, 0, numNodes - 1);
    for (int i = 0; i < numArcs; ++i) {
      final int arc = graph.addArc(tails[i], heads[i]);
      maxFlow.setArcCapacity(arc, capacities[i]);
    }
    if (maxFlow.solve()) {
      final long totalFlow = maxFlow.getOptimalFlow();
      System.out.println("total flow " + totalFlow + "/" + expectedTotalFlow);
      for (int i = 0; i < numArcs; ++i) {
        System.out.println("Arc " + i + ": " + maxFlow.flow(i) + "/" +
                           expectedFlows[i]);
      }
    } else {
      System.out.println("No solution found");
    }
  }

  public static void main(String[] args) throws Exception {
    FlowExample.solveMinCostFlow();
    FlowExample.solveMaxFlow();
  }
}
