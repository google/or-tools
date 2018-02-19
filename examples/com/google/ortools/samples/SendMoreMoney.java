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

public class SendMoreMoney {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves the SEND+MORE=MONEY problem.
   *
   */
  private static void solve() {
    int base = 10;

    Solver solver = new Solver("SendMoreMoney");
    IntVar s = solver.makeIntVar(0, base - 1, "s");
    IntVar e = solver.makeIntVar(0, base - 1, "e");
    IntVar n = solver.makeIntVar(0, base - 1, "n");
    IntVar d = solver.makeIntVar(0, base - 1, "d");
    IntVar m = solver.makeIntVar(0, base - 1, "m");
    IntVar o = solver.makeIntVar(0, base - 1, "o");
    IntVar r = solver.makeIntVar(0, base - 1, "r");
    IntVar y = solver.makeIntVar(0, base - 1, "y");

    IntVar[] x = {s,e,n,d,m,o,r,y};

    IntVar[] eq = {s,e,n,d,  m,o,r,e, m,o,n,e,y};
    int[] coeffs = {1000, 100, 10, 1,            //    S E N D +
                    1000, 100, 10, 1,            //    M O R E
                    -10000, -1000, -100, -10, -1 // == M O N E Y
    };
    solver.addConstraint(solver.makeScalProdEquality(eq, coeffs, 0));

    // alternative:
    solver.addConstraint(
        solver.makeScalProdEquality(new IntVar[] {s,e,n,d,  m,o,r,e, m,o,n,e,y},
                                    coeffs, 0));

    // s > 0
    solver.addConstraint(solver.makeGreater(s, 0));
    // m > 0
    solver.addConstraint(solver.makeGreater(m, 0));

    solver.addConstraint(solver.makeAllDifferent(x));

    DecisionBuilder db = solver.makePhase(x,
                                          solver.INT_VAR_DEFAULT,
                                          solver.INT_VALUE_DEFAULT);
    solver.newSearch(db);
    while (solver.nextSolution()) {
      for(int i = 0; i < 8; i++) {
        System.out.print(x[i].toString() + " ");
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
    SendMoreMoney.solve();
  }
}
