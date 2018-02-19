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

public class AllDifferentExcept0 {

  static {
    System.loadLibrary("jniortools");
  }

  //
  // alldifferent_except_0(solver, x)
  //
  // A decomposition of the global constraint
  // alldifferent_except_0, i.e. all values
  // must be either distinct, or 0.
  //
  public static void alldifferent_except_0(Solver solver, IntVar[] a) {

    int n = a.length;
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < i; j++) {
        IntVar bi = solver.makeIsDifferentCstVar(a[i], 0);
        IntVar bj = solver.makeIsDifferentCstVar(a[j], 0);
        IntVar bij = solver.makeIsDifferentCstVar(a[i], a[j]);
        solver.addConstraint(
            solver.makeLessOrEqual(
                solver.makeProd(bi, bj).var(), bij));
      }
    }
  }


  /**
   *
   * Implements a (decomposition) of global constraint
   * alldifferent_except_0.
   * See http://www.hakank.org/google_or_tools/circuit.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("AllDifferentExcept0");

    //
    // data
    //
    int n = 5;

    //
    // variables
    //
    IntVar[] x = solver.makeIntVarArray(n, 0, n - 1, "x");

    //
    // constraints
    //
    alldifferent_except_0(solver, x);

    // we also require at least 2 0's
    IntVar[] z_tmp = solver.makeBoolVarArray(n, "z_tmp");
    for(int i = 0; i < n; i++) {
      solver.addConstraint(
          solver.makeIsEqualCstCt(x[i], 0, z_tmp[i]));
    }

    IntVar z = solver.makeSum(z_tmp).var();
    solver.addConstraint(solver.makeEquality(z, 2));

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
      System.out.println("  z: " + z.value());
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
    AllDifferentExcept0.solve();
  }
}
