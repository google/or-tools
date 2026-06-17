// Copyright 2010-2025 Google LLC
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
import com.google.ortools.graph.LinearSumAssignment;
import java.util.stream.IntStream;
// [END import]

/** Minimal Linear Sum Assignment problem. */
public class AssignmentLinearSumAssignment {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // [START solver]
    LinearSumAssignment assignment = new LinearSumAssignment();
    // [END solver]

    // [START data]
    final int[][] costs = {
        {90, 76, 75, 70},
        {35, 85, 55, 65},
        {125, 95, 90, 105},
        {45, 110, 95, 115},
    };
    final int numWorkers = 4;
    final int numTasks = 4;

    final int[] allWorkers = IntStream.range(0, numWorkers).toArray();
    final int[] allTasks = IntStream.range(0, numTasks).toArray();
    // [END data]

    // [START constraints]
    // Add each arc.
    for (int w : allWorkers) {
      for (int t : allTasks) {
        if (costs[w][t] != 0) {
          assignment.addArcWithCost(w, t, costs[w][t]);
        }
      }
    }
    // [END constraints]

    // [START solve]
    LinearSumAssignment.Status status = assignment.solve();
    // [END solve]

    // [START print_solution]
    if (status == LinearSumAssignment.Status.OPTIMAL) {
      System.out.println("Total cost: " + assignment.getOptimalCost());
      for (int worker : allWorkers) {
        System.out.println("Worker " + worker + " assigned to task "
            + assignment.getRightMate(worker) + ". Cost: " + assignment.getAssignmentCost(worker));
      }
    } else {
      System.out.println("Solving the min cost flow problem failed.");
      System.out.println("Solver status: " + status);
    }
    // [END print_solution]
  }

  private AssignmentLinearSumAssignment() {}
}
// [END program]
