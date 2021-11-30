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
package com.google.ortools.linearsolver.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;
// [END import]

/** Simple linear programming example. */
public final class LinearProgrammingExample {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // [START solver]
    MPSolver solver = MPSolver.createSolver("GLOP");
    // [END solver]

    // [START variables]
    double infinity = java.lang.Double.POSITIVE_INFINITY;
    // x and y are continuous non-negative variables.
    MPVariable x = solver.makeNumVar(0.0, infinity, "x");
    MPVariable y = solver.makeNumVar(0.0, infinity, "y");
    System.out.println("Number of variables = " + solver.numVariables());
    // [END variables]

    // [START constraints]
    // x + 2*y <= 14.
    MPConstraint c0 = solver.makeConstraint(-infinity, 14.0, "c0");
    c0.setCoefficient(x, 1);
    c0.setCoefficient(y, 2);

    // 3*x - y >= 0.
    MPConstraint c1 = solver.makeConstraint(0.0, infinity, "c1");
    c1.setCoefficient(x, 3);
    c1.setCoefficient(y, -1);

    // x - y <= 2.
    MPConstraint c2 = solver.makeConstraint(-infinity, 2.0, "c2");
    c2.setCoefficient(x, 1);
    c2.setCoefficient(y, -1);
    System.out.println("Number of constraints = " + solver.numConstraints());
    // [END constraints]

    // [START objective]
    // Maximize 3 * x + 4 * y.
    MPObjective objective = solver.objective();
    objective.setCoefficient(x, 3);
    objective.setCoefficient(y, 4);
    objective.setMaximization();
    // [END objective]

    // [START solve]
    final MPSolver.ResultStatus resultStatus = solver.solve();
    // [END solve]

    // [START print_solution]
    if (resultStatus == MPSolver.ResultStatus.OPTIMAL) {
      System.out.println("Solution:");
      System.out.println("Objective value = " + objective.value());
      System.out.println("x = " + x.solutionValue());
      System.out.println("y = " + y.solutionValue());
    } else {
      System.err.println("The problem does not have an optimal solution!");
    }
    // [END print_solution]

    // [START advanced]
    System.out.println("\nAdvanced usage:");
    System.out.println("Problem solved in " + solver.wallTime() + " milliseconds");
    System.out.println("Problem solved in " + solver.iterations() + " iterations");
    // [END advanced]
  }

  private LinearProgrammingExample() {}
}
// [END program]
