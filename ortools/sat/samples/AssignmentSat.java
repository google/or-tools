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

// CP-SAT example that solves an assignment problem.
// [START program]
package com.google.ortools.sat.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;
// [END import]

/** Assignment problem. */
public class AssignmentSat {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Data
    // [START data_model]
    int[][] costs = {
        {90, 80, 75, 70},
        {35, 85, 55, 65},
        {125, 95, 90, 95},
        {45, 110, 95, 115},
        {50, 100, 90, 100},
    };
    final int numWorkers = costs.length;
    final int numTasks = costs[0].length;
    // [END data_model]

    // Model
    // [START model]
    CpModel model = new CpModel();
    // [END model]

    // Variables
    // [START variables]
    IntVar[][] x = new IntVar[numWorkers][numTasks];
    // Variables in a 1-dim array.
    IntVar[] xFlat = new IntVar[numWorkers * numTasks];
    int[] costsFlat = new int[numWorkers * numTasks];
    for (int i = 0; i < numWorkers; ++i) {
      for (int j = 0; j < numTasks; ++j) {
        x[i][j] = model.newIntVar(0, 1, "");
        int k = i * numTasks + j;
        xFlat[k] = x[i][j];
        costsFlat[k] = costs[i][j];
      }
    }
    // [END variables]

    // Constraints
    // [START constraints]
    // Each worker is assigned to at most one task.
    for (int i = 0; i < numWorkers; ++i) {
      IntVar[] vars = new IntVar[numTasks];
      for (int j = 0; j < numTasks; ++j) {
        vars[j] = x[i][j];
      }
      model.addLessOrEqual(LinearExpr.sum(vars), 1);
    }
    // Each task is assigned to exactly one worker.
    for (int j = 0; j < numTasks; ++j) {
      // LinearExpr taskSum;
      IntVar[] vars = new IntVar[numWorkers];
      for (int i = 0; i < numWorkers; ++i) {
        vars[i] = x[i][j];
      }
      model.addEquality(LinearExpr.sum(vars), 1);
    }
    // [END constraints]

    // Objective
    // [START objective]
    model.minimize(LinearExpr.scalProd(xFlat, costsFlat));
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
      for (int i = 0; i < numWorkers; ++i) {
        for (int j = 0; j < numTasks; ++j) {
          if (solver.value(x[i][j]) == 1) {
            System.out.println(
                "Worker " + i + " assigned to task " + j + ".  Cost: " + costs[i][j]);
          }
        }
      }
    } else {
      System.err.println("No solution found.");
    }
    // [END print_solution]
  }

  private AssignmentSat() {}
}
