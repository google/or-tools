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

public class Diet {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves the Diet problem.
   * See http://www.hakank.org/google_or_tools/diet1.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("Diet");

    int n = 4;
    int[] price  = { 50, 20, 30, 80}; // in cents

    // requirements for each nutrition type
    int[] limits = {500,  6, 10,  8};

    // nutritions for each product
    int[] calories  = {400, 200, 150, 500};
    int[] chocolate = {3, 2, 0, 0};
    int[] sugar     = {2, 2, 4, 4};
    int[] fat       = {2, 4, 1, 5};


    //
    // Variables
    //
    IntVar[] x = solver.makeIntVarArray(n, 0, 100, "x");

    IntVar cost = solver.makeScalProd(x, price).var();

    //
    // Constraints
    //
    solver.addConstraint(
        solver.makeScalProdGreaterOrEqual(x, calories, limits[0]));

    solver.addConstraint(
        solver.makeScalProdGreaterOrEqual(x,chocolate, limits[1]));

    solver.addConstraint(
        solver.makeScalProdGreaterOrEqual(x, sugar, limits[2]));

    solver.addConstraint(
        solver.makeScalProdGreaterOrEqual(x, fat, limits[3]));


    //
    // Objective
    //
    OptimizeVar obj = solver.makeMinimize(cost, 1);

    //
    // Search
    //
    DecisionBuilder db = solver.makePhase(x,
                                          solver.CHOOSE_PATH,
                                          solver.ASSIGN_MIN_VALUE);
    solver.newSearch(db, obj);
    while (solver.nextSolution()) {
      System.out.println("cost: " + cost.value());
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
    Diet.solve();
  }
}
