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

public class SetCovering4 {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves a set covering problem.
   * See http://www.hakank.org/google_or_tools/set_covering4.py
   *
   */
  private static void solve(int set_partition) {

    Solver solver = new Solver("SetCovering4");

    //
    // data
    //


    // Set partition and set covering problem from
    // Example from the Swedish book
    // Lundgren, Roennqvist, Vaebrand
    // 'Optimeringslaera' (translation: 'Optimization theory'),
    // page 408.
    int num_alternatives = 10;
    int num_objects = 8;

    // costs for the alternatives
    int[] costs = {19, 16, 18, 13, 15, 19, 15, 17, 16, 15};

    // the alternatives, and their objects
    int[][] a = {
      // 1 2 3 4 5 6 7 8    the objects
        {1,0,0,0,0,1,0,0},  // alternative 1
        {0,1,0,0,0,1,0,1},  // alternative 2
        {1,0,0,1,0,0,1,0},  // alternative 3
        {0,1,1,0,1,0,0,0},  // alternative 4
        {0,1,0,0,1,0,0,0},  // alternative 5
        {0,1,1,0,0,0,0,0},  // alternative 6
        {0,1,1,1,0,0,0,0},  // alternative 7
        {0,0,0,1,1,0,0,1},  // alternative 8
        {0,0,1,0,0,1,0,1},  // alternative 9
        {1,0,0,0,0,1,1,0}}; // alternative 10

    //
    // variables
    //
    IntVar[] x = solver.makeIntVarArray(num_alternatives, 0, 1, "x");

    // number of assigned senators, to be minimize
    IntVar z = solver.makeScalProd(x, costs).var();

    //
    // constraints
    //


    for(int j = 0; j < num_objects; j++) {
      IntVar[] b = new IntVar[num_alternatives];
      for(int i = 0; i < num_alternatives; i++) {
        b[i] = solver.makeProd(x[i], a[i][j]).var();
      }

      if (set_partition == 1) {
        solver.addConstraint(
            solver.makeSumGreaterOrEqual(b, 1));
      } else {
        solver.addConstraint(
            solver.makeSumEquality(b, 1));
      }
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
      System.out.print("Selected alternatives: ");
      for(int i = 0; i < num_alternatives; i++) {
        if (x[i].value() == 1) {
          System.out.print((1 + i) + " ");
        }
      }
      System.out.println("\n");
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
    System.out.println("Set partition:");
    SetCovering4.solve(1);
    System.out.println("\nSet covering:");
    SetCovering4.solve(0);
  }
}
