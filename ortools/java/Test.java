package com.google.ortools;

import com.google.ortools.Loader;

import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;

/**
 * @author Mizux
 */
public class Test {
  private static void testLP() {
    MPSolver solver =
      new MPSolver("SimpleLpProgram", MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING);
    MPVariable x = solver.makeNumVar(0.0, 1.0, "x");
    MPVariable y = solver.makeNumVar(0.0, 2.0, "y");
    System.out.println("Number of variables = " + solver.numVariables());
    MPConstraint ct = solver.makeConstraint(0.0, 2.0, "ct");
    ct.setCoefficient(x, 1);
    ct.setCoefficient(y, 1);
    System.out.println("Number of constraints = " + solver.numConstraints());
    MPObjective objective = solver.objective();
    objective.setCoefficient(x, 3);
    objective.setCoefficient(y, 1);
    objective.setMaximization();
    solver.solve();
    System.out.println("Solution:");
    System.out.println("Objective value = " + objective.value());
    System.out.println("x = " + x.solutionValue());
    System.out.println("y = " + y.solutionValue());
  }

  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    testLP();
  }
}

