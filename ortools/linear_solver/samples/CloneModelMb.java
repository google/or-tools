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

// Minimal example to clone a model.
// [START program]
package com.google.ortools.linearsolver.samples;

// [START import]
import com.google.ortools.Loader;
import com.google.ortools.modelbuilder.LinearConstraint;
import com.google.ortools.modelbuilder.LinearExpr;
import com.google.ortools.modelbuilder.ModelBuilder;
import com.google.ortools.modelbuilder.ModelSolver;
import com.google.ortools.modelbuilder.SolveStatus;
import com.google.ortools.modelbuilder.Variable;
// [END import]

/** Minimal Mixed Integer Programming example to showcase cloning a model. */
public final class CloneModelMb {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // [START model]
    // Create the linear model.
    ModelBuilder model = new ModelBuilder();
    // [END model]

    // [START variables]
    double infinity = java.lang.Double.POSITIVE_INFINITY;
    // x and y are integer non-negative variables.
    Variable x = model.newIntVar(0.0, infinity, "x");
    Variable y = model.newIntVar(0.0, infinity, "y");
    // [END variables]

    // [START constraints]
    // x + 7 * y <= 17.5.
    LinearConstraint unusedC1 =
        model.addLessOrEqual(LinearExpr.newBuilder().add(x).addTerm(y, 7), 17.5).withName("c0");

    // x <= 3.5.
    LinearConstraint c2 = model.addLessOrEqual(x, 3.5).withName("c1");
    // [END constraints]

    // [START objective]
    // Maximize x + 10 * y.
    model.maximize(LinearExpr.newBuilder().add(x).addTerm(y, 10.0));
    // [END objective]

    // [Start clone]
    // Clone the model.
    System.out.println("Cloning the model");
    ModelBuilder modelCopy = model.getClone();
    Variable xCopy = modelCopy.varFromIndex(x.getIndex());
    Variable yCopy = modelCopy.varFromIndex(y.getIndex());
    Variable zCopy = modelCopy.newBoolVar("z");
    LinearConstraint c2Copy = modelCopy.constraintFromIndex(c2.getIndex());

    // Add a new constraint.
    LinearConstraint unusedC3Copy = modelCopy.addGreaterOrEqual(xCopy, 1);

    // Modify a constraint.
    c2Copy.addTerm(zCopy, 2.0);

    System.out.println("Number of constraints in the original model = " + model.numConstraints());
    System.out.println("Number of constraints in the cloned model = " + modelCopy.numConstraints());
    // [END clone]

    // [START solver]
    // Create the solver with the CP-SAT backend and check it is supported.
    ModelSolver solver = new ModelSolver("sat");
    if (!solver.solverIsSupported()) {
      return;
    }
    // [END solver]

    // [START solve]
    final SolveStatus status = solver.solve(modelCopy);
    // [END solve]

    // [START print_solution]
    if (status == SolveStatus.OPTIMAL) {
      System.out.println("Solution:");
      System.out.println("Objective value = " + solver.getObjectiveValue());
      System.out.println("x = " + solver.getValue(xCopy));
      System.out.println("y = " + solver.getValue(yCopy));
      System.out.println("z = " + solver.getValue(zCopy));
    } else {
      System.err.println("The problem does not have an optimal solution!");
    }
    // [END print_solution]

    // [START advanced]
    System.out.println("\nAdvanced usage:");
    System.out.println("Problem solved in " + solver.getWallTime() + " seconds");
    // [END advanced]
  }

  private CloneModelMb() {}
}
// [END program]
