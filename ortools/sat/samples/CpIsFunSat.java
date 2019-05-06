// Copyright 2010-2018 Google LLC
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
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;

/** Cryptarithmetic puzzle. */
public class CpIsFunSat {
  static {
    System.loadLibrary("jniortools");
  }

  // [START solution_printing]
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
  // [END solution_printing]

  public static void main(String[] args) throws Exception {
    // Create the model.
    CpModel model = new CpModel();

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

    // [START constraints]
    // Define constraints.
    model.addAllDifferent(letters);

    // CP + IS + FUN = TRUE
    model.addEquality(LinearExpr.scalProd(new IntVar[] {c, p, i, s, f, u, n, t, r, u, e},
                          new long[] {base, 1, base, 1, base * base, base, 1, -base * base * base,
                              -base * base, -base, -1}),
        0);
    // [END constraints]

    // [START solve]
    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinter cb = new VarArraySolutionPrinter(letters);
    solver.searchAllSolutions(model, cb);
    // [END solve]

    System.out.println("Statistics");
    System.out.println("  - conflicts : " + solver.numConflicts());
    System.out.println("  - branches  : " + solver.numBranches());
    System.out.println("  - wall time : " + solver.wallTime() + " s");
    System.out.println("  - solutions : " + cb.getSolutionCount());
  }
}
// [END program]
