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

public class SetCovering3 {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves a set covering problem.
   * See http://www.hakank.org/google_or_tools/set_covering3.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("SetCovering3");

    //
    // data
    //

    // Set covering problem from
    // Katta G. Murty: 'Optimization Models for Decision Making',
    // page 302f
    // http://ioe.engin.umich.edu/people/fac/books/murty/opti_model/junior-7.pdf
    int num_groups = 6;
    int num_senators = 10;

    // which group does a senator belong to?
    int[][] belongs = {{1, 1, 1, 1, 1, 0, 0, 0, 0, 0},   // 1 southern
                       {0, 0, 0, 0, 0, 1, 1, 1, 1, 1},   // 2 northern
                       {0, 1, 1, 0, 0, 0, 0, 1, 1, 1},   // 3 liberals
                       {1, 0, 0, 0, 1, 1, 1, 0, 0, 0},   // 4 conservative
                       {0, 0, 1, 1, 1, 1, 1, 0, 1, 0},   // 5 democrats
                       {1, 1, 0, 0, 0, 0, 0, 1, 0, 1}};  // 6 republicans

    //
    // variables
    //
    IntVar[] x = solver.makeIntVarArray(num_senators, 0, 1, "x");

    // number of assigned senators, to be minimize
    IntVar z = solver.makeSum(x).var();

    //
    // constraints
    //


    // ensure that each group is covered by at least
    // one senator
    for(int i = 0; i < num_groups; i++) {
      IntVar[] b = new IntVar[num_senators];
      for(int j = 0; j < num_senators; j++) {
        b[j] = solver.makeProd(x[j], belongs[i][j]).var();
      }
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
      for(int j = 0; j < num_senators; j++) {
        System.out.print(x[j].value() + " ");
      }
      System.out.println();

      // More details
      for(int j = 0; j < num_senators; j++) {
        if (x[j].value() == 1) {
          System.out.print("Senator " + (1 + j) +
                           " belongs to these groups: ");
          for(int i = 0; i < num_groups; i++) {
            if (belongs[i][j] == 1) {
              System.out.print((1 + i) + " ");
            }
          }
          System.out.println();
        }
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
    SetCovering3.solve();
  }
}
