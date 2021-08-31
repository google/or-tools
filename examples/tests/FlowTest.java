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

package com.google.ortools.graph;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;

import com.google.ortools.Loader;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Test the Min/Max Flow solver java interface. */
public final class FlowTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testMinCostFlow() {
    final int numSources = 4;
    final int numTargets = 4;
    final int[][] costs = {{90, 75, 75, 80},
                           {35, 85, 55, 65},
                           {125, 95, 90, 105},
                           {45, 110, 95, 115}};
    final int expectedCost = 275;
    final MinCostFlow minCostFlow = new MinCostFlow();
    assertNotNull(minCostFlow);
    for (int source = 0; source < numSources; ++source) {
      for (int target = 0; target < numTargets; ++target) {
        minCostFlow.addArcWithCapacityAndUnitCost(
            source, numSources + target, 1, costs[source][target]);
      }
    }

    for (int source = 0; source < numSources; ++source) {
      minCostFlow.setNodeSupply(source, 1);
    }
    for (int target = 0; target < numTargets; ++target) {
      minCostFlow.setNodeSupply(numSources + target, -1);
    }
    final MinCostFlowBase.Status solveStatus = minCostFlow.solve();
    assertEquals(solveStatus, MinCostFlow.Status.OPTIMAL);
    final long totalFlowCost = minCostFlow.getOptimalCost();
    assertEquals(expectedCost, totalFlowCost);
  }

  @Test
  public void testMaxFlow() {
    final int numNodes = 6;
    final int numArcs = 9;
    final int[] tails = {0, 0, 0, 0, 1, 2, 3, 3, 4};
    final int[] heads = {1, 2, 3, 4, 3, 4, 4, 5, 5};
    final int[] capacities = {5, 8, 2, 0, 4, 5, 6, 0, 4};
    final int[] expectedFlows = {4, 4, 2, 0, 4, 4, 0, 6, 4};
    final int expectedTotalFlow = 10;
    final MaxFlow maxFlow = new MaxFlow();
    assertNotNull(maxFlow);
    for (int i = 0; i < numArcs; ++i) {
      maxFlow.addArcWithCapacity(tails[i], heads[i], capacities[i]);
    }
    maxFlow.setArcCapacity(7, 6);
    final MaxFlow.Status solveStatus = maxFlow.solve(/*source=*/0, /*sink=*/numNodes - 1);
    assertEquals(solveStatus, MaxFlow.Status.OPTIMAL);
    final long totalFlow = maxFlow.getOptimalFlow();
    assertEquals(expectedTotalFlow, totalFlow);
    for (int i = 0; i < numArcs; ++i) {
      assertEquals(maxFlow.getFlow(i), expectedFlows[i]);
    }
  }
}
