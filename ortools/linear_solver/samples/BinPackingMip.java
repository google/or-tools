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

// MIP example that solves a bin packing problem.
// [START program]
package com.google.ortools.linearsolver.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;
// [END import]

/** Bin packing problem. */
public class BinPackingMip {
  // [START program_part1]
  // [START data_model]
  static class DataModel {
    public final double[] weights = {48, 30, 19, 36, 36, 27, 42, 42, 36, 24, 30};
    public final int numItems = weights.length;
    public final int numBins = weights.length;
    public final int binCapacity = 100;
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
    MPVariable[][] x = new MPVariable[data.numItems][data.numBins];
    for (int i = 0; i < data.numItems; ++i) {
      for (int j = 0; j < data.numBins; ++j) {
        x[i][j] = solver.makeIntVar(0, 1, "");
      }
    }
    MPVariable[] y = new MPVariable[data.numBins];
    for (int j = 0; j < data.numBins; ++j) {
      y[j] = solver.makeIntVar(0, 1, "");
    }
    // [END variables]

    // [START constraints]
    double infinity = java.lang.Double.POSITIVE_INFINITY;
    for (int i = 0; i < data.numItems; ++i) {
      MPConstraint constraint = solver.makeConstraint(1, 1, "");
      for (int j = 0; j < data.numBins; ++j) {
        constraint.setCoefficient(x[i][j], 1);
      }
    }
    // The bin capacity contraint for bin j is
    //   sum_i w_i x_ij <= C*y_j
    // To define this constraint, first subtract the left side from the right to get
    //   0 <= C*y_j - sum_i w_i x_ij
    //
    // Note: Since sum_i w_i x_ij is positive (and y_j is 0 or 1), the right side must
    // be less than or equal to C. But it's not necessary to add this constraint
    // because it is forced by the other constraints.

    for (int j = 0; j < data.numBins; ++j) {
      MPConstraint constraint = solver.makeConstraint(0, infinity, "");
      constraint.setCoefficient(y[j], data.binCapacity);
      for (int i = 0; i < data.numItems; ++i) {
        constraint.setCoefficient(x[i][j], -data.weights[i]);
      }
    }
    // [END constraints]

    // [START objective]
    MPObjective objective = solver.objective();
    for (int j = 0; j < data.numBins; ++j) {
      objective.setCoefficient(y[j], 1);
    }
    objective.setMinimization();
    // [END objective]

    // [START solve]
    final MPSolver.ResultStatus resultStatus = solver.solve();
    // [END solve]

    // [START print_solution]
    // Check that the problem has an optimal solution.
    if (resultStatus == MPSolver.ResultStatus.OPTIMAL) {
      System.out.println("Number of bins used: " + objective.value());
      double totalWeight = 0;
      for (int j = 0; j < data.numBins; ++j) {
        if (y[j].solutionValue() == 1) {
          System.out.println("\nBin " + j + "\n");
          double binWeight = 0;
          for (int i = 0; i < data.numItems; ++i) {
            if (x[i][j].solutionValue() == 1) {
              System.out.println("Item " + i + " - weight: " + data.weights[i]);
              binWeight += data.weights[i];
            }
          }
          System.out.println("Packed bin weight: " + binWeight);
          totalWeight += binWeight;
        }
      }
      System.out.println("\nTotal packed weight: " + totalWeight);
    } else {
      System.err.println("The problem does not have an optimal solution.");
    }
    // [END print_solution]
  }
  private BinPackingMip() {}
}
// [END program_part2]
// [END program]
