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

// MIP example that uses a variable array.
// [START program]
package com.google.ortools.linearsolver.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;
// [END import]

// [START program_part1]
/** MIP example with a variable array. */
public class MipVarArray {
  // [START data_model]
  static class DataModel {
    public final double[][] constraintCoeffs = {
        {5, 7, 9, 2, 1},
        {18, 4, -9, 10, 12},
        {4, 7, 3, 8, 5},
        {5, 13, 16, 3, -7},
    };
    public final double[] bounds = {250, 285, 211, 315};
    public final double[] objCoeffs = {7, 8, 2, 9, 6};
    public final int numVars = 5;
    public final int numConstraints = 4;
  }
  // [END data_model]

  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // [START data]
    final DataModel data = new DataModel();
    // [END data]
    // [END program_part1]

    // [START solver]
    // Create the linear solver with the SCIP backend.
    MPSolver solver = MPSolver.createSolver("SCIP");
    if (solver == null) {
      System.out.println("Could not create solver SCIP");
      return;
    }
    // [END solver]

    // [START program_part2]
    // [START variables]
    double infinity = java.lang.Double.POSITIVE_INFINITY;
    MPVariable[] x = new MPVariable[data.numVars];
    for (int j = 0; j < data.numVars; ++j) {
      x[j] = solver.makeIntVar(0.0, infinity, "");
    }
    System.out.println("Number of variables = " + solver.numVariables());
    // [END variables]

    // [START constraints]
    // Create the constraints.
    for (int i = 0; i < data.numConstraints; ++i) {
      MPConstraint constraint = solver.makeConstraint(0, data.bounds[i], "");
      for (int j = 0; j < data.numVars; ++j) {
        constraint.setCoefficient(x[j], data.constraintCoeffs[i][j]);
      }
    }
    System.out.println("Number of constraints = " + solver.numConstraints());
    // [END constraints]

    // [START objective]
    MPObjective objective = solver.objective();
    for (int j = 0; j < data.numVars; ++j) {
      objective.setCoefficient(x[j], data.objCoeffs[j]);
    }
    objective.setMaximization();
    // [END objective]

    // [START solve]
    final MPSolver.ResultStatus resultStatus = solver.solve();
    // [END solve]

    // [START print_solution]
    // Check that the problem has an optimal solution.
    if (resultStatus == MPSolver.ResultStatus.OPTIMAL) {
      System.out.println("Objective value = " + objective.value());
      for (int j = 0; j < data.numVars; ++j) {
        System.out.println("x[" + j + "] = " + x[j].solutionValue());
      }
      System.out.println();
      System.out.println("Problem solved in " + solver.wallTime() + " milliseconds");
      System.out.println("Problem solved in " + solver.iterations() + " iterations");
      System.out.println("Problem solved in " + solver.nodes() + " branch-and-bound nodes");
    } else {
      System.err.println("The problem does not have an optimal solution.");
    }
    // [END print_solution]
  }

  private MipVarArray() {}
}
// [END program_part2]
// [END program]
