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

public class ToNum {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   *  toNum(solver, a, num, base)
   *
   *  channelling between the array a and the number num
   *
   */
  private static void toNum(Solver solver, IntVar[] a, IntVar num, int base) {
    int len = a.length;

    IntVar[] tmp = new IntVar[len];
    for(int i = 0; i < len; i++) {
      tmp[i] = solver.makeProd(a[i], (int)Math.pow(base,(len-i-1))).var();
    }
    solver.addConstraint(
        solver.makeEquality(solver.makeSum(tmp).var(), num));
  }


  /**
   *
   * Implements toNum: channeling between a number and an array.
   * See http://www.hakank.org/google_or_tools/toNum.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("ToNum");

    int n = 5;
    int base = 10;


    //
    // variables
    //
    IntVar[] x =  solver.makeIntVarArray(n, 0, base - 1, "x");
    IntVar num = solver.makeIntVar(0, (int)Math.pow(base, n) - 1 , "num");



    //
    // constraints
    //
    solver.addConstraint(solver.makeAllDifferent(x));


    toNum(solver, x, num, base);

    // extra constraint (just for fun):
    // second digit should be 7
    // solver.addConstraint(solver.makeEquality(x[1], 7));


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
      System.out.print("num: " + num.value() + ": ");
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
    ToNum.solve();
  }
}
