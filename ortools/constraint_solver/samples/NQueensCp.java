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
// OR-Tools solution to the N-queens problem.
package com.google.ortools.constraintsolver.samples;
// [START import]
// [END import]
import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.DecisionBuilder;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.Solver;
// [END import]

/** N-Queens Problem. */
public final class NQueensCp {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Instantiate the solver.
    // [START solver]
    Solver solver = new Solver("N-Queens");
    // [END solver]

    // [START variables]
    int boardSize = 8;
    IntVar[] queens = new IntVar[boardSize];
    for (int i = 0; i < boardSize; ++i) {
      queens[i] = solver.makeIntVar(0, boardSize - 1, "x" + i);
    }
    // [END variables]

    // Define constraints.
    // [START constraints]
    // All rows must be different.
    solver.addConstraint(solver.makeAllDifferent(queens));

    // All columns must be different because the indices of queens are all different.
    // No two queens can be on the same diagonal.
    IntVar[] diag1 = new IntVar[boardSize];
    IntVar[] diag2 = new IntVar[boardSize];
    for (int i = 0; i < boardSize; ++i) {
      diag1[i] = solver.makeSum(queens[i], i).var();
      diag2[i] = solver.makeSum(queens[i], -i).var();
    }
    solver.addConstraint(solver.makeAllDifferent(diag1));
    solver.addConstraint(solver.makeAllDifferent(diag2));
    // [END constraints]

    // [START db]
    // Create the decision builder to search for solutions.
    final DecisionBuilder db =
        solver.makePhase(queens, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
    // [END db]

    // [START solve]
    int solutionCount = 0;
    solver.newSearch(db);
    while (solver.nextSolution()) {
      System.out.println("Solution " + solutionCount);
      for (int i = 0; i < boardSize; ++i) {
        for (int j = 0; j < boardSize; ++j) {
          if (queens[j].value() == i) {
            System.out.print("Q");
          } else {
            System.out.print("_");
          }
          if (j != boardSize - 1) {
            System.out.print(" ");
          }
        }
        System.out.println();
      }
      solutionCount++;
    }
    solver.endSearch();
    // [END solve]

    // Statistics.
    // [START statistics]
    System.out.println("Statistics");
    System.out.println("  failures: " + solver.failures());
    System.out.println("  branches: " + solver.branches());
    System.out.println("  wall time: " + solver.wallTime() + "ms");
    System.out.println("  Solutions found: " + solutionCount);
    // [END statistics]
  }

  private NQueensCp() {}
}
// [END program]
