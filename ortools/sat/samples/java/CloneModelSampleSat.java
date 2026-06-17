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

// [START import]
import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;

// [END import]

/** Minimal CP-SAT example to showcase cloning a model. */
public final class CloneModelSampleSat {
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
    model.maximize(LinearExpr.newBuilder().add(x).addTerm(y, 2).addTerm(z, 3).build());
    // [END objective]

    // Create a solver and solve the model.
    // [START solve]
    CpSolver solver = new CpSolver();
    CpSolverStatus unusedStatus = solver.solve(model);
    System.out.printf("Optimal value of the original model: %f\n", solver.objectiveValue());
    // [END solve]

    // [START clone]
    CpModel copy = model.getClone();

    // Add new constraint: copy_of_x + copy_of_y == 1.
    IntVar copyOfX = copy.getIntVarFromProtoIndex(x.getIndex());
    IntVar copyOfY = copy.getIntVarFromProtoIndex(y.getIndex());

    copy.addLessOrEqual(LinearExpr.newBuilder().add(copyOfX).add(copyOfY).build(), 1);
    // [END clone]

    unusedStatus = solver.solve(copy);
    System.out.printf("Optimal value of the cloned model: %f\n", solver.objectiveValue());
  }

  private CloneModelSampleSat() {}
}
// [END program]
