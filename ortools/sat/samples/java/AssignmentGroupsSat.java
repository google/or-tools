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
// CP-SAT example that solves an assignment problem.
package com.google.ortools.sat.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.LinearExprBuilder;
import com.google.ortools.sat.Literal;
import java.util.ArrayList;
import java.util.List;
import java.util.stream.IntStream;
// [END import]

/** Assignment problem. */
public class AssignmentGroupsSat {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Data
    // [START data]
    int[][] costs = {
        {90, 76, 75, 70, 50, 74},
        {35, 85, 55, 65, 48, 101},
        {125, 95, 90, 105, 59, 120},
        {45, 110, 95, 115, 104, 83},
        {60, 105, 80, 75, 59, 62},
        {45, 65, 110, 95, 47, 31},
        {38, 51, 107, 41, 69, 99},
        {47, 85, 57, 71, 92, 77},
        {39, 63, 97, 49, 118, 56},
        {47, 101, 71, 60, 88, 109},
        {17, 39, 103, 64, 61, 92},
        {101, 45, 83, 59, 92, 27},
    };
    final int numWorkers = costs.length;
    final int numTasks = costs[0].length;

    final int[] allWorkers = IntStream.range(0, numWorkers).toArray();
    final int[] allTasks = IntStream.range(0, numTasks).toArray();
    // [END data]

    // Allowed groups of workers:
    // [START allowed_groups]
    int[][] group1 = {
        {0, 0, 1, 1}, // Workers 2, 3
        {0, 1, 0, 1}, // Workers 1, 3
        {0, 1, 1, 0}, // Workers 1, 2
        {1, 1, 0, 0}, // Workers 0, 1
        {1, 0, 1, 0}, // Workers 0, 2
    };

    int[][] group2 = {
        {0, 0, 1, 1}, // Workers 6, 7
        {0, 1, 0, 1}, // Workers 5, 7
        {0, 1, 1, 0}, // Workers 5, 6
        {1, 1, 0, 0}, // Workers 4, 5
        {1, 0, 0, 1}, // Workers 4, 7
    };

    int[][] group3 = {
        {0, 0, 1, 1}, // Workers 10, 11
        {0, 1, 0, 1}, // Workers 9, 11
        {0, 1, 1, 0}, // Workers 9, 10
        {1, 0, 1, 0}, // Workers 8, 10
        {1, 0, 0, 1}, // Workers 8, 11
    };
    // [END allowed_groups]

    // Model
    // [START model]
    CpModel model = new CpModel();
    // [END model]

    // Variables
    // [START variables]
    Literal[][] x = new Literal[numWorkers][numTasks];
    for (int worker : allWorkers) {
      for (int task : allTasks) {
        x[worker][task] = model.newBoolVar("x[" + worker + "," + task + "]");
      }
    }
    // [END variables]

    // Constraints
    // [START constraints]
    // Each worker is assigned to at most one task.
    for (int worker : allWorkers) {
      List<Literal> tasks = new ArrayList<>();
      for (int task : allTasks) {
        tasks.add(x[worker][task]);
      }
      model.addAtMostOne(tasks);
    }

    // Each task is assigned to exactly one worker.
    for (int task : allTasks) {
      List<Literal> workers = new ArrayList<>();
      for (int worker : allWorkers) {
        workers.add(x[worker][task]);
      }
      model.addExactlyOne(workers);
    }
    // [END constraints]

    // [START assignments]
    // Create variables for each worker, indicating whether they work on some task.
    IntVar[] work = new IntVar[numWorkers];
    for (int worker : allWorkers) {
      work[worker] = model.newBoolVar("work[" + worker + "]");
    }

    for (int worker : allWorkers) {
      LinearExprBuilder expr = LinearExpr.newBuilder();
      for (int task : allTasks) {
        expr.add(x[worker][task]);
      }
      model.addEquality(work[worker], expr);
    }

    // Define the allowed groups of worders
    model.addAllowedAssignments(new IntVar[] {work[0], work[1], work[2], work[3]})
        .addTuples(group1);
    model.addAllowedAssignments(new IntVar[] {work[4], work[5], work[6], work[7]})
        .addTuples(group2);
    model.addAllowedAssignments(new IntVar[] {work[8], work[9], work[10], work[11]})
        .addTuples(group3);
    // [END assignments]

    // Objective
    // [START objective]
    LinearExprBuilder obj = LinearExpr.newBuilder();
    for (int worker : allWorkers) {
      for (int task : allTasks) {
        obj.addTerm(x[worker][task], costs[worker][task]);
      }
    }
    model.minimize(obj);
    // [END objective]

    // Solve
    // [START solve]
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);
    // [END solve]

    // Print solution.
    // [START print_solution]
    // Check that the problem has a feasible solution.
    if (status == CpSolverStatus.OPTIMAL || status == CpSolverStatus.FEASIBLE) {
      System.out.println("Total cost: " + solver.objectiveValue() + "\n");
      for (int worker : allWorkers) {
        for (int task : allTasks) {
          if (solver.booleanValue(x[worker][task])) {
            System.out.println("Worker " + worker + " assigned to task " + task
                + ".  Cost: " + costs[worker][task]);
          }
        }
      }
    } else {
      System.err.println("No solution found.");
    }
    // [END print_solution]
  }

  private AssignmentGroupsSat() {}
}
