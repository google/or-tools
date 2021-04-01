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
package com.google.ortools.sat.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;
// [END import]

/** Sample showing how to solve a multiple knapsack problem. */
public class MultipleKnapsackSat {
  // [START data]
  static class DataModel {
    int[] items = new int[] {48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36};
    int[] values = new int[] {10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25};
    int[] binCapacities = new int[] {100, 100, 100, 100, 100};
    int numItems = items.length;
    int numBins = 5;
  }
  // [END data]

  // [START solution_printer]
  static void printSolution(
      DataModel data, CpSolver solver, IntVar[][] x, IntVar[] load, IntVar[] value) {
    System.out.printf("Optimal objective value: %f%n", solver.objectiveValue());
    System.out.println();
    long packedWeight = 0;
    long packedValue = 0;
    for (int b = 0; b < data.numBins; ++b) {
      System.out.println("Bin " + b);
      for (int i = 0; i < data.numItems; ++i) {
        if (solver.value(x[i][b]) > 0) {
          System.out.println(
              "Item " + i + "  -  Weight: " + data.items[i] + "  Value: " + data.values[i]);
        }
      }
      System.out.println("Packed bin weight: " + solver.value(load[b]));
      packedWeight = packedWeight + solver.value(load[b]);
      System.out.println("Packed bin value: " + solver.value(value[b]) + "\n");
      packedValue = packedValue + solver.value(value[b]);
    }
    System.out.println("Total packed weight: " + packedWeight);
    System.out.println("Total packed value: " + packedValue);
  }

  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Instantiate the data problem.
    // [START data]
    final DataModel data = new DataModel();
    // [END data]
    int totalValue = 0;
    for (int i = 0; i < data.numItems; ++i) {
      totalValue = totalValue + data.values[i];
    }

    // [START model]
    CpModel model = new CpModel();
    // [END model]

    // [START variables]
    IntVar[][] x = new IntVar[data.numItems][data.numBins];
    for (int i = 0; i < data.numItems; ++i) {
      for (int b = 0; b < data.numBins; ++b) {
        x[i][b] = model.newIntVar(0, 1, "x_" + i + "_" + b);
      }
    }
    // Main variables.
    // Load and value variables.
    IntVar[] load = new IntVar[data.numBins];
    IntVar[] value = new IntVar[data.numBins];
    for (int b = 0; b < data.numBins; ++b) {
      load[b] = model.newIntVar(0, data.binCapacities[b], "load_" + b);
      value[b] = model.newIntVar(0, totalValue, "value_" + b);
    }

    // Links load and value with x.
    int[] sizes = new int[data.numItems];
    for (int i = 0; i < data.numItems; ++i) {
      sizes[i] = data.items[i];
    }
    for (int b = 0; b < data.numBins; ++b) {
      IntVar[] vars = new IntVar[data.numItems];
      for (int i = 0; i < data.numItems; ++i) {
        vars[i] = x[i][b];
      }
      model.addEquality(LinearExpr.scalProd(vars, data.items), load[b]);
      model.addEquality(LinearExpr.scalProd(vars, data.values), value[b]);
    }
    // [END variables]

    // [START constraints]
    // Each item can be in at most one bin.
    // Place all items.
    for (int i = 0; i < data.numItems; ++i) {
      IntVar[] vars = new IntVar[data.numBins];
      for (int b = 0; b < data.numBins; ++b) {
        vars[b] = x[i][b];
      }
      model.addLessOrEqual(LinearExpr.sum(vars), 1);
    }
    // [END constraints]
    // Maximize sum of load.
    // [START objective]
    model.maximize(LinearExpr.sum(value));
    // [END objective]

    // [START solve]
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);
    // [END solve]

    // [START print_solution]
    System.out.println("Solve status: " + status);
    if (status == CpSolverStatus.OPTIMAL) {
      printSolution(data, solver, x, load, value);
    }
    // [END print_solution]
  }

  private MultipleKnapsackSat() {}
}
