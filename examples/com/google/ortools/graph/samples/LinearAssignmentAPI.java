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

package com.google.ortools.graph.samples;

import com.google.ortools.graph.ForwardEbertLinearSumAssignment;
import com.google.ortools.graph.ForwardStarGraph;

/**
 * Test assignment on a 4x4 matrix. Example taken from
 *  http://www.ee.oulu.fi/~mpa/matreng/eem1_2-1.htm with kCost[0][1]
 *  modified so the optimum solution is unique.
 *
 */

public class LinearAssignmentAPI {

 static {
    System.loadLibrary("jniortools");
  }


  private static void runAssignmentOn4x4Matrix() {
    final int numSources = 4;
    final int numTargets = 4;
    final int[][] cost = {{ 90, 76, 75, 80 },
                          { 35, 85, 55, 65 },
                          { 125, 95, 90, 105 },
                          { 45, 110, 95, 115 }};
    final int expectedCost = cost[0][3] + cost[1][2] + cost[2][1] + cost[3][0];

    ForwardStarGraph graph = new ForwardStarGraph(numSources + numTargets,
                                                  numSources * numTargets);
    ForwardEbertLinearSumAssignment assignment =
        new ForwardEbertLinearSumAssignment(graph, numSources);
    for (int source = 0; source < numSources; ++source) {
      for (int target = 0; target < numTargets; ++target) {
        final int arc = graph.addArc(source, numSources + target);
        assignment.setArcCost(arc, cost[source][target]);
      }
    }

    if (assignment.computeAssignment()) {
    final long totalCost = assignment.getCost();
    System.out.println("total cost = " + totalCost + "/" + expectedCost);
    } else {
      System.out.println("No solution found");
    }
  }

  public static void main(String[] args) throws Exception {
    LinearAssignmentAPI.runAssignmentOn4x4Matrix();
  }
}
