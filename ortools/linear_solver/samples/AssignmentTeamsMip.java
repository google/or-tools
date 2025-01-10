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
import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;
import java.util.stream.IntStream;
// [END import]

/** MIP example that solves an assignment problem. */
public class AssignmentTeamsMip {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Data
    // [START data]
    double[][] costs = {
        {90, 76, 75, 70},
        {35, 85, 55, 65},
        {125, 95, 90, 105},
        {45, 110, 95, 115},
        {60, 105, 80, 75},
        {45, 65, 110, 95},
    };
    int numWorkers = costs.length;
    int numTasks = costs[0].length;

    final int[] allWorkers = IntStream.range(0, numWorkers).toArray();
    final int[] allTasks = IntStream.range(0, numTasks).toArray();

    final int[] team1 = {0, 2, 4};
    final int[] team2 = {1, 3, 5};
    // Maximum total of tasks for any team
    final int teamMax = 2;
    // [END data]

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
    for (int worker : allWorkers) {
      for (int task : allTasks) {
        x[worker][task] = solver.makeBoolVar("x[" + worker + "," + task + "]");
      }
    }
    // [END variables]

    // Constraints
    // [START constraints]
    // Each worker is assigned to at most one task.
    for (int worker : allWorkers) {
      MPConstraint constraint = solver.makeConstraint(0, 1, "");
      for (int task : allTasks) {
        constraint.setCoefficient(x[worker][task], 1);
      }
    }
    // Each task is assigned to exactly one worker.
    for (int task : allTasks) {
      MPConstraint constraint = solver.makeConstraint(1, 1, "");
      for (int worker : allWorkers) {
        constraint.setCoefficient(x[worker][task], 1);
      }
    }

    // Each team takes at most two tasks.
    MPConstraint team1Tasks = solver.makeConstraint(0, teamMax, "");
    for (int worker : team1) {
      for (int task : allTasks) {
        team1Tasks.setCoefficient(x[worker][task], 1);
      }
    }

    MPConstraint team2Tasks = solver.makeConstraint(0, teamMax, "");
    for (int worker : team2) {
      for (int task : allTasks) {
        team2Tasks.setCoefficient(x[worker][task], 1);
      }
    }
    // [END constraints]

    // Objective
    // [START objective]
    MPObjective objective = solver.objective();
    for (int worker : allWorkers) {
      for (int task : allTasks) {
        objective.setCoefficient(x[worker][task], costs[worker][task]);
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
      for (int worker : allWorkers) {
        for (int task : allTasks) {
          // Test if x[i][j] is 0 or 1 (with tolerance for floating point
          // arithmetic).
          if (x[worker][task].solutionValue() > 0.5) {
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

  private AssignmentTeamsMip() {}
}
// [END program]
