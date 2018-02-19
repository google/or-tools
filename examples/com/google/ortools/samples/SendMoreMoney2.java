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

public class SendMoreMoney2 {

  static Solver sol;

  static {
    System.loadLibrary("jniortools");
  }

  //
  // Some helper methods
  //
  static IntExpr p(IntExpr a, int b, IntExpr c) {
    return sol.makeSum(sol.makeProd(a, b), c);
  }

  static IntExpr p(IntVar a, int b) {
    return sol.makeProd(a, b);
  }

  // a slighly more intelligent scalar product
  static IntExpr sp(IntVar[] a) {
    int len = a.length;
    int c = 1;
    int[] t = new int[len];
    for(int i = len-1; i >= 0; i--) {
      t[i] = c;
      c *= 10;
    }
    return sol.makeScalProd(a, t);
  }


  /**
   *
   * Solves the SEND+MORE=MONEY problem with different approaches.
   *
   */
  private static void solve(int alt) {

    sol = new Solver("SendMoreMoney");

    int base = 10;

    //
    // variables
    //
    IntVar s = sol.makeIntVar(0, base - 1, "s");
    IntVar e = sol.makeIntVar(0, base - 1, "e");
    IntVar n = sol.makeIntVar(0, base - 1, "n");
    IntVar d = sol.makeIntVar(0, base - 1, "d");
    IntVar m = sol.makeIntVar(0, base - 1, "m");
    IntVar o = sol.makeIntVar(0, base - 1, "o");
    IntVar r = sol.makeIntVar(0, base - 1, "r");
    IntVar y = sol.makeIntVar(0, base - 1, "y");

    IntVar[] x = {s, e, n, d, m, o, r, y};

    //
    // Constraints
    //

    /*
     *
     * Below are some alternatives encodings of the
     * same idea:
     *
     *             1000*s + 100*e + 10*n + d +
     *             1000*m + 100*o + 10*r + e ==
     *   10000*m + 1000*o + 100*n + 10*e + y
     *
     */


    if (alt == 0) {
      //
      // First, a version approach which is just too noisy.
      //
      sol.addConstraint(
          sol.makeEquality(
              sol.makeSum(sol.makeSum(
                  sol.makeProd(s, 1000),
                  sol.makeSum(sol.makeProd(e, 100),
                              sol.makeSum(sol.makeProd(n, 10),
                                          sol.makeProd(d, 1)))),
                          sol.makeSum(
                              sol.makeProd(m, 1000),
                              sol.makeSum(sol.makeProd(o, 100),
                                          sol.makeSum(sol.makeProd(r, 10),
                                                      sol.makeProd(e, 1))))
                          ).var(),
              sol.makeSum(sol.makeProd(m, 10000),
                          sol.makeSum(
                              sol.makeProd(o, 1000),
                              sol.makeSum(
                                  sol.makeProd(n, 100),
                                  sol.makeSum(
                                      sol.makeProd(e, 10),
                                      sol.makeProd(y, 1))))).var()));

    } else if (alt == 1) {

      //
      // Alternative 1, using the helper methods
      //
      //     p(IntExpr, int, IntExpr) and
      //     p(IntVar, int)
      //
      sol.addConstraint(
          sol.makeEquality(
              sol.makeSum(p(s, 1000, p(e, 100, p(n, 10, p(d, 1)))),
                          p(m, 1000, p(o, 100, p(r, 10, p(e, 1))))).var(),
              p(m, 10000, p(o, 1000, p(n, 100, p(e, 10, p(y, 1))))).var()));

    } else if (alt == 2) {

      //
      // Alternative 2
      //
      sol.addConstraint(
          sol.makeEquality(
              sol.makeSum(
                  sol.makeScalProd(new IntVar[] {s, e, n, d},
                                   new int[] {1000, 100, 10, 1}),
                  sol.makeScalProd(new IntVar[] {m, o, r, e},
                                   new int[] {1000, 100, 10, 1})).var(),
              sol.makeScalProd(new IntVar[] {m, o, n, e, y},
                               new int[] {10000, 1000, 100, 10, 1}).var()));

    } else if (alt == 3) {


      //
      // alternative 3: same approach as 2, with some helper methods
      //
      sol.addConstraint(
          sol.makeEquality(sol.makeSum(sp(new IntVar[] {s, e, n, d}),
                                       sp(new IntVar[] {m, o, r, e})).var(),
                           sp(new IntVar[] {m, o, n, e, y}).var()));

    } else if (alt == 4) {

      //
      // Alternative 4, using explicit variables
      //
      IntExpr send = sol.makeScalProd(new IntVar[] {s, e, n, d},
                                      new int[] {1000, 100, 10, 1});
      IntExpr more = sol.makeScalProd(new IntVar[] {m, o, r, e},
                                      new int[] {1000, 100, 10, 1});
      IntExpr money = sol.makeScalProd(new IntVar[] {m, o, n, e, y},
                                       new int[] {10000, 1000, 100, 10, 1});
      sol.addConstraint(
          sol.makeEquality(sol.makeSum(send, more).var(), money.var()));

    }


    // s > 0
    sol.addConstraint(sol.makeGreater(s, 0));
    // m > 0
    sol.addConstraint(sol.makeGreater(m, 0));

    sol.addConstraint(sol.makeAllDifferent(x));

    //
    // Search
    //
    DecisionBuilder db = sol.makePhase(x,
                                       sol.INT_VAR_DEFAULT,
                                       sol.INT_VALUE_DEFAULT);
    sol.newSearch(db);
    while (sol.nextSolution()) {
      for(int i = 0; i < 8; i++) {
        System.out.print(x[i].toString() + " ");
      }
      System.out.println();
    }
    sol.endSearch();

    //
    // Statistics
    //
    System.out.println();
    System.out.println("Solutions: " + sol.solutions());
    System.out.println("Failures: " + sol.failures());
    System.out.println("Branches: " + sol.branches());
    System.out.println("Wall time: " + sol.wallTime() + "ms");

  }

  public static void main(String[] args) throws Exception {
    for (int i = 0; i < 5; i++) {
      System.out.println("\nalternative #" + i);
      SendMoreMoney2.solve(i);
    }
  }
}
