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

public class AllInterval {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Implements the all interval problem.
   * See http://www.hakank.org/google_or_tools/all_interval.py
   *
   */
  private static void solve(int n) {

    Solver solver = new Solver("AllInterval");


    //
    // variables
    //
    IntVar[] x = solver.makeIntVarArray(n, 0, n - 1, "x");
    IntVar[] diffs = solver.makeIntVarArray(n - 1, 1, n - 1, "diffs");

    //
    // constraints
    //
    solver.addConstraint(solver.makeAllDifferent(x));
    solver.addConstraint(solver.makeAllDifferent(diffs));

    for(int k = 0; k < n - 1; k++) {
      solver.addConstraint(
          solver.makeEquality(diffs[k],
              solver.makeAbs(solver.makeDifference(x[k + 1], x[k])).var()));
    }


    // symmetry breaking
    solver.addConstraint(solver.makeLess(x[0], x[n - 1]));
    solver.addConstraint(solver.makeLess(diffs[0], diffs[1]));


    //
    // search
    //
    DecisionBuilder db = solver.makePhase(x,
                                          solver.CHOOSE_FIRST_UNBOUND,
                                          solver.ASSIGN_MIN_VALUE);

    solver.newSearch(db);

    //
    // output
    //
    while (solver.nextSolution()) {
      System.out.print("x    : ");
      for(int i = 0; i < n; i++) {
        System.out.print(x[i].value() + " ");
      }
      System.out.print("\ndiffs: ");

      for(int i = 0; i < n-1; i++) {
        System.out.print(diffs[i].value() + " ");
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

    int n = 12;
    if (args.length > 0) {
      n = Integer.parseInt(args[0]);
    }

    AllInterval.solve(n);
  }
}
