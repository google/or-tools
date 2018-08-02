// Copyright 2010-2017 Google
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

import com.google.ortools.sat.*;

public class BinpackingProblem {

  static {
    System.loadLibrary("jniortools");
  }
  static void BinpackingProblem() {
    // Data.
    int bin_capacity = 100;
    int slack_capacity = 20;
    int num_bins = 5;

    int[][] items = new int[][] { {20, 6}, {15, 6}, {30, 4}, {45, 3} };
    int num_items = items.length;

    // Model.
    CpModel model = new CpModel();

    // Main variables.
    IntVar[][] x = new IntVar[num_items][num_bins];
    for (int i = 0; i < num_items; ++i) {
      int num_copies = items[i][1];
      for (int b = 0; b < num_bins; ++b) {
        x[i][b] = model.newIntVar(0, num_copies, "x_" + i + "_" + b);
      }
    }

    // Load variables.
    IntVar[] load = new IntVar[num_bins];
    for (int b = 0; b < num_bins; ++b) {
      load[b] = model.newIntVar(0, bin_capacity, "load_" + b);
    }

    // Slack variables.
    IntVar[] slacks = new IntVar[num_bins];
    for (int b = 0; b < num_bins; ++b) {
      slacks[b] = model.newBoolVar("slack_" +  b);
    }

    // Links load and x.
    int[] sizes = new int[num_items];
    for (int i = 0; i < num_items; ++i) {
      sizes[i] = items[i][0];
    }
    for (int b = 0; b < num_bins; ++b) {
      IntVar[] vars = new IntVar[num_items];
      for (int i = 0; i < num_items; ++i) {
        vars[i] = x[i][b];
      }
      model.addScalProdEqual(vars, sizes, load[b]);
    }

    // Place all items.
    for (int i = 0; i < num_items; ++i) {
      IntVar[] vars = new IntVar[num_bins];
      for (int b = 0; b < num_bins; ++b) {
        vars[b] = x[i][b];
      }
      model.addLinearSumEqual(vars, items[i][1]);
    }

    // Links load and slack.
    int safe_capacity = bin_capacity - slack_capacity;
    for (int b = 0; b < num_bins; ++b) {
      //  slack[b] => load[b] <= safe_capacity.
      model.addLessOrEqual(load[b], safe_capacity).onlyEnforceIf(slacks[b]);
      // not(slack[b]) => load[b] > safe_capacity.
      model.addGreaterOrEqual(load[b], safe_capacity + 1).onlyEnforceIf(
          slacks[b].not());
    }

    // Maximize sum of slacks.
    model.MaximizeSum(slacks);

    // Solves and prints out the solution.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);
    System.out.println("Solve status: " + status);
    if (status == CpSolverStatus.OPTIMAL) {
      System.out.println(String.format("Optimal objective value: %f",
                                       solver.objectiveValue()));
      for (int b = 0; b < num_bins; ++b)
      {
        System.out.println(String.format("load_%d = %d",
                                         b, solver.value(load[b])));
        for (int i = 0; i < num_items; ++i)
        {
          System.out.println(String.format("  item_%d_%d = %d",
                                           i, b, solver.value(x[i][b])));
        }
      }
    }
    System.out.println("Statistics");
    System.out.println("  - conflicts : " + solver.numConflicts());
    System.out.println("  - branches  : " + solver.numBranches());
    System.out.println("  - wall time : " + solver.wallTime() + " s");
  }

  public static void main(String[] args) throws Exception {
    BinpackingProblem();
  }
}
