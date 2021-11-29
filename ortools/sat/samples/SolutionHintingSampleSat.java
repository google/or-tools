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
import com.google.ortools.sat.LinearExpr;

/** Minimal CP-SAT example to showcase calling the solver. */
public class SolutionHintingSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Create the model.
    // [START model]
    CpModel model = new CpModel();
    // [END model]

    // Create the variables.
    // [START variables]
    int numVals = 3;

    IntVar x = model.newIntVar(0, numVals - 1, "x");
    IntVar y = model.newIntVar(0, numVals - 1, "y");
    IntVar z = model.newIntVar(0, numVals - 1, "z");
    // [END variables]

    // Create the constraints.
    // [START constraints]
    model.addDifferent(x, y);
    // [END constraints]

    // [START objective]
    model.maximize(LinearExpr.scalProd(new IntVar[] {x, y, z}, new int[] {1, 2, 3}));
    // [END objective]

    // Solution hinting: x <- 1, y <- 2
    model.addHint(x, 1);
    model.addHint(y, 2);

    // Create a solver and solve the model.
    // [START solve]
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinterWithObjective cb =
        new VarArraySolutionPrinterWithObjective(new IntVar[] {x, y, z});
    solver.solve(model, cb);
    // [END solve]
  }

  static class VarArraySolutionPrinterWithObjective extends CpSolverSolutionCallback {
    public VarArraySolutionPrinterWithObjective(IntVar[] variables) {
      variableArray = variables;
    }

    @Override
    public void onSolutionCallback() {
      System.out.printf("Solution #%d: time = %.02f s%n", solutionCount, wallTime());
      System.out.printf("  objective value = %f%n", objectiveValue());
      for (IntVar v : variableArray) {
        System.out.printf("  %s = %d%n", v.getName(), value(v));
      }
      solutionCount++;
    }

    private int solutionCount;
    private final IntVar[] variableArray;
  }
}
// [END program]
