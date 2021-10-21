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
import com.google.ortools.graph.MaxFlow;
// [END import]

/** Minimal MaxFlow program. */
public final class SimpleMaxFlowProgram {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // [START solver]
    // Instantiate a SimpleMaxFlow solver.
    MaxFlow maxFlow = new MaxFlow();
    // [END solver]

    // [START data]
    // Define three parallel arrays: start_nodes, end_nodes, and the capacities
    // between each pair. For instance, the arc from node 0 to node 1 has a
    // capacity of 20.
    // From Taha's 'Introduction to Operations Research',
    // example 6.4-2.
    int[] startNodes = new int[] {0, 0, 0, 1, 1, 2, 2, 3, 3};
    int[] endNodes = new int[] {1, 2, 3, 2, 4, 3, 4, 2, 4};
    int[] capacities = new int[] {20, 30, 10, 40, 30, 10, 20, 5, 20};
    // [END data]

    // [START constraints]
    // Add each arc.
    for (int i = 0; i < startNodes.length; ++i) {
      int arc = maxFlow.addArcWithCapacity(startNodes[i], endNodes[i], capacities[i]);
      if (arc != i) {
        throw new Exception("Internal error");
      }
    }
    // [END constraints]

    // [START solve]
    // Find the maximum flow between node 0 and node 4.
    MaxFlow.Status status = maxFlow.solve(0, 4);
    // [END solve]

    // [START print_solution]
    if (status == MaxFlow.Status.OPTIMAL) {
      System.out.println("Max. flow: " + maxFlow.getOptimalFlow());
      System.out.println();
      System.out.println("  Arc     Flow / Capacity");
      for (int i = 0; i < maxFlow.getNumArcs(); ++i) {
        System.out.println(maxFlow.getTail(i) + " -> " + maxFlow.getHead(i) + "    "
            + maxFlow.getFlow(i) + "  /  " + maxFlow.getCapacity(i));
      }
    } else {
      System.out.println("Solving the max flow problem failed. Solver status: " + status);
    }
    // [END print_solution]
  }

  private SimpleMaxFlowProgram() {}
}
// [END program]
