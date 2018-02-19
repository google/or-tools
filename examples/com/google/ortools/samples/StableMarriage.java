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

public class StableMarriage {

  static {
    System.loadLibrary("jniortools");
  }

  /**
   *
   * Solves some stable marriage problems.
   * See http://www.hakank.org/google_or_tools/stable_marriage.py
   *
   */
  private static void solve(long[][][] ranks, String problem_name) {

    Solver solver = new Solver("StableMarriage");

    //
    // data
    //
    System.out.println("\n#####################");
    System.out.println("Problem: " + problem_name);


    long[][] rankWomen = ranks[0];
    long[][] rankMen   = ranks[1];

    int n = rankWomen.length;

    //
    // variables
    //
    IntVar[] wife = solver.makeIntVarArray(n, 0, n - 1, "wife");
    IntVar[] husband = solver.makeIntVarArray(n, 0, n - 1, "husband");

    //
    // constraints
    // (the comments are the Comet code)
    //   forall(m in Men)
    //      cp.post(husband[wife[m]] == m);
    for(int m = 0; m < n; m++) {
      solver.addConstraint(
          solver.makeEquality(solver.makeElement(husband, wife[m]), m));
    }

    //   forall(w in Women)
    //     cp.post(wife[husband[w]] == w);
    for(int w = 0; w < n; w++) {
      solver.addConstraint(
          solver.makeEquality(solver.makeElement(wife, husband[w]), w));
    }


    //   forall(m in Men, o in Women)
    //       cp.post(rankMen[m,o] < rankMen[m, wife[m]] =>
    //               rankWomen[o,husband[o]] < rankWomen[o,m]);
    for(int m = 0; m < n; m++) {
      for(int o = 0; o < n; o++) {
        IntVar b1 = solver.makeIsGreaterCstVar(
                        solver.makeElement(rankMen[m], wife[m]).var(),
                        rankMen[m][o]);

        IntVar b2 = solver.makeIsLessCstVar(
                        solver.makeElement(rankWomen[o], husband[o]).var(),
                        rankWomen[o][m]);
        solver.addConstraint(
            solver.makeLessOrEqual(
                solver.makeDifference(b1, b2), 0));
      }
    }

    //   forall(w in Women, o in Men)
    //      cp.post(rankWomen[w,o] < rankWomen[w,husband[w]] =>
    //              rankMen[o,wife[o]] < rankMen[o,w]);
    for(int w = 0; w < n; w++) {
      for(int o = 0; o < n; o++) {
        IntVar b1 = solver.makeIsGreaterCstVar(
                        solver.makeElement(rankWomen[w], husband[w]).var(),
                        rankWomen[w][o]);
        IntVar b2 = solver.makeIsLessCstVar(
                        solver.makeElement(rankMen[o], wife[o]).var(),
                        rankMen[o][w]);
        solver.addConstraint(
            solver.makeLessOrEqual(
                solver.makeDifference(b1, b2), 0));
        }
      }


    //
    // search
    //
    DecisionBuilder db = solver.makePhase(wife,
                                          solver.INT_VAR_DEFAULT,
                                          solver.INT_VALUE_DEFAULT);

    solver.newSearch(db);

    //
    // output
    //
    while (solver.nextSolution()) {
      System.out.print("wife   : ");
      for(int i = 0; i < n; i++) {
        System.out.print(wife[i].value() + " ");
      }
      System.out.print("\nhusband: ");
      for(int i = 0; i < n; i++) {
        System.out.print(husband[i].value() + " ");
      }
      System.out.println("\n");

    }

    solver.endSearch();

    // Statistics
    // System.out.println();
    System.out.println("Solutions: " + solver.solutions());
    System.out.println("Failures: " + solver.failures());
    System.out.println("Branches: " + solver.branches());
    System.out.println("Wall time: " + solver.wallTime() + "ms");

  }

  public static void main(String[] args) throws Exception {

    //
    // From Pascal Van Hentenryck's OPL book
    //
    long[][][] van_hentenryck = {
      // rankWomen
     {{1, 2, 4, 3, 5},
      {3, 5, 1, 2, 4},
      {5, 4, 2, 1, 3},
      {1, 3, 5, 4, 2},
      {4, 2, 3, 5, 1}},

     // rankMen
     {{5, 1, 2, 4, 3},
      {4, 1, 3, 2, 5},
      {5, 3, 2, 4, 1},
      {1, 5, 4, 3, 2},
      {4, 3, 2, 1, 5}}
    };


    //
    // Data from MathWorld
    // http://mathworld.wolfram.com/StableMarriageProblem.html
    //
    long[][][]  mathworld = {
      // rankWomen
      {{3, 1, 5, 2, 8, 7, 6, 9, 4},
       {9, 4, 8, 1, 7, 6, 3, 2, 5},
       {3, 1, 8, 9, 5, 4, 2, 6, 7},
       {8, 7, 5, 3, 2, 6, 4, 9, 1},
       {6, 9, 2, 5, 1, 4, 7, 3, 8},
       {2, 4, 5, 1, 6, 8, 3, 9, 7},
       {9, 3, 8, 2, 7, 5, 4, 6, 1},
       {6, 3, 2, 1, 8, 4, 5, 9, 7},
       {8, 2, 6, 4, 9, 1, 3, 7, 5}},

      // rankMen
      {{7, 3, 8, 9, 6, 4, 2, 1, 5},
       {5, 4, 8, 3, 1, 2, 6, 7, 9},
       {4, 8, 3, 9, 7, 5, 6, 1, 2},
       {9, 7, 4, 2, 5, 8, 3, 1, 6},
       {2, 6, 4, 9, 8, 7, 5, 1, 3},
       {2, 7, 8, 6, 5, 3, 4, 1, 9},
       {1, 6, 2, 3, 8, 5, 4, 9, 7},
       {5, 6, 9, 1, 2, 8, 4, 3, 7},
       {6, 1, 4, 7, 5, 8, 3, 9, 2}}};

    //
    // Data from
    // http://www.csee.wvu.edu/~ksmani/courses/fa01/random/lecnotes/lecture5.pdf
    //
    long[][][] problem3 = {
      // rankWomen
      {{1,2,3,4},
       {4,3,2,1},
       {1,2,3,4},
       {3,4,1,2}},

      // rankMen"
      {{1,2,3,4},
       {2,1,3,4},
       {1,4,3,2},
       {4,3,1,2}}};


    //
    // Data from
    // http://www.comp.rgu.ac.uk/staff/ha/ZCSP/additional_problems/stable_marriage/stable_marriage.pdf
    // page 4
    //
    long[][][] problem4 = {
      // rankWomen
      {{1,5,4,6,2,3},
       {4,1,5,2,6,3},
       {6,4,2,1,5,3},
       {1,5,2,4,3,6},
       {4,2,1,5,6,3},
       {2,6,3,5,1,4}},

      // rankMen
      {{1,4,2,5,6,3},
       {3,4,6,1,5,2},
       {1,6,4,2,3,5},
       {6,5,3,4,2,1},
       {3,1,2,4,5,6},
       {2,3,1,6,5,4}}};


    StableMarriage.solve(van_hentenryck, "Van Hentenryck");
    StableMarriage.solve(mathworld, "MathWorld");
    StableMarriage.solve(problem3, "Problem 3");
    StableMarriage.solve(problem4, "Problem 4");


  }
}
