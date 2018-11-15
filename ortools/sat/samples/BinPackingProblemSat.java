// Copyright 2010-2018 Google LLC
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

import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;

/** Solves a bin packing problem with the CP-SAT solver. */
public class BinPackingProblemSat {
  static {
    System.loadLibrary("jniortools");
  }

  public static void main(String[] args) throws Exception {
    // Data.
    int binCapacity = 100;
    int slackCapacity = 20;
    int numBins = 5;

    int[][] items = new int[][] {{20, 6}, {15, 6}, {30, 4}, {45, 3}};
    int numItems = items.length;

    // Model.
    CpModel model = new CpModel();

    // Main variables.
    IntVar[][] x = new IntVar[numItems][numBins];
    for (int i = 0; i < numItems; ++i) {
      int numCopies = items[i][1];
      for (int b = 0; b < numBins; ++b) {
        x[i][b] = model.newIntVar(0, numCopies, "x_" + i + "_" + b);
      }
    }

    // Load variables.
    IntVar[] load = new IntVar[numBins];
    for (int b = 0; b < numBins; ++b) {
      load[b] = model.newIntVar(0, binCapacity, "load_" + b);
    }

    // Slack variables.
    IntVar[] slacks = new IntVar[numBins];
    for (int b = 0; b < numBins; ++b) {
      slacks[b] = model.newBoolVar("slack_" + b);
    }

    // Links load and x.
    int[] sizes = new int[numItems];
    for (int i = 0; i < numItems; ++i) {
      sizes[i] = items[i][0];
    }
    for (int b = 0; b < numBins; ++b) {
      IntVar[] vars = new IntVar[numItems];
      for (int i = 0; i < numItems; ++i) {
        vars[i] = x[i][b];
      }
      model.addScalProdEqual(vars, sizes, load[b]);
    }

    // Place all items.
    for (int i = 0; i < numItems; ++i) {
      IntVar[] vars = new IntVar[numBins];
      for (int b = 0; b < numBins; ++b) {
        vars[b] = x[i][b];
      }
      model.addLinearSumEqual(vars, items[i][1]);
    }

    // Links load and slack.
    int safeCapacity = binCapacity - slackCapacity;
    for (int b = 0; b < numBins; ++b) {
      //  slack[b] => load[b] <= safeCapacity.
      model.addLessOrEqual(load[b], safeCapacity).onlyEnforceIf(slacks[b]);
      // not(slack[b]) => load[b] > safeCapacity.
      model.addGreaterOrEqual(load[b], safeCapacity + 1).onlyEnforceIf(slacks[b].not());
    }

    // Maximize sum of slacks.
    model.maximizeSum(slacks);

    // Solves and prints out the solution.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);
    System.out.println("Solve status: " + status);
    if (status == CpSolverStatus.OPTIMAL) {
      System.out.printf("Optimal objective value: %f%n", solver.objectiveValue());
      for (int b = 0; b < numBins; ++b) {
        System.out.printf("load_%d = %d%n", b, solver.value(load[b]));
        for (int i = 0; i < numItems; ++i) {
          System.out.printf("  item_%d_%d = %d%n", i, b, solver.value(x[i][b]));
        }
      }
    }
    System.out.println("Statistics");
    System.out.println("  - conflicts : " + solver.numConflicts());
    System.out.println("  - branches  : " + solver.numBranches());
    System.out.println("  - wall time : " + solver.wallTime() + " s");
  }
}
