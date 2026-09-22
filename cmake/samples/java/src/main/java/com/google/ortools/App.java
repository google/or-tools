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

package com.google.ortools;

import com.google.ortools.init.OrToolsVersion;
import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;

/** Hello world! */
public final class App {
  public static void main(String[] args) {
    System.out.println("Hello World!");
    Loader.loadNativeLibraries();
    System.out.println("Google OR-Tools version: " + OrToolsVersion.getVersionString());

    MPSolver solver = MPSolver.createSolver("GLOP");
    if (solver == null) {
      throw new AssertionError("Could not create solver GLOP");
    }
    MPVariable x = solver.makeNumVar(0.0, 1.0, "x");
    MPVariable y = solver.makeNumVar(0.0, 2.0, "y");
    System.out.println("Number of variables = " + solver.numVariables());

    // Create a linear constraint, x + y <= 2.
    double infinity = Double.POSITIVE_INFINITY;
    MPConstraint ct = solver.makeConstraint(-infinity, 2.0, "ct");
    ct.setCoefficient(x, 1);
    ct.setCoefficient(y, 1);
    System.out.println("Number of constraints = " + solver.numConstraints());

    // Create the objective function, 3 * x + y.
    MPObjective objective = solver.objective();
    objective.setCoefficient(x, 3);
    objective.setCoefficient(y, 1);
    objective.setMaximization();

    System.out.println("Solving with " + solver.solverVersion());
    final MPSolver.ResultStatus resultStatus = solver.solve();
    System.out.println("Status: " + resultStatus);
    if (resultStatus != MPSolver.ResultStatus.OPTIMAL) {
      System.out.println("The problem does not have an optimal solution!");
      if (resultStatus == MPSolver.ResultStatus.FEASIBLE) {
        System.out.println("A potentially suboptimal solution was found");
      } else {
        System.out.println("The solver could not solve the problem.");
        return;
      }
    }

    System.out.println("Solution:");
    System.out.println("Objective value = " + objective.value());
    System.out.println("x = " + x.solutionValue());
    System.out.println("y = " + y.solutionValue());

    System.out.println("Advanced usage:");
    System.out.println("Problem solved in " + solver.wallTime() + " milliseconds");
    System.out.println("Problem solved in " + solver.iterations() + " iterations");
  }
}
