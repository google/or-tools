
/*
 * Copyright 2017 Darian Sastre darian.sastre@minimaxlabs.com
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ************************************************************************
 *
 * This model was created by Hakan Kjellerstrand (hakank@gmail.com)
 */
package com.google.ortools.contrib;

import com.google.ortools.Loader;
import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;

public class DietMIP {
  private static void solve(String solverType) {
    System.out.println("---- DietMIP with " + solverType);

    MPSolver solver = MPSolver.createSolver(solverType);
    if (solver == null)
      return;

    double infinity = MPSolver.infinity();

    int n = 4; // variables number
    int m = 4; // constraints number

    int[] price = {50, 20, 30, 80};

    int[] limits = {500, 6, 10, 8};

    int[] calories = {400, 200, 150, 500};
    int[] chocolate = {3, 2, 0, 0};
    int[] sugar = {2, 2, 4, 4};
    int[] fat = {2, 4, 1, 5};

    int[][] values = {calories, chocolate, sugar, fat};

    MPVariable[] x = solver.makeIntVarArray(n, 0, 100, "x");
    MPObjective objective = solver.objective();
    MPConstraint[] targets = new MPConstraint[4];

    for (int i = 0; i < n; i++) {
      objective.setCoefficient(x[i], price[i]);

      // constraints
      targets[i] = solver.makeConstraint(limits[i], infinity);
      for (int j = 0; j < m; j++) {
        targets[i].setCoefficient(x[j], values[i][j]);
      }
    }

    final MPSolver.ResultStatus resultStatus = solver.solve();

    /** printing */
    if (resultStatus != MPSolver.ResultStatus.OPTIMAL) {
      System.err.println("The problem does not have an optimal solution!");
      return;
    } else {
      System.out.println("Optimal objective value = " + solver.objective().value());

      System.out.print("Item quantities: ");
      System.out.print((int) x[0].solutionValue() + " ");
      System.out.print((int) x[1].solutionValue() + " ");
      System.out.print((int) x[2].solutionValue() + " ");
      System.out.print((int) x[3].solutionValue() + " ");
    }
  }

  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    solve("SCIP");
    solve("CBC");
  }
}
