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

// MIP example that solves a bin packing problem.
// [START program]
package com.google.ortools.linearsolver.samples;

// [START import]
import com.google.ortools.Loader;
import com.google.ortools.modelbuilder.LinearExpr;
import com.google.ortools.modelbuilder.LinearExprBuilder;
import com.google.ortools.modelbuilder.ModelBuilder;
import com.google.ortools.modelbuilder.ModelSolver;
import com.google.ortools.modelbuilder.SolveStatus;
import com.google.ortools.modelbuilder.Variable;

// [END import]

/** Bin packing problem. */
public class BinPackingMb {
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

    // [START model]
    ModelBuilder model = new ModelBuilder();
    // [END model]

    // [START program_part2]
    // [START variables]
    Variable[][] x = new Variable[data.numItems][data.numBins];
    for (int i = 0; i < data.numItems; ++i) {
      for (int j = 0; j < data.numBins; ++j) {
        x[i][j] = model.newBoolVar("");
      }
    }
    Variable[] y = new Variable[data.numBins];
    for (int j = 0; j < data.numBins; ++j) {
      y[j] = model.newBoolVar("");
    }
    // [END variables]

    // [START constraints]
    for (int i = 0; i < data.numItems; ++i) {
      LinearExprBuilder oneCopy = LinearExpr.newBuilder();
      for (int j = 0; j < data.numBins; ++j) {
        oneCopy.add(x[i][j]);
      }
      model.addEquality(oneCopy, 1);
    }

    // The bin capacity constraint for bin j is
    //   sum_i w_i x_ij <= C*y_j
    // To define this constraint, first subtract the left side from the right to get
    //   0 <= C*y_j - sum_i w_i x_ij
    //
    // Note: Since sum_i w_i x_ij is positive (and y_j is 0 or 1), the right side must
    // be less than or equal to C. But it's not necessary to add this constraint
    // because it is forced by the other constraints.
    for (int j = 0; j < data.numBins; ++j) {
      LinearExprBuilder load = LinearExpr.newBuilder();
      for (int i = 0; i < data.numItems; ++i) {
        load.addTerm(x[i][j], data.weights[i]);
      }
      model.addGreaterOrEqual(LinearExpr.term(y[j], data.binCapacity), load);
    }
    // [END constraints]

    // [START objective]
    model.minimize(LinearExpr.sum(y));
    // [END objective]

    // [START solver]
    // Create the solver with the SCIP backend and check it is supported.
    ModelSolver solver = new ModelSolver("scip");
    if (!solver.solverIsSupported()) {
      return;
    }
    // [END solver]

    // [START solve]
    final SolveStatus resultStatus = solver.solve(model);
    // [END solve]

    // [START print_solution]
    // Check that the problem has an optimal solution.
    if (resultStatus == SolveStatus.OPTIMAL) {
      System.out.println("Number of bins used: " + solver.getObjectiveValue());
      double totalWeight = 0;
      for (int j = 0; j < data.numBins; ++j) {
        if (solver.getValue(y[j]) == 1) {
          System.out.println("\nBin " + j + "\n");
          double binWeight = 0;
          for (int i = 0; i < data.numItems; ++i) {
            if (solver.getValue(x[i][j]) == 1) {
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

  private BinPackingMb() {}
}
// [END program_part2]
// [END program]
