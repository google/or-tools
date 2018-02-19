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


public class CoinsGrid {

  static {
    System.loadLibrary("jniortools");
  }

  /**
   *
   * Solves the Coins Grid problm.
   * See http://www.hakank.org/google_or_tools/coins_grid.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("CoinsGrid");

    //
    // data
    //
    int n = 31;
    int c = 14;

    //
    // variables
    //
    IntVar[][] x = new IntVar[n][n];
    IntVar[] x_flat = new IntVar[n * n];

    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        x[i][j] = solver.makeIntVar(0, 1, "x[" + i + "," + j + "]");
        x_flat[i * n + j] = x[i][j];
      }
    }

    //
    // constraints
    //

    // sum row/columns == c
    for(int i = 0; i < n; i++) {
      IntVar[] row = new IntVar[n];
      IntVar[] col = new IntVar[n];
      for(int j = 0; j < n; j++) {
        row[j] = x[i][j];
        col[j] = x[j][i];
      }
      solver.addConstraint(solver.makeSumEquality(row, c));
      solver.addConstraint(solver.makeSumEquality(col, c));
    }

    System.out.println("here1");

    // quadratic horizonal distance
    IntVar[] obj_tmp = new IntVar[n * n];
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        obj_tmp[i * n + j] =
          solver.makeProd(x[i][j],(i - j) * (i - j)).var();
      }
    }
    IntVar obj_var = solver.makeSum(obj_tmp).var();

    //
    // objective
    //
    OptimizeVar obj = solver.makeMinimize(obj_var, 1);

    //
    // search
    //
    DecisionBuilder db = solver.makePhase(x_flat,
                                          solver.CHOOSE_FIRST_UNBOUND,
                                          solver.ASSIGN_MAX_VALUE);

    solver.newSearch(db, obj);

    //
    // output
    //
    while (solver.nextSolution()) {
      System.out.println("obj_var: " + obj_var.value());
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
    CoinsGrid.solve();
  }
}
