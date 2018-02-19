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

public class SetCovering {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves a set covering problem.
   * See http://www.hakank.org/google_or_tools/set_covering.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("SetCovering");

    //
    // data
    //
    // Placing of firestations, from Winston 'Operations Research',
    // page 486.
    int min_distance = 15;
    int num_cities = 6;

    int[][] distance = {{ 0,10,20,30,30,20},
                        {10, 0,25,35,20,10},
                        {20,25, 0,15,30,20},
                        {30,35,15, 0,15,25},
                        {30,20,30,15, 0,14},
                        {20,10,20,25,14, 0}};


    //
    // variables
    //
    IntVar[] x = solver.makeIntVarArray(num_cities, 0, 1, "x");
    IntVar z = solver.makeSum(x).var();


    //
    // constraints
    //

    // ensure that all cities are covered
    for(int i = 0; i < num_cities; i++) {
      ArrayList<IntVar> b = new ArrayList<IntVar>();
      for(int j = 0; j < num_cities; j++) {
        if (distance[i][j] <= min_distance) {
          b.add(x[j]);
        }
      }
      solver.addConstraint(
          solver.makeSumGreaterOrEqual(b.toArray(new IntVar[1]), 1));

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
      for(int i = 0; i < num_cities; i++) {
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
    SetCovering.solve();
  }
}
