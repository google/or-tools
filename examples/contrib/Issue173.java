package com.google.ortools.contrib;

import com.google.ortools.Loader;
import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;

public class Issue173 {
  public static void breakit() {
    for (int i = 0; i < 50000; i++) {
      solveLP();
    }
  }

  private static void solveLP() {
    MPSolver solver = MPSolver.createSolver("CBC");
    if (solver == null) {
      System.out.println("Could not create solver CBC");
      return;
    }
    MPVariable x = solver.makeNumVar(Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY, "x");

    final MPObjective objective = solver.objective();
    objective.setMaximization();
    objective.setCoefficient(x, 1);

    MPConstraint constraint = solver.makeConstraint(0, 5);
    constraint.setCoefficient(x, 1);

    solver.solve();
  }

  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    breakit();
  }
}
