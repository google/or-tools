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
import com.google.ortools.constraintsolver.*;

public class Xkcd {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves the xkcd problem.
   * See http://www.hakank.org/google_or_tools/xkcd.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("Xkcd");

    int n = 6;
    // for price and total: multiplied by 100 to be able to use integers
    int[]price = {215, 275, 335, 355, 420, 580};
    int total = 1505;

    //
    // Variables
    //
    IntVar[] x = solver.makeIntVarArray(n, 0, 10, "x");

    //
    // Constraints
    //
    solver.addConstraint(
        solver.makeEquality(solver.makeScalProd(x, price).var(), total));

    //
    // Search
    //
    DecisionBuilder db = solver.makePhase(x,
                                          solver.CHOOSE_FIRST_UNBOUND,
                                          solver.ASSIGN_MIN_VALUE);
    solver.newSearch(db);

    while (solver.nextSolution()) {
      System.out.print("x: ");
      for(int i = 0; i < n; i++) {
        System.out.print(x[i].value() + " ");
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
    Xkcd.solve();
  }
}
