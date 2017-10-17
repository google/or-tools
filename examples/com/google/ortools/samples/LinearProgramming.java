// Copyright 2010-2017 Google
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


package com.google.ortools.samples;

import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;

/**
 * Linear programming example that shows how to use the API.
 *
 */

public class LinearProgramming {
  static { System.loadLibrary("jniortools"); }

  private static MPSolver createSolver (String solverType) {
    try {
      return new MPSolver("LinearProgrammingExample",
                          MPSolver.OptimizationProblemType.valueOf(solverType));
    } catch (java.lang.IllegalArgumentException e) {
      return null;
    }
  }

  private static void runLinearProgrammingExample(String solverType,
                                                  boolean printModel) {
    MPSolver solver = createSolver(solverType);
    if (solver == null) {
      System.out.println("Could not create solver " + solverType);
      return;
    }
    double infinity = MPSolver.infinity();
    // x1, x2 and x3 are continuous non-negative variables.
    MPVariable x1 = solver.makeNumVar(0.0, infinity, "x1");
    MPVariable x2 = solver.makeNumVar(0.0, infinity, "x2");
    MPVariable x3 = solver.makeNumVar(0.0, infinity, "x3");

    // Maximize 10 * x1 + 6 * x2 + 4 * x3.
    MPObjective objective = solver.objective();
    objective.setCoefficient(x1, 10);
    objective.setCoefficient(x2, 6);
    objective.setCoefficient(x3, 4);
    objective.setMaximization();

    // x1 + x2 + x3 <= 100.
    MPConstraint c0 = solver.makeConstraint(-infinity, 100.0);
    c0.setCoefficient(x1, 1);
    c0.setCoefficient(x2, 1);
    c0.setCoefficient(x3, 1);

    // 10 * x1 + 4 * x2 + 5 * x3 <= 600.
    MPConstraint c1 = solver.makeConstraint(-infinity, 600.0);
    c1.setCoefficient(x1, 10);
    c1.setCoefficient(x2, 4);
    c1.setCoefficient(x3, 5);

    // 2 * x1 + 2 * x2 + 6 * x3 <= 300.
    MPConstraint c2 = solver.makeConstraint(-infinity, 300.0);
    c2.setCoefficient(x1, 2);
    c2.setCoefficient(x2, 2);
    c2.setCoefficient(x3, 6);

    System.out.println("Number of variables = " + solver.numVariables());
    System.out.println("Number of constraints = " + solver.numConstraints());

    if (printModel) {
      String model = solver.exportModelAsLpFormat(false);
      System.out.println(model);
    }

    final MPSolver.ResultStatus resultStatus = solver.solve();

    // Check that the problem has an optimal solution.
    if (resultStatus != MPSolver.ResultStatus.OPTIMAL) {
      System.err.println("The problem does not have an optimal solution!");
      return;
    }

    // Verify that the solution satisfies all constraints (when using solvers
    // others than GLOP_LINEAR_PROGRAMMING, this is highly recommended!).
    if (!solver.verifySolution(/*tolerance=*/1e-7, /*logErrors=*/true)) {
      System.err.println("The solution returned by the solver violated the"
                         + " problem constraints by at least 1e-7");
      return;
    }

    System.out.println("Problem solved in " + solver.wallTime() + " milliseconds");

    // The objective value of the solution.
    System.out.println("Optimal objective value = " + solver.objective().value());

    // The value of each variable in the solution.
    System.out.println("x1 = " + x1.solutionValue());
    System.out.println("x2 = " + x2.solutionValue());
    System.out.println("x3 = " + x3.solutionValue());

    final double[] activities = solver.computeConstraintActivities();

    System.out.println("Advanced usage:");
    System.out.println("Problem solved in " + solver.iterations() + " iterations");
    System.out.println("x1: reduced cost = " + x1.reducedCost());
    System.out.println("x2: reduced cost = " + x2.reducedCost());
    System.out.println("x3: reduced cost = " + x3.reducedCost());
    System.out.println("c0: dual value = " + c0.dualValue());
    System.out.println("    activity = " + activities[c0.index()]);
    System.out.println("c1: dual value = " + c1.dualValue());
    System.out.println("    activity = " + activities[c1.index()]);
    System.out.println("c2: dual value = " + c2.dualValue());
    System.out.println("    activity = " + activities[c2.index()]);
  }

  public static void main(String[] args) throws Exception {
    System.out.println("---- Linear programming example with GLOP (recommended) ----");
    runLinearProgrammingExample("GLOP_LINEAR_PROGRAMMING", true);
    System.out.println("---- Linear programming example with CLP ----");
    runLinearProgrammingExample("CLP_LINEAR_PROGRAMMING", false);
    System.out.println("---- Linear programming example with GLPK ----");
    runLinearProgrammingExample("GLPK_LINEAR_PROGRAMMING", false);
  }
}
