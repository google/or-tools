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

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;

/** Code sample that solves a model and displays a small number of solutions. */
public final class StopAfterNSolutionsSampleSat {
  static class VarArraySolutionPrinterWithLimit extends CpSolverSolutionCallback {
    public VarArraySolutionPrinterWithLimit(IntVar[] variables, int limit) {
      variableArray = variables;
      solutionLimit = limit;
    }

    @Override
    public void onSolutionCallback() {
      System.out.printf("Solution #%d: time = %.02f s%n", solutionCount, wallTime());
      for (IntVar v : variableArray) {
        System.out.printf("  %s = %d%n", v.getName(), value(v));
      }
      solutionCount++;
      if (solutionCount >= solutionLimit) {
        System.out.printf("Stop search after %d solutions%n", solutionLimit);
        stopSearch();
      }
    }

    public int getSolutionCount() {
      return solutionCount;
    }

    private int solutionCount;
    private final IntVar[] variableArray;
    private final int solutionLimit;
  }

  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Create the model.
    CpModel model = new CpModel();
    // Create the variables.
    int numVals = 3;

    IntVar x = model.newIntVar(0, numVals - 1, "x");
    IntVar y = model.newIntVar(0, numVals - 1, "y");
    IntVar z = model.newIntVar(0, numVals - 1, "z");

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinterWithLimit cb =
        new VarArraySolutionPrinterWithLimit(new IntVar[] {x, y, z}, 5);
    // Tell the solver to enumerate all solutions.
    solver.getParameters().setEnumerateAllSolutions(true);
    // And solve.
    solver.solve(model, cb);

    System.out.println(cb.getSolutionCount() + " solutions found.");
    if (cb.getSolutionCount() != 5) {
      throw new RuntimeException("Did not stop the search correctly.");
    }
  }

  private StopAfterNSolutionsSampleSat() {}
}
// [END program]
