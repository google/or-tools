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
package com.google.ortools.sat.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;
// [END import]

/** OR-Tools solution to the N-queens problem. */
public final class NQueensSat {
  // [START solution_printer]
  static class SolutionPrinter extends CpSolverSolutionCallback {
    public SolutionPrinter(IntVar[] queensIn) {
      solutionCount = 0;
      queens = queensIn;
    }

    @Override
    public void onSolutionCallback() {
      System.out.println("Solution " + solutionCount);
      for (int i = 0; i < queens.length; ++i) {
        for (int j = 0; j < queens.length; ++j) {
          if (value(queens[j]) == i) {
            System.out.print("Q");
          } else {
            System.out.print("_");
          }
          if (j != queens.length - 1) {
            System.out.print(" ");
          }
        }
        System.out.println();
      }
      solutionCount++;
    }

    public int getSolutionCount() {
      return solutionCount;
    }

    private int solutionCount;
    private final IntVar[] queens;
  }
  // [END solution_printer]

  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Create the model.
    // [START model]
    CpModel model = new CpModel();
    // [END model]

    // [START variables]
    int boardSize = 8;
    IntVar[] queens = new IntVar[boardSize];
    for (int i = 0; i < boardSize; ++i) {
      queens[i] = model.newIntVar(0, boardSize - 1, "x" + i);
    }
    // [END variables]

    // Define constraints.
    // [START constraints]
    // All rows must be different.
    model.addAllDifferent(queens);

    // All columns must be different because the indices of queens are all different.
    // No two queens can be on the same diagonal.
    LinearExpr[] diag1 = new LinearExpr[boardSize];
    LinearExpr[] diag2 = new LinearExpr[boardSize];
    for (int i = 0; i < boardSize; ++i) {
      diag1[i] = LinearExpr.affine(queens[i], /*coefficient=*/1, /*offset=*/i);
      diag2[i] = LinearExpr.affine(queens[i], /*coefficient=*/1, /*offset=*/-i);
    }
    model.addAllDifferent(diag1);
    model.addAllDifferent(diag2);
    // [END constraints]

    // Create a solver and solve the model.
    // [START solve]
    CpSolver solver = new CpSolver();
    SolutionPrinter cb = new SolutionPrinter(queens);
    // Tell the solver to enumerate all solutions.
    solver.getParameters().setEnumerateAllSolutions(true);
    // And solve.
    solver.solve(model, cb);
    // [END solve]

    // Statistics.
    // [START statistics]
    System.out.println("Statistics");
    System.out.println("  conflicts : " + solver.numConflicts());
    System.out.println("  branches  : " + solver.numBranches());
    System.out.println("  wall time : " + solver.wallTime() + " s");
    System.out.println("  solutions : " + cb.getSolutionCount());
    // [END statistics]
  }

  private NQueensSat() {}
}
// [END program]
