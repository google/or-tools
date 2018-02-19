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

public class DivisibleBy9Through1 {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * A simple propagator for modulo constraint.
   *
   * This implementation is based on the ECLiPSe version
   * mentioned in "A Modulo propagator for ECLiPSE"
   * http://www.hakank.org/constraint_programming_blog/2010/05/a_modulo_propagator_for_eclips.html
   * The ECLiPSe Prolog source code:
   * http://www.hakank.org/eclipse/modulo_propagator.ecl
   *
   */
  public static void my_mod(Solver solver, IntVar x, IntVar y, IntVar r) {

    long lbx = x.min();
    long ubx = x.max();
    long ubx_neg = -ubx;
    long lbx_neg = -lbx;
    int min_x = (int)Math.min(lbx, ubx_neg);
    int max_x = (int)Math.max(ubx, lbx_neg);

    IntVar d = solver.makeIntVar(min_x, max_x, "d");

    // r >= 0
    solver.addConstraint(solver.makeGreaterOrEqual(r,0));

    // x*r >= 0
    solver.addConstraint(
        solver.makeGreaterOrEqual(
            solver.makeProd(x,r).var(), 0));

    // -abs(y) < r
    solver.addConstraint(
        solver.makeLess(
            solver.makeOpposite(solver.makeAbs(y).var()).var(), r));

    // r < abs(y)
    solver.addConstraint(
        solver.makeLess(r,
            solver.makeAbs(y).var().var()));

    // min_x <= d, i.e. d > min_x
    solver.addConstraint(solver.makeGreater(d, min_x));


    // d <= max_x
    solver.addConstraint(solver.makeLessOrEqual(d, max_x));

    // x == y*d+r
    solver.addConstraint(solver.makeEquality(x,
        solver.makeSum(
            solver.makeProd(y,d).var(),r).var()));

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
   * Solves the divisible by 9 through 1 problem.
   * See http://www.hakank.org/google_or_tools/divisible_by_9_through_1.py
   *
   */
  private static void solve(int base) {

    Solver solver = new Solver("DivisibleBy9Through1");

    //
    // data
    //
    int m = (int)Math.pow(base,(base-1)) - 1;
    int n = base - 1;

    String[] digits_str = {"_","0","1","2","3","4","5","6","7","8","9"};

    System.out.println("base: " + base);

    //
    // variables
    //

    // digits
    IntVar[] x = solver.makeIntVarArray(n, 1, base - 1, "x");

    // the numbers. t[0] contains the answe
    IntVar[] t = solver.makeIntVarArray(n, 0, m, "t");


    //
    // constraints
    //
    solver.addConstraint(solver.makeAllDifferent(x));

    // Ensure the divisibility of base .. 1
    IntVar zero = solver.makeIntConst(0);
    for(int i = 0; i < n; i++) {
      int mm = base - i - 1;
      IntVar[] tt = new IntVar[mm];
      for(int j = 0; j < mm; j++) {
        tt[j] = x[j];
      }
      toNum(solver, tt, t[i], base);
      IntVar mm_const = solver.makeIntConst(mm);
      my_mod(solver, t[i], mm_const, zero);
    }



    //
    // search
    //
    DecisionBuilder db = solver.makePhase(x,
                                          solver.INT_VAR_DEFAULT,
                                          solver.INT_VALUE_DEFAULT);
    solver.newSearch(db);

    //
    // output
    //
    while (solver.nextSolution()) {
      System.out.print("x: ");
      for(int i = 0; i < n; i++) {
        System.out.print(x[i].value() + " ");
      }
      System.out.println("\nt: ");
      for(int i = 0; i < n; i++) {
        System.out.print(t[i].value() + " ");
      }
      System.out.println();

      if (base != 10) {
        System.out.print("Number base 10: " + t[0].value());
        System.out.print(" Base " + base + ": ");
        for(int i = 0; i < n; i++) {
          System.out.print(digits_str[(int)x[i].value() + 1]);
        }
        System.out.println("\n");
      }

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

    int base = 10;

    if (args.length > 0) {
      int new_base = Integer.parseInt(args[0]);
      if (new_base > 10) {
        // Note: The next valid base after 10 is 14 and
        // the number 559922224824157, which is too large in this model.
        System.out.println("Sorry, max allowed base is 10. Setting base to 10.");
      } else if (new_base < 2) {
        System.out.println("Sorry, min allowed base is 2. Setting base to 2.");
        base = 2;
      } else {
        base = new_base;
      }
    }

    DivisibleBy9Through1.solve(base);
  }
}
