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

public class Strimko2 {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves a Strimko problem.
   * See http://www.hakank.org/google_or_tools/strimko2.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("Strimko2");

    //
    // data
    //
    int[][] streams = {{1,1,2,2,2,2,2},
                       {1,1,2,3,3,3,2},
                       {1,4,1,3,3,5,5},
                       {4,4,3,1,3,5,5},
                       {4,6,6,6,7,7,5},
                       {6,4,6,4,5,5,7},
                       {6,6,4,7,7,7,7}};

    // Note: This is 1-based
    int[][] placed = {{2,1,1},
                      {2,3,7},
                      {2,5,6},
                      {2,7,4},
                      {3,2,7},
                      {3,6,1},
                      {4,1,4},
                      {4,7,5},
                      {5,2,2},
                      {5,6,6}};

    int n = streams.length;
    int num_placed = placed.length;



    //
    // variables
    //

    IntVar[][] x = new IntVar[n][n];
    IntVar[] x_flat = new IntVar[n * n];
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        x[i][j] = solver.makeIntVar(1, n, "x[" + i + "," + j + "]");
        x_flat[i * n + j] = x[i][j];
      }
    }

    //
    // constraints
    //

    // all rows and columns must be unique, i.e. a Latin Square
    for(int i = 0; i < n; i++) {

      IntVar[] row = new IntVar[n];
      IntVar[] col = new IntVar[n];
      for(int j = 0; j < n; j++) {
        row[j] = x[i][j];
        col[j] = x[j][i];
      }

      solver.addConstraint(solver.makeAllDifferent(row));
      solver.addConstraint(solver.makeAllDifferent(col));
    }


    // streams
    for(int s = 1; s <= n; s++) {
      ArrayList<IntVar> tmp = new ArrayList<IntVar>();
      for(int i = 0; i < n; i++) {
        for(int j = 0; j < n; j++) {
          if (streams[i][j] == s) {
            tmp.add(x[i][j]);
          }
        }
      }
      solver.addConstraint(
          solver.makeAllDifferent(tmp.toArray(new IntVar[1])));
    }


    // placed
    for(int i = 0; i <  num_placed; i++) {
      // note: also adjust to 0-based
      solver.addConstraint(
          solver.makeEquality(x[placed[i][0] - 1][placed[i][1] - 1],
                              placed[i][2]));
    }

    //
    // search
    //
    DecisionBuilder db = solver.makePhase(x_flat,
                                          solver.INT_VAR_DEFAULT,
                                          solver.INT_VALUE_DEFAULT);
    solver.newSearch(db);

    //
    // output
    //
    while (solver.nextSolution()) {
      for(int i = 0; i < n; i++) {
        for(int j = 0; j < n; j++) {
          System.out.print(x[i][j].value() + " ");
        }
        System.out.println();
      }
      System.out.println();

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

    Strimko2.solve();

  }
}
