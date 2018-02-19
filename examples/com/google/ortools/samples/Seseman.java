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

public class Seseman {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves the Seseman convent problem.
   * See http://www.hakank.org/google_or_tools/seseman.py
   *
   */
  private static void solve(int n) {

    Solver solver = new Solver("Seseman");

    //
    // data
    //
    final int border_sum = n * n;

    //
    // variables
    //
    IntVar[][] x = new IntVar[n][n];
    IntVar[] x_flat = new IntVar[n * n];
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        x[i][j] = solver.makeIntVar(0, n * n);
        x_flat[i * n + j] = x[i][j];
      }
    }

    IntVar total_sum = solver.makeSum(x_flat).var();


    //
    // constraints
    //

    // zero in all middle cells
    for(int i = 1; i < n-1; i++) {
      for(int j = 1; j < n-1; j++) {
        solver.addConstraint(solver.makeEquality(x[i][j], 0));
      }
    }

    // all borders must be >= 1
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        if (i == 0 || j == 0 || i == n - 1 || j == n - 1) {
          solver.addConstraint(solver.makeGreaterOrEqual(x[i][j], 1));
        }
      }
    }

    // sum the four borders
    IntVar[] border1 = new IntVar[n];
    IntVar[] border2 = new IntVar[n];
    IntVar[] border3 = new IntVar[n];
    IntVar[] border4 = new IntVar[n];
    for(int i = 0; i < n; i++) {
      border1[i] = x[i][0];
      border2[i] = x[i][n - 1];
      border3[i] = x[0][i];
      border4[i] = x[n - 1][i];
    }
    solver.addConstraint(
        solver.makeSumEquality(border1, border_sum));

    solver.addConstraint(
        solver.makeSumEquality(border2, border_sum));

    solver.addConstraint(
        solver.makeSumEquality(border3, border_sum));

    solver.addConstraint(
        solver.makeSumEquality(border4, border_sum));


    //
    // search
    //

    DecisionBuilder db = solver.makePhase(x_flat,
                                          solver.INT_VAR_DEFAULT,
                                          solver.INT_VALUE_DEFAULT);
    solver.newSearch(db);

    while (solver.nextSolution()) {
      System.out.println("total_sum: " + total_sum.value());
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
    int n = 3;
    if (args.length > 0) {
      n = Integer.parseInt(args[0]);
    }
    Seseman.solve(n);
  }
}
