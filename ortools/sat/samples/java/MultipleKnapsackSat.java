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

// [START program]
// Solves a multiple knapsack problem using the CP-SAT solver.
package com.google.ortools.sat.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.LinearExprBuilder;
import com.google.ortools.sat.Literal;
import java.util.ArrayList;
import java.util.List;
import java.util.stream.IntStream;
// [END import]

/** Sample showing how to solve a multiple knapsack problem. */
public class MultipleKnapsackSat {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // Instantiate the data problem.
    // [START data]
    final int[] weights = {48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36};
    final int[] values = {10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25};
    final int numItems = weights.length;
    final int[] allItems = IntStream.range(0, numItems).toArray();

    final int[] binCapacities = {100, 100, 100, 100, 100};
    final int numBins = binCapacities.length;
    final int[] allBins = IntStream.range(0, numBins).toArray();
    // [END data]

    // [START model]
    CpModel model = new CpModel();
    // [END model]

    // Variables.
    // [START variables]
    Literal[][] x = new Literal[numItems][numBins];
    for (int i : allItems) {
      for (int b : allBins) {
        x[i][b] = model.newBoolVar("x_" + i + "_" + b);
      }
    }
    // [END variables]

    // Constraints.
    // [START constraints]
    // Each item is assigned to at most one bin.
    for (int i : allItems) {
      List<Literal> bins = new ArrayList<>();
      for (int b : allBins) {
        bins.add(x[i][b]);
      }
      model.addAtMostOne(bins);
    }

    // The amount packed in each bin cannot exceed its capacity.
    for (int b : allBins) {
      LinearExprBuilder load = LinearExpr.newBuilder();
      for (int i : allItems) {
        load.addTerm(x[i][b], weights[i]);
      }
      model.addLessOrEqual(load, binCapacities[b]);
    }
    // [END constraints]

    // Objective.
    // [START objective]
    // Maximize total value of packed items.
    LinearExprBuilder obj = LinearExpr.newBuilder();
    for (int i : allItems) {
      for (int b : allBins) {
        obj.addTerm(x[i][b], values[i]);
      }
    }
    model.maximize(obj);
    // [END objective]

    // [START solve]
    CpSolver solver = new CpSolver();
    final CpSolverStatus status = solver.solve(model);
    // [END solve]

    // [START print_solution]
    // Check that the problem has an optimal solution.
    if (status == CpSolverStatus.OPTIMAL) {
      System.out.println("Total packed value: " + solver.objectiveValue());
      long totalWeight = 0;
      for (int b : allBins) {
        long binWeight = 0;
        long binValue = 0;
        System.out.println("Bin " + b);
        for (int i : allItems) {
          if (solver.booleanValue(x[i][b])) {
            System.out.println("Item " + i + " weight: " + weights[i] + " value: " + values[i]);
            binWeight += weights[i];
            binValue += values[i];
          }
        }
        System.out.println("Packed bin weight: " + binWeight);
        System.out.println("Packed bin value: " + binValue);
        totalWeight += binWeight;
      }
      System.out.println("Total packed weight: " + totalWeight);
    } else {
      System.err.println("The problem does not have an optimal solution.");
    }
    // [END print_solution]
  }

  private MultipleKnapsackSat() {}
}
// [END program]
