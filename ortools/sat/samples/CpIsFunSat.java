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

/** Cryptarithmetic puzzle. */
public final class CpIsFunSat {
  // [START solution_printer]
  static class VarArraySolutionPrinter extends CpSolverSolutionCallback {
    public VarArraySolutionPrinter(IntVar[] variables) {
      variableArray = variables;
    }

    @Override
    public void onSolutionCallback() {
      for (IntVar v : variableArray) {
        System.out.printf("  %s = %d", v.getName(), value(v));
      }
      System.out.println();
      solutionCount++;
    }

    public int getSolutionCount() {
      return solutionCount;
    }

    private int solutionCount;
    private final IntVar[] variableArray;
  }
  // [END solution_printer]

  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Create the model.
    // [START model]
    CpModel model = new CpModel();
    // [END model]

    // [START variables]
    int base = 10;
    IntVar c = model.newIntVar(1, base - 1, "C");
    IntVar p = model.newIntVar(0, base - 1, "P");
    IntVar i = model.newIntVar(1, base - 1, "I");
    IntVar s = model.newIntVar(0, base - 1, "S");
    IntVar f = model.newIntVar(1, base - 1, "F");
    IntVar u = model.newIntVar(0, base - 1, "U");
    IntVar n = model.newIntVar(0, base - 1, "N");
    IntVar t = model.newIntVar(1, base - 1, "T");
    IntVar r = model.newIntVar(0, base - 1, "R");
    IntVar e = model.newIntVar(0, base - 1, "E");

    // We need to group variables in a list to use the constraint AllDifferent.
    IntVar[] letters = new IntVar[] {c, p, i, s, f, u, n, t, r, e};
    // [END variables]

    // Define constraints.
    // [START constraints]
    model.addAllDifferent(letters);

    // CP + IS + FUN = TRUE
    model.addEquality(LinearExpr.scalProd(new IntVar[] {c, p, i, s, f, u, n, t, r, u, e},
                          new long[] {base, 1, base, 1, base * base, base, 1, -base * base * base,
                              -base * base, -base, -1}),
        0);
    // [END constraints]

    // Create a solver and solve the model.
    // [START solve]
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinter cb = new VarArraySolutionPrinter(letters);
    // Tell the solver to enumerate all solutions.
    solver.getParameters().setEnumerateAllSolutions(true);
    // And solve.
    solver.solve(model, cb);
    // [END solve]

    // Statistics.
    // [START statistics]
    System.out.println("Statistics");
    System.out.println("  - conflicts : " + solver.numConflicts());
    System.out.println("  - branches  : " + solver.numBranches());
    System.out.println("  - wall time : " + solver.wallTime() + " s");
    System.out.println("  - solutions : " + cb.getSolutionCount());
    // [END statistics]
  }

  private CpIsFunSat() {}
}
// [END program]
