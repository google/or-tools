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
package com.google.ortools.linearsolver.samples;

// [START import]
import com.google.ortools.Loader;
import com.google.ortools.modelbuilder.LinearExpr;
import com.google.ortools.modelbuilder.LinearExprBuilder;
import com.google.ortools.modelbuilder.ModelBuilder;
import com.google.ortools.modelbuilder.ModelSolver;
import com.google.ortools.modelbuilder.SolveStatus;
import com.google.ortools.modelbuilder.Variable;

// [END import]

/** MIP example that solves an assignment problem. */
public class AssignmentMb {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();

    // Data
    // [START data_model]
    double[][] costs = {
        {90, 80, 75, 70},
        {35, 85, 55, 65},
        {125, 95, 90, 95},
        {45, 110, 95, 115},
        {50, 100, 90, 100},
    };
    int numWorkers = costs.length;
    int numTasks = costs[0].length;
    // [END data_model]

    // [START model]
    ModelBuilder model = new ModelBuilder();
    // [END model]

    // Variables
    // [START variables]
    // x[i][j] is an array of 0-1 variables, which will be 1
    // if worker i is assigned to task j.
    Variable[][] x = new Variable[numWorkers][numTasks];
    for (int i = 0; i < numWorkers; ++i) {
      for (int j = 0; j < numTasks; ++j) {
        x[i][j] = model.newBoolVar("");
      }
    }
    // [END variables]

    // Constraints
    // [START constraints]
    // Each worker is assigned to at most one task.
    for (int i = 0; i < numWorkers; ++i) {
      LinearExprBuilder assignedWork = LinearExpr.newBuilder();
      for (int j = 0; j < numTasks; ++j) {
        assignedWork.add(x[i][j]);
      }
      model.addLessOrEqual(assignedWork, 1);
    }

    // Each task is assigned to exactly one worker.
    for (int j = 0; j < numTasks; ++j) {
      LinearExprBuilder assignedWorker = LinearExpr.newBuilder();
      for (int i = 0; i < numWorkers; ++i) {
        assignedWorker.add(x[i][j]);
      }
      model.addEquality(assignedWorker, 1);
    }
    // [END constraints]

    // Objective
    // [START objective]
    LinearExprBuilder objective = LinearExpr.newBuilder();
    for (int i = 0; i < numWorkers; ++i) {
      for (int j = 0; j < numTasks; ++j) {
        objective.addTerm(x[i][j], costs[i][j]);
      }
    }
    model.minimize(objective);
    // [END objective]

    // [START solver]
    // Create the solver with the SCIP backend and check it is supported.
    ModelSolver solver = new ModelSolver("scip");
    if (!solver.solverIsSupported()) {
      return;
    }
    // [END solver]

    // [START solve]
    final SolveStatus resultStatus = solver.solve(model);
    // [END solve]

    // Print solution.
    // [START print_solution]
    // Check that the problem has a feasible solution.
    if (resultStatus == SolveStatus.OPTIMAL || resultStatus == SolveStatus.FEASIBLE) {
      System.out.println("Total cost: " + solver.getObjectiveValue() + "\n");
      for (int i = 0; i < numWorkers; ++i) {
        for (int j = 0; j < numTasks; ++j) {
          // Test if x[i][j] is 0 or 1 (with tolerance for floating point
          // arithmetic).
          if (solver.getValue(x[i][j]) > 0.9) {
            System.out.println(
                "Worker " + i + " assigned to task " + j + ".  Cost = " + costs[i][j]);
          }
        }
      }
    } else {
      System.err.println("No solution found.");
    }
    // [END print_solution]
  }

  private AssignmentMb() {}
}
// [END program]
