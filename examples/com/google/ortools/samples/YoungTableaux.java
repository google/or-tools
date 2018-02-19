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

public class YoungTableaux {

  static {
    System.loadLibrary("jniortools");
  }



  /**
   *
   * Implements Young tableaux and partitions.
   * See http://www.hakank.org/google_or_tools/young_tableuax.py
   *
   */
  private static void solve(int n) {

    Solver solver = new Solver("YoungTableaux");

    System.out.println("n: " + n);

    //
    // variables
    //
    IntVar[][] x =  new IntVar[n][n];
    IntVar[] x_flat = new IntVar[n * n];

    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        x[i][j] = solver.makeIntVar(1, n + 1, "x[" + i + "," + j + "]");
        x_flat[i * n + j] = x[i][j];
      }
    }

    // partition structure
    IntVar[] p = solver.makeIntVarArray(n, 0, n + 1, "p");

    //
    // constraints
    //

    // 1..n is used exactly once
    for(int i = 1; i <= n; i++) {
      solver.addConstraint(solver.makeCount(x_flat, i, 1));
    }

    solver.addConstraint(solver.makeEquality(x[0][0], 1));

    // row wise
    for(int i = 0; i < n; i++) {
      for(int j = 1; j < n; j++) {
        solver.addConstraint(
            solver.makeGreaterOrEqual(x[i][j], x[i][j - 1]));
      }
    }

    // column wise
    for(int j = 0; j < n; j++) {
      for(int i = 1; i < n; i++) {
        solver.addConstraint(
            solver.makeGreaterOrEqual(x[i][j], x[i - 1][j]));
      }
    }

    // calculate the structure (i.e. the partition)
    for(int i = 0; i < n; i++) {
      IntVar[] b = new IntVar[n];
      for(int j = 0; j < n; j++) {
        b[j] = solver.makeIsLessOrEqualCstVar(x[i][j], n);
      }
      solver.addConstraint(
          solver.makeEquality(p[i], solver.makeSum(b).var()));
    }

    solver.addConstraint(
        solver.makeEquality(solver.makeSum(p).var(), n));

    for(int i = 1; i < n; i++) {
      solver.addConstraint(solver.makeGreaterOrEqual(p[i - 1], p[i]));
    }


    //
    // search
    //
    DecisionBuilder db = solver.makePhase(x_flat,
                                          solver.CHOOSE_FIRST_UNBOUND,
                                          solver.ASSIGN_MIN_VALUE);

    solver.newSearch(db);

    //
    // output
    //
    while (solver.nextSolution()) {
      System.out.print("p: ");
      for(int i = 0; i < n; i++) {
        System.out.print(p[i].value() + " ");
      }

      System.out.println("\nx:");
      for(int i = 0; i < n; i++) {
        for(int j = 0; j < n; j++) {
          long val = x[i][j].value();
          if (val <= n) {
            System.out.print(val + " ");
          }
        }
        if (p[i].value() > 0) {
          System.out.println();
        }
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

    int n = 5;
    if (args.length > 0) {
      n = Integer.parseInt(args[0]);
    }

    YoungTableaux.solve(n);
  }
}
