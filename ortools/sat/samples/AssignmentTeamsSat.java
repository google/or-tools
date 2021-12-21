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
import java.util.ArrayList;
import java.util.List;
import java.util.stream.IntStream;
// [END import]

/** Assignment problem. */
public class AssignmentTeamsSat {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Data
    // [START data]
    int[][] costs = {
        {90, 76, 75, 70},
        {35, 85, 55, 65},
        {125, 95, 90, 105},
        {45, 110, 95, 115},
        {60, 105, 80, 75},
        {45, 65, 110, 95},
    };
    final int numWorkers = costs.length;
    final int numTasks = costs[0].length;

    final int[] allWorkers = IntStream.range(0, numWorkers).toArray();
    final int[] allTasks = IntStream.range(0, numTasks).toArray();

    final int[] team1 = {0, 2, 4};
    final int[] team2 = {1, 3, 5};
    // Maximum total of tasks for any team
    final int teamMax = 2;
    // [END data]

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
    for (int worker : allWorkers) {
      for (int task : allTasks) {
        x[worker][task] = model.newBoolVar("x[" + worker + "," + task + "]");
        int k = worker * numTasks + task;
        xFlat[k] = x[worker][task];
        costsFlat[k] = costs[worker][task];
      }
    }
    // [END variables]

    // Constraints
    // [START constraints]
    // Each worker is assigned to at most one task.
    for (int worker : allWorkers) {
      IntVar[] vars = new IntVar[numTasks];
      for (int task : allTasks) {
        vars[task] = x[worker][task];
      }
      model.addLessOrEqual(LinearExpr.sum(vars), 1);
    }
    // Each task is assigned to exactly one worker.
    for (int task : allTasks) {
      // LinearExpr taskSum;
      IntVar[] vars = new IntVar[numWorkers];
      for (int worker : allWorkers) {
        vars[worker] = x[worker][task];
      }
      model.addEquality(LinearExpr.sum(vars), 1);
    }

    // Each team takes at most two tasks.
    List<IntVar> team1Tasks = new ArrayList<>();
    for (int worker : team1) {
      for (int task : allTasks) {
        team1Tasks.add(x[worker][task]);
      }
    }
    model.addLessOrEqual(LinearExpr.sum(team1Tasks.toArray(new IntVar[0])), teamMax);

    List<IntVar> team2Tasks = new ArrayList<>();
    for (int worker : team2) {
      for (int task : allTasks) {
        team2Tasks.add(x[worker][task]);
      }
    }
    model.addLessOrEqual(LinearExpr.sum(team2Tasks.toArray(new IntVar[0])), teamMax);
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
      for (int worker : allWorkers) {
        for (int task : allTasks) {
          if (solver.value(x[worker][task]) == 1) {
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

  private AssignmentTeamsSat() {}
}
