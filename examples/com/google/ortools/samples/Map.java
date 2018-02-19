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

public class Map {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   * Solves a simple map coloring problem.
   * See http://www.hakank.org/google_or_tools/map.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("Map");

    //
    // data
    //
    int Belgium     = 0;
    int Denmark     = 1;
    int France      = 2;
    int Germany     = 3;
    int Netherlands = 4;
    int Luxembourg  = 5;

    int n = 6;
    int max_num_colors = 4;

    //
    // Variables
    //
    IntVar[] color = solver.makeIntVarArray(n, 1, max_num_colors, "x");

    //
    // Constraints
    //
    solver.addConstraint(solver.makeNonEquality(color[France],
                                                color[Belgium]));
    solver.addConstraint(solver.makeNonEquality(color[France],
                                                color[Luxembourg]));
    solver.addConstraint(solver.makeNonEquality(color[France],
                                                color[Germany]));
    solver.addConstraint(solver.makeNonEquality(color[Luxembourg],
                                                color[Germany]));
    solver.addConstraint(solver.makeNonEquality(color[Luxembourg],
                                                color[Belgium]));
    solver.addConstraint(solver.makeNonEquality(color[Belgium],
                                                color[Netherlands]));
    solver.addConstraint(solver.makeNonEquality(color[Belgium],
                                                color[Germany]));
    solver.addConstraint(solver.makeNonEquality(color[Germany],
                                                color[Netherlands]));
    solver.addConstraint(solver.makeNonEquality(color[Germany],
                                                color[Denmark]));

    // Symmetry breaking
    solver.addConstraint(solver.makeEquality(color[Belgium], 1));

    //
    // Search
    //
    DecisionBuilder db = solver.makePhase(color,
                                          solver.CHOOSE_FIRST_UNBOUND,
                                          solver.ASSIGN_MIN_VALUE);
    solver.newSearch(db);

    while (solver.nextSolution()) {
      System.out.print("Colors: ");
      for(int i = 0; i < n; i++) {
        System.out.print(color[i].value() + " ");
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
    Map.solve();
  }
}
