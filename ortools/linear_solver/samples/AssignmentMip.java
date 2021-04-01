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
package com.google.ortools.linearsolver.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;
// [END import]

/** MIP example that solves an assignment problem. */
public class AssignmentMip {
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

    // Solver
    // [START solver]
    // Create the linear solver with the SCIP backend.
    MPSolver solver = MPSolver.createSolver("SCIP");
    if (solver == null) {
      System.out.println("Could not create solver SCIP");
      return;
    }
    // [END solver]

    // Variables
    // [START variables]
    // x[i][j] is an array of 0-1 variables, which will be 1
    // if worker i is assigned to task j.
    MPVariable[][] x = new MPVariable[numWorkers][numTasks];
    for (int i = 0; i < numWorkers; ++i) {
      for (int j = 0; j < numTasks; ++j) {
        x[i][j] = solver.makeIntVar(0, 1, "");
      }
    }
    // [END variables]

    // Constraints
    // [START constraints]
    // Each worker is assigned to at most one task.
    for (int i = 0; i < numWorkers; ++i) {
      MPConstraint constraint = solver.makeConstraint(0, 1, "");
      for (int j = 0; j < numTasks; ++j) {
        constraint.setCoefficient(x[i][j], 1);
      }
    }
    // Each task is assigned to exactly one worker.
    for (int j = 0; j < numTasks; ++j) {
      MPConstraint constraint = solver.makeConstraint(1, 1, "");
      for (int i = 0; i < numWorkers; ++i) {
        constraint.setCoefficient(x[i][j], 1);
      }
    }
    // [END constraints]

    // Objective
    // [START objective]
    MPObjective objective = solver.objective();
    for (int i = 0; i < numWorkers; ++i) {
      for (int j = 0; j < numTasks; ++j) {
        objective.setCoefficient(x[i][j], costs[i][j]);
      }
    }
    objective.setMinimization();
    // [END objective]

    // Solve
    // [START solve]
    MPSolver.ResultStatus resultStatus = solver.solve();
    // [END solve]

    // Print solution.
    // [START print_solution]
    // Check that the problem has a feasible solution.
    if (resultStatus == MPSolver.ResultStatus.OPTIMAL
        || resultStatus == MPSolver.ResultStatus.FEASIBLE) {
      System.out.println("Total cost: " + objective.value() + "\n");
      for (int i = 0; i < numWorkers; ++i) {
        for (int j = 0; j < numTasks; ++j) {
          // Test if x[i][j] is 0 or 1 (with tolerance for floating point
          // arithmetic).
          if (x[i][j].solutionValue() > 0.5) {
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

  private AssignmentMip() {}
}
// [END program]
