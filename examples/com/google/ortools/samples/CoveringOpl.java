// Copyright 2011 Hakan Kjellerstrand hakank@gmail.com
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

package com.google.ortools.samples;

import java.io.*;
import java.util.*;
import java.text.*;

import com.google.ortools.constraintsolver.DecisionBuilder;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.Solver;
import com.google.ortools.constraintsolver.OptimizeVar;

public class CoveringOpl {

  static {
    System.loadLibrary("jniortools");
  }

  /**
   *
   * Solves a set covering problem.
   * See http://www.hakank.org/google_or_tools/covering_opl.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("CoveringOpl");

    //
    // data
    //
    int num_workers = 32;
    int num_tasks = 15;

    // Which worker is qualified for each task.
    // Note: This is 1-based and will be made 0-base below.
    int[][] qualified =  {{ 1,  9, 19,  22,  25,  28,  31 },
                          { 2, 12, 15, 19, 21, 23, 27, 29, 30, 31, 32 },
                          { 3, 10, 19, 24, 26, 30, 32 },
                          { 4, 21, 25, 28, 32 },
                          { 5, 11, 16, 22, 23, 27, 31 },
                          { 6, 20, 24, 26, 30, 32 },
                          { 7, 12, 17, 25, 30, 31 } ,
                          { 8, 17, 20, 22, 23  },
                          { 9, 13, 14,  26, 29, 30, 31 },
                          { 10, 21, 25, 31, 32 },
                          { 14, 15, 18, 23, 24, 27, 30, 32 },
                          { 18, 19, 22, 24, 26, 29, 31 },
                          { 11, 20, 25, 28, 30, 32 },
                          { 16, 19, 23, 31 },
                          { 9, 18, 26, 28, 31, 32 }};

    int[] cost = {1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3,
                  3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 8, 9};


    //
    // variables
    //
    IntVar[] hire = solver.makeIntVarArray(num_workers, 0, 1, "workers");
    IntVar total_cost = solver.makeScalProd(hire, cost).var();

    //
    // constraints
    //
    for(int j = 0; j < num_tasks; j++) {
      // Sum the cost for hiring the qualified workers
      // (also, make 0-base).
      int len = qualified[j].length;
      IntVar[] tmp = new IntVar[len];
      for(int c = 0; c < len; c++) {
        tmp[c] = hire[qualified[j][c] - 1];
      }
      IntVar b = solver.makeSum(tmp).var();
      solver.addConstraint(solver.makeGreaterOrEqual(b, 1));
    }


    // Objective: Minimize total cost
    OptimizeVar objective = solver.makeMinimize(total_cost, 1);

    //
    // search
    //
    DecisionBuilder db = solver.makePhase(hire,
                                          solver.CHOOSE_FIRST_UNBOUND,
                                          solver.ASSIGN_MIN_VALUE);

    solver.newSearch(db, objective);

    //
    // output
    //
    while (solver.nextSolution()) {
      System.out.println("Cost: " + total_cost.value());
      System.out.print("Hire: ");
      for(int i = 0; i < num_workers; i++) {
        if (hire[i].value() == 1) {
          System.out.print(i + " ");
        }
      }
      System.out.println("\n");

    }

    solver.endSearch();

    // Statistics
    System.out.println();
    System.out.println("Solutions: " + solver.solutions());
    System.out.println("Failures: " + solver.failures());
    System.out.println("Branches: " + solver.branches());
    System.out.println("Wall time: " + solver.wallTime() + "ms");

  }

  public static void main(String[] args) throws Exception {

    CoveringOpl.solve();

  }
}
