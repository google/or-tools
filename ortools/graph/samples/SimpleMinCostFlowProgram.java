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
// From Bradley, Hax, and Maganti, 'Applied Mathematical Programming', figure 8.1.
package com.google.ortools.graph.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.graph.MinCostFlow;
import com.google.ortools.graph.MinCostFlowBase;
// [END import]

/** Minimal MinCostFlow program. */
public class SimpleMinCostFlowProgram {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // [START solver]
    // Instantiate a SimpleMinCostFlow solver.
    MinCostFlow minCostFlow = new MinCostFlow();
    // [END solver]

    // [START data]
    // Define four parallel arrays: sources, destinations, capacities, and unit costs
    // between each pair. For instance, the arc from node 0 to node 1 has a
    // capacity of 15.
    // Problem taken From Taha's 'Introduction to Operations Research',
    // example 6.4-2.
    int[] startNodes = new int[] {0, 0, 1, 1, 1, 2, 2, 3, 4};
    int[] endNodes = new int[] {1, 2, 2, 3, 4, 3, 4, 4, 2};
    int[] capacities = new int[] {15, 8, 20, 4, 10, 15, 4, 20, 5};
    int[] unitCosts = new int[] {4, 4, 2, 2, 6, 1, 3, 2, 3};

    // Define an array of supplies at each node.
    int[] supplies = new int[] {20, 0, 0, -5, -15};
    // [END data]

    // [START constraints]
    // Add each arc.
    for (int i = 0; i < startNodes.length; ++i) {
      int arc = minCostFlow.addArcWithCapacityAndUnitCost(
          startNodes[i], endNodes[i], capacities[i], unitCosts[i]);
      if (arc != i) {
        throw new Exception("Internal error");
      }
    }

    // Add node supplies.
    for (int i = 0; i < supplies.length; ++i) {
      minCostFlow.setNodeSupply(i, supplies[i]);
    }
    // [END constraints]

    // [START solve]
    // Find the min cost flow.
    MinCostFlowBase.Status status = minCostFlow.solve();
    // [END solve]

    // [START print_solution]
    if (status == MinCostFlow.Status.OPTIMAL) {
      System.out.println("Minimum cost: " + minCostFlow.getOptimalCost());
      System.out.println();
      System.out.println(" Edge   Flow / Capacity  Cost");
      for (int i = 0; i < minCostFlow.getNumArcs(); ++i) {
        long cost = minCostFlow.getFlow(i) * minCostFlow.getUnitCost(i);
        System.out.println(minCostFlow.getTail(i) + " -> " + minCostFlow.getHead(i) + "  "
            + minCostFlow.getFlow(i) + "  / " + minCostFlow.getCapacity(i) + "       " + cost);
      }
    } else {
      System.out.println("Solving the min cost flow problem failed.");
      System.out.println("Solver status: " + status);
    }
    // [END print_solution]
  }

  private SimpleMinCostFlowProgram() {}
}
// [END program]
