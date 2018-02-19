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
import com.google.ortools.constraintsolver.OptimizeVar;

public class SetCovering2 {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves a set covering problem.
   * See http://www.hakank.org/google_or_tools/set_covering2.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("SetCovering2");

    //
    // data
    //

    // Example 9.1-2 from
    // Taha "Operations Research - An Introduction",
    // page 354ff.
    // Minimize the number of security telephones in street
    // corners on a campus.

    int n = 8;            // maximum number of corners
    int num_streets = 11; // number of connected streets

    // corners of each street
    // Note: 1-based (handled below)
    int[][] corner = {{1,2},
                      {2,3},
                      {4,5},
                      {7,8},
                      {6,7},
                      {2,6},
                      {1,6},
                      {4,7},
                      {2,4},
                      {5,8},
                      {3,5}};

    //
    // variables
    //
    IntVar[] x = solver.makeIntVarArray(n, 0, 1, "x");

    // number of telephones, to be minimize
    IntVar z = solver.makeSum(x).var();

    //
    // constraints
    //

    // ensure that all cities are covered
    for(int i = 0; i < num_streets; i++) {
      IntVar[] b = new IntVar[2];
      b[0] = x[corner[i][0] - 1];
      b[1] = x[corner[i][1] - 1];
      solver.addConstraint(
          solver.makeSumGreaterOrEqual(b, 1));
    }

    //
    // objective
    //
    OptimizeVar objective = solver.makeMinimize(z, 1);


    //
    // search
    //
    DecisionBuilder db = solver.makePhase(x,
                                          solver.INT_VAR_DEFAULT,
                                          solver.INT_VALUE_DEFAULT);
    solver.newSearch(db, objective);

    //
    // output
    //
    while (solver.nextSolution()) {
      System.out.println("z: " + z.value());
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
    SetCovering2.solve();
  }
}
