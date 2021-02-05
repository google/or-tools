// Copyright 2021 Xiang Chen
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
import com.google.ortools.sat.Literal;

/** Minimal CP-SAT example to showcase assumptions. */
public class AssumptionsSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Create the model.
    // [START model]
    CpModel model = new CpModel();
    // [END model]

    // Create the variables.
    // [START variables]
    IntVar x = model.newIntVar(0, 10, "x");
    IntVar y = model.newIntVar(0, 10, "y");
    IntVar z = model.newIntVar(0, 10, "z");
    Literal a = model.newBoolVar("a");
    Literal b = model.newBoolVar("b");
    Literal c = model.newBoolVar("c");
    // [END variables]

    // Creates the constraints.
    // [START constraints]
    model.addGreaterThan(x, y).onlyEnforceIf(a);
    model.addGreaterThan(y, z).onlyEnforceIf(b);
    model.addGreaterThan(z, x).onlyEnforceIf(c);
    // [END constraints]

    // Add assumptions
    model.addAssumptions(new Literal[] {a, b, c});

    // Create a solver and solve the model.
    // [START solve]
    CpSolver solver = new CpSolver();
    solver.solve(model);
    System.out.println(solver.sufficientAssumptionsForInfeasibility());
    // [END solve]
  }

}
// [END program]
