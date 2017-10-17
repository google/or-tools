// Copyright 2010-2017 Google
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

import com.google.ortools.graph.LinearSumAssignment;

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

    LinearSumAssignment assignment = new LinearSumAssignment();
    for (int source = 0; source < numSources; ++source) {
      for (int target = 0; target < numTargets; ++target) {
        assignment.addArcWithCost(source, target, cost[source][target]);
      }
    }

    if (assignment.solve() == LinearSumAssignment.Status.OPTIMAL) {
      System.out.println("Total cost = " + assignment.getOptimalCost() + "/" + expectedCost);
      for (int node = 0; node < assignment.getNumNodes(); ++node) {
        System.out.println("Left node " + node
            + " assigned to right node " + assignment.getRightMate(node)
            + " with cost " + assignment.getAssignmentCost(node));
      }
    } else {
      System.out.println("No solution found.");
    }
  }

  public static void main(String[] args) throws Exception {
    LinearAssignmentAPI.runAssignmentOn4x4Matrix();
  }
}
