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

public class SendMostMoney {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves the SEND+MOST=MONEY problem, where
   * we maximize MONEY.
   * See http://www.hakank.org/google_or_tools/send_more_money.py
   *
   */
  private static long solve(long MONEY) {

    Solver solver = new Solver("SendMostMoney");


    //
    // data
    //
    final int base = 10;

    //
    // variables
    //
    IntVar s = solver.makeIntVar(0, base - 1, "s");
    IntVar e = solver.makeIntVar(0, base - 1, "e");
    IntVar n = solver.makeIntVar(0, base - 1, "n");
    IntVar d = solver.makeIntVar(0, base - 1, "d");
    IntVar m = solver.makeIntVar(0, base - 1, "m");
    IntVar o = solver.makeIntVar(0, base - 1, "o");
    IntVar t = solver.makeIntVar(0, base - 1, "t");
    IntVar y = solver.makeIntVar(0, base - 1, "y");

    IntVar[] x = {s, e, n, d, m, o, t, y};

    IntVar[] eq = {s,e,n,d,  m,o,s,t, m,o,n,e,y};
    int[] coeffs = {1000, 100, 10, 1,         //    S E N D +
                    1000, 100, 10, 1,         //    M O S T
                    -10000,-1000, -100,-10,-1 // == M O N E Y
    };
    solver.addConstraint(solver.makeScalProdEquality(eq, coeffs, 0));

    IntVar money = solver.makeScalProd(new IntVar[] {m, o, n, e, y},
                                       new int[] {10000, 1000, 100, 10, 1}).var();

    //
    // constraints
    //

    // s > 0
    solver.addConstraint(solver.makeGreater(s, 0));
    // m > 0
    solver.addConstraint(solver.makeGreater(m, 0));

    solver.addConstraint(solver.makeAllDifferent(x));

    if (MONEY > 0) {
      // Search for all solutions.
      solver.addConstraint(solver.makeEquality(money, MONEY));
    }

    //
    // search
    //
    DecisionBuilder db = solver.makePhase(x,
                                          solver.CHOOSE_FIRST_UNBOUND,
                                          solver.ASSIGN_MAX_VALUE);

    if (MONEY == 0) {
      // first round: get the optimal value
      OptimizeVar obj = solver.makeMaximize(money, 1);
      solver.newSearch(db, obj);
    } else {
      // search for all solutions
      solver.newSearch(db);
    }

    long money_ret = 0;
    while (solver.nextSolution()) {
      System.out.println("money: " + money.value());
      money_ret = money.value();
      for(int i = 0; i < x.length; i++) {
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

    return money_ret;

  }

  public static void main(String[] args) throws Exception {
    System.out.println("Get the max value of money:");
    long this_money = SendMostMoney.solve(0);
    System.out.println("\nThen find all solutions with this value:");
    long tmp = SendMostMoney.solve(this_money);
  }
}
