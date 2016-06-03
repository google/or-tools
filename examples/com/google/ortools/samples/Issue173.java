package com.google.ortools.samples;

import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;

public class Issue173 {

  static {
    System.loadLibrary("jniortools");
  }

  public static void breakit() {

    for (int i = 0; i < 50000; i++) {
      solveLP();
    }
  }

  private static void solveLP() {
    MPSolver solver = new MPSolver(
        "test",
        MPSolver.OptimizationProblemType.CBC_MIXED_INTEGER_PROGRAMMING);
    MPVariable x = solver.makeNumVar(Double.NEGATIVE_INFINITY,
                                     Double.POSITIVE_INFINITY, "x");

    final MPObjective objective = solver.objective();
    objective.setMaximization();
    objective.setCoefficient(x, 1);

    MPConstraint constraint = solver.makeConstraint(0, 5);
    constraint.setCoefficient(x, 1);

    solver.solve();
  }

  public static void main(String[] args) throws Exception {
    breakit();
  }
}
