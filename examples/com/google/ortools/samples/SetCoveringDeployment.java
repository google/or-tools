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

public class SetCoveringDeployment {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves a set covering deployment problem.
   * See http://www.hakank.org/google_or_tools/set_covering_deployment.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("SetCoveringDeployment");

    //
    // data
    //

    // From http://mathworld.wolfram.com/SetCoveringDeployment.html
    String[] countries = {"Alexandria",
                           "Asia Minor",
                           "Britain",
                           "Byzantium",
                           "Gaul",
                           "Iberia",
                           "Rome",
                           "Tunis"};

    int n = countries.length;

    // the incidence matrix (neighbours)
    int[][] mat = {{0, 1, 0, 1, 0, 0, 1, 1},
                   {1, 0, 0, 1, 0, 0, 0, 0},
                   {0, 0, 0, 0, 1, 1, 0, 0},
                   {1, 1, 0, 0, 0, 0, 1, 0},
                   {0, 0, 1, 0, 0, 1, 1, 0},
                   {0, 0, 1, 0, 1, 0, 1, 1},
                   {1, 0, 0, 1, 1, 1, 0, 1},
                   {1, 0, 0, 0, 0, 1, 1, 0}};


    //
    // variables
    //

    // First army
    IntVar[] x = solver.makeIntVarArray(n, 0, 1, "x");

    // Second (reserve) army
    IntVar[] y = solver.makeIntVarArray(n, 0, 1, "y");

    // total number of armies
    IntVar num_armies = solver.makeSum(solver.makeSum(x),
                                       solver.makeSum(y)).var();

    //
    // constraints
    //

    //
    //  Constraint 1: There is always an army in a city
    //                (+ maybe a backup)
    //                Or rather: Is there a backup, there
    //                must be an an army
    //
    for(int i = 0; i < n; i++) {
      solver.addConstraint(solver.makeGreaterOrEqual(x[i], y[i]));
    }

    //
    // Constraint 2: There should always be an backup
    //               army near every city
    //
    for(int i = 0; i < n; i++) {
      ArrayList<IntVar> count_neighbours = new ArrayList<IntVar>();
      for(int j = 0; j < n; j++) {
        if (mat[i][j] == 1) {
          count_neighbours.add(y[j]);
        }
      }
      solver.addConstraint(
          solver.makeGreaterOrEqual(
              solver.makeSum(x[i],
                  solver.makeSum(
                      count_neighbours.toArray(new IntVar[1])).var()), 1));
    }

    //
    // objective
    //
    OptimizeVar objective = solver.makeMinimize(num_armies, 1);


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
      System.out.println("num_armies: " + num_armies.value());
      for(int i = 0; i < n; i++) {
        if (x[i].value() == 1) {
          System.out.print("Army: " + countries[i] + " ");
        }
        if (y[i].value() == 1) {
          System.out.println("Reserve army: " + countries[i]);
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

    SetCoveringDeployment.solve();

  }
}
