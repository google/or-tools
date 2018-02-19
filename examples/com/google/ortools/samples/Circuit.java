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

public class Circuit {

  static {
    System.loadLibrary("jniortools");
  }

  /**
   * circuit(solver, x)
   *
   * A decomposition of the global constraint circuit, based
   * on some observation of the orbits in an array.
   *
   * Note: The domain of x must be 0..n-1 (not 1..n)
   * since Java is 0-based.
   */
  public static void circuit(Solver solver, IntVar[] x) {

    int n = x.length;
    IntVar[] z = solver.makeIntVarArray(n, 0, n - 1, "z");

    solver.addConstraint(solver.makeAllDifferent(x));
    solver.addConstraint(solver.makeAllDifferent(z));

    // put the orbit of x[0] in z[0..n-1]
    solver.addConstraint(solver.makeEquality(z[0], x[0]));
    for(int i = 1; i < n-1; i++) {
      solver.addConstraint(
          solver.makeEquality(z[i],
              solver.makeElement(x, z[i-1]).var()));
    }

    // z may not be 0 for i < n-1
    for(int i = 1; i < n - 1; i++) {
      solver.addConstraint(solver.makeNonEquality(z[i], 0));
    }

    // when i = n-1 it must be 0
    solver.addConstraint(solver.makeEquality(z[n - 1], 0));

  }


  /**
   *
   * Implements a (decomposition) of the global constraint circuit.
   * See http://www.hakank.org/google_or_tools/circuit.py
   *
   */
  private static void solve(int n) {

    Solver solver = new Solver("Circuit");

    //
    // variables
    //
    IntVar[] x = solver.makeIntVarArray(n, 0, n - 1, "x");


    //
    // constraints
    //
    circuit(solver, x);

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

    int n = 5;
    if (args.length > 0) {
      n = Integer.parseInt(args[0]);
    }
    Circuit.solve(n);
  }
}
