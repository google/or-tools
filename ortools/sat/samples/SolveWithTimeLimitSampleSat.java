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
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;

/** Solves a problem with a time limit. */
public final class SolveWithTimeLimitSampleSat {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Create the model.
    CpModel model = new CpModel();
    // Create the variables.
    int numVals = 3;

    IntVar x = model.newIntVar(0, numVals - 1, "x");
    IntVar y = model.newIntVar(0, numVals - 1, "y");
    IntVar z = model.newIntVar(0, numVals - 1, "z");
    // Create the constraint.
    model.addDifferent(x, y);

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    solver.getParameters().setMaxTimeInSeconds(10.0);
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.OPTIMAL) {
      System.out.println("x = " + solver.value(x));
      System.out.println("y = " + solver.value(y));
      System.out.println("z = " + solver.value(z));
    }
  }

  private SolveWithTimeLimitSampleSat() {}
}
// [END program]
