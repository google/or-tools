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

import com.google.ortools.constraintsolver.*;

public class LeastDiff {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves the Least Diff problem.
   * See http://www.hakank.org/google_or_tools/least_diff.py
   *
   */
  private static void solve() {
    final int base = 10;

    Solver solver = new Solver("LeastDiff");

    //
    // Variables
    //
    IntVar a = solver.makeIntVar(0, base - 1, "a");
    IntVar b = solver.makeIntVar(0, base - 1, "b");
    IntVar c = solver.makeIntVar(0, base - 1, "c");
    IntVar d = solver.makeIntVar(0, base - 1, "d");
    IntVar e = solver.makeIntVar(0, base - 1, "e");

    IntVar f = solver.makeIntVar(0, base - 1, "f");
    IntVar g = solver.makeIntVar(0, base - 1, "g");
    IntVar h = solver.makeIntVar(0, base - 1, "h");
    IntVar i = solver.makeIntVar(0, base - 1, "i");
    IntVar j = solver.makeIntVar(0, base - 1, "j");

    IntVar[] all = {a,b,c,d,e,f,g,h,i,j};

    //
    // Constraints
    //
    int[] coeffs = {10000, 1000, 100, 10, 1};
    IntVar x = solver.makeScalProd(new IntVar[]{a,b,c,d,e}, coeffs).var();
    x.setName("x");
    IntVar y = solver.makeScalProd(new IntVar[]{f,g,h,i,j}, coeffs).var();
    y.setName("y");

    // a > 0
    solver.addConstraint(solver.makeGreater(a, 0));
    // f > 0
    solver.addConstraint(solver.makeGreater(f, 0));

    // diff = x - y
    IntVar diff = solver.makeDifference(x, y).var();
    diff.setName("diff");

    solver.addConstraint(solver.makeAllDifferent(all));

    //
    // Objective
    //
    OptimizeVar obj = solver.makeMinimize(diff, 1);

    //
    // Search
    //
    DecisionBuilder db = solver.makePhase(all,
                                          solver.CHOOSE_PATH,
                                          solver.ASSIGN_MIN_VALUE);
    solver.newSearch(db, obj);
    while (solver.nextSolution()) {
      System.out.println("" + x.value() + " - " +
                         y.value() + " = " + diff.value());
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
    LeastDiff.solve();
  }
}
