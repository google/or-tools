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

// MIP example that solves a multiple knapsack problem.
// [START program]
package com.google.ortools.linearsolver.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;
// [END import]

/** Multiple knapsack problem. */
public class MultipleKnapsackMip {
  // [START program_part1]
  // [START data_model]
  static class DataModel {
    public final double[] weights = {48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36};
    public final double[] values = {10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25};
    public final int numItems = weights.length;
    public final int numBins = 5;
    public final double[] binCapacities = {100, 100, 100, 100, 100};
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
    // [END variables]

    // [START constraints]
    for (int i = 0; i < data.numItems; ++i) {
      MPConstraint constraint = solver.makeConstraint(0, 1, "");
      for (int j = 0; j < data.numBins; ++j) {
        constraint.setCoefficient(x[i][j], 1);
      }
    }
    for (int j = 0; j < data.numBins; ++j) {
      MPConstraint constraint = solver.makeConstraint(0, data.binCapacities[j], "");
      for (int i = 0; i < data.numItems; ++i) {
        constraint.setCoefficient(x[i][j], data.weights[i]);
      }
    }
    // [END constraints]

    // [START objective]
    MPObjective objective = solver.objective();
    for (int i = 0; i < data.numItems; ++i) {
      for (int j = 0; j < data.numBins; ++j) {
        objective.setCoefficient(x[i][j], data.values[i]);
      }
    }
    objective.setMaximization();
    // [END objective]

    // [START solve]
    final MPSolver.ResultStatus resultStatus = solver.solve();
    // [END solve]

    // [START print_solution]
    // Check that the problem has an optimal solution.
    if (resultStatus == MPSolver.ResultStatus.OPTIMAL) {
      System.out.println("Total packed value: " + objective.value() + "\n");
      double totalWeight = 0;
      for (int j = 0; j < data.numBins; ++j) {
        double binWeight = 0;
        double binValue = 0;
        System.out.println("Bin " + j + "\n");
        for (int i = 0; i < data.numItems; ++i) {
          if (x[i][j].solutionValue() == 1) {
            System.out.println(
                "Item " + i + " - weight: " + data.weights[i] + "  value: " + data.values[i]);
            binWeight += data.weights[i];
            binValue += data.values[i];
          }
        }
        System.out.println("Packed bin weight: " + binWeight);
        System.out.println("Packed bin value: " + binValue + "\n");
        totalWeight += binWeight;
      }
      System.out.println("Total packed weight: " + totalWeight);
    } else {
      System.err.println("The problem does not have an optimal solution.");
    }
    // [END print_solution]
  }

  private MultipleKnapsackMip() {}
}
// [END program_part2]
// [END program]
