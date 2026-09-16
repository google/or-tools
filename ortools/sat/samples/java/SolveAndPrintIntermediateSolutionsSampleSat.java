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
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;
import java.util.function.Consumer;

/** Solves an optimization problem and displays all intermediate solutions. */
public final class SolveAndPrintIntermediateSolutionsSampleSat {
  // [START print_solution]
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

    public int getSolutionCount() {
      return solutionCount;
    }

    private int solutionCount;
    private final IntVar[] variableArray;
  }

  static class BestBoundCallback implements Consumer<Double> {
    public BestBoundCallback() {
      bestBound = 0.0;
      numImprovements = 0;
    }

    @Override
    public void accept(Double bound) {
      bestBound = bound;
      numImprovements++;
    }

    public double getBestBound() {
      return bestBound;
    }

    double bestBound;
    int numImprovements;
  }

  // [END print_solution]

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

    // Create the constraint.
    // [START constraints]
    model.addDifferent(x, y);
    // [END constraints]

    // Maximize a linear combination of variables.
    // [START objective]
    model.maximize(LinearExpr.weightedSum(new IntVar[] {x, y, z}, new long[] {1, 2, 3}));
    // [END objective]

    // Create a solver and solve the model.
    // [START solve]
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinterWithObjective cb =
        new VarArraySolutionPrinterWithObjective(new IntVar[] {x, y, z});
    solver.getParameters().setNumWorkers(1);
    solver.getParameters().setLinearizationLevel(2);
    BestBoundCallback bestBoundCallback = new BestBoundCallback();

    solver.setBestBoundCallback(bestBoundCallback);
    CpSolverStatus unusedStatus = solver.solve(model, cb);
    // [END solve]

    System.out.println("solution count: " + cb.getSolutionCount());
    System.out.println("best bound count: " + bestBoundCallback.numImprovements);
  }

  private SolveAndPrintIntermediateSolutionsSampleSat() {}
}
// [END program]
