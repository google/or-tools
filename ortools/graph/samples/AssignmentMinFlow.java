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
package com.google.ortools.graph.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.graph.MinCostFlow;
import com.google.ortools.graph.MinCostFlowBase;
// [END import]

/** Minimal Assignment Min Flow. */
public class AssignmentMinFlow {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // [START solver]
    // Instantiate a SimpleMinCostFlow solver.
    MinCostFlow minCostFlow = new MinCostFlow();
    // [END solver]

    // [START data]
    // Define four parallel arrays: sources, destinations, capacities, and unit costs
    // between each pair.
    int[] startNodes =
        new int[] {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 6, 7, 8};
    int[] endNodes =
        new int[] {1, 2, 3, 4, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 5, 6, 7, 8, 9, 9, 9, 9};
    int[] capacities =
        new int[] {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int[] unitCosts = new int[] {
        0, 0, 0, 0, 90, 76, 75, 70, 35, 85, 55, 65, 125, 95, 90, 105, 45, 110, 95, 115, 0, 0, 0, 0};

    int source = 0;
    int sink = 9;
    int tasks = 4;
    // Define an array of supplies at each node.
    int[] supplies = new int[] {tasks, 0, 0, 0, 0, 0, 0, 0, 0, -tasks};
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
      System.out.println("Total cost: " + minCostFlow.getOptimalCost());
      System.out.println();
      for (int i = 0; i < minCostFlow.getNumArcs(); ++i) {
        // Can ignore arcs leading out of source or into sink.
        if (minCostFlow.getTail(i) != source && minCostFlow.getHead(i) != sink) {
          // Arcs in the solution have a flow value of 1. Their start and end nodes
          // give an assignment of worker to task.
          if (minCostFlow.getFlow(i) > 0) {
            System.out.println("Worker " + minCostFlow.getTail(i) + " assigned to task "
                + minCostFlow.getHead(i) + " Cost: " + minCostFlow.getUnitCost(i));
          }
        }
      }
    } else {
      System.out.println("Solving the min cost flow problem failed.");
      System.out.println("Solver status: " + status);
    }
    // [END print_solution]
  }

  private AssignmentMinFlow() {}
}
// [END program]
