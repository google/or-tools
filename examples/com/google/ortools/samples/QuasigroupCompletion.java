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

public class QuasigroupCompletion {

  static {
    System.loadLibrary("jniortools");
  }

  static int X = 0;

  /*
   * default problem
   *
   * Example from Ruben Martins and Inès Lynce
   * Breaking Local Symmetries in Quasigroup Completion Problems, page 3
   * The solution is unique:
   *
   *   1 3 2 5 4
   *   2 5 4 1 3
   *   4 1 3 2 5
   *   5 4 1 3 2
   *   3 2 5 4 1
   */
  static int default_n = 5;
  static int[][] default_problem = {{1, X, X, X, 4},
                                    {X, 5, X, X, X},
                                    {4, X, X, 2, X},
                                    {X, 4, X, X, X},
                                    {X, X, 5, X, 1}};


  // for the actual problem
  static int n;
  static int[][] problem;


  /**
   *
   * Solves the Quasigroup Completion problem.
   * See http://www.hakank.org/google_or_tools/quasigroup_completion.py
   */
  private static void solve() {

    Solver solver = new Solver("QuasigroupCompletion");

    //
    // data
    //
    System.out.println("Problem:");
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        System.out.print(problem[i][j] + " ");
      }
      System.out.println();
    }
    System.out.println();


    //
    // Variables
    //
    IntVar[][] x =  new IntVar[n][n];
    IntVar[] x_flat = new IntVar[n * n]; // for branching
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        x[i][j] = solver.makeIntVar(1, n, "x[" + i + "," + j + "]");
        x_flat[i * n + j] = x[i][j];
      }
    }

    //
    // Constraints
    //
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        if (problem[i][j] > X) {
          solver.addConstraint(solver.makeEquality(x[i][j], problem[i][j]));
        }
      }
    }

    //
    // rows and columns must be different
    //

    // rows
    for(int i = 0; i < n; i++) {
      IntVar[] row = new IntVar[n];
      for(int j = 0; j < n; j++) {
        row[j] = x[i][j];
      }
      solver.addConstraint(
          solver.makeAllDifferent(row));

    }

    // columns
    for(int j = 0; j < n; j++) {
      IntVar[] col = new IntVar[n];
      for(int i = 0; i < n; i++) {
        col[i] = x[i][j];
      }
      solver.addConstraint(solver.makeAllDifferent(col));
    }


    //
    // Search
    //
    DecisionBuilder db = solver.makePhase(x_flat,
                                          solver.INT_VAR_SIMPLE,
                                          solver.ASSIGN_MIN_VALUE);
    solver.newSearch(db);

    int sol = 0;
    while (solver.nextSolution()) {
      sol++;
      System.out.println("Solution #" + sol + ":");
      for(int i = 0; i < n; i++) {
        for(int j = 0; j < n; j++) {
          System.out.print(x[i][j].value() + " ");
        }
        System.out.println();
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

  /**
   *
   * Reads a Quasigroup completion file.
   * File format:
   *  # a comment which is ignored
   *  % a comment which also is ignored
   *  number of rows (n)
   *  <
   *    row number of space separated entries
   *  >
   *
   * "." or "0" means unknown, integer 1..n means known value
   *
   * Example
   *   5
   *    1 . . . 4
   *   . 5 . . .
   *   4 . . 2 .
   *   . 4 . . .
   *   . . 5 . 1
   *
   */
  private static void readFile(String file) {

    System.out.println("readFile(" + file + ")");
    int lineCount = 0;

    try {

      BufferedReader inr = new BufferedReader(new FileReader(file));
      String str;
      while ((str = inr.readLine()) != null && str.length() > 0) {

        str = str.trim();

        // ignore comments
        if(str.startsWith("#") || str.startsWith("%")) {
          continue;
        }

        System.out.println(str);
        if (lineCount == 0) {
          n = Integer.parseInt(str); // number of rows
          problem = new int[n][n];
        } else {
          // the problem matrix
          String row[] = str.split(" ");
          for(int i = 0; i < n; i++) {
            String s = row[i];
            if (s.equals(".")) {
              problem[lineCount - 1][i] = 0;
            } else {
              problem[lineCount - 1][i] = Integer.parseInt(s);
            }
          }
        }

        lineCount++;

      } // end while

      inr.close();

    } catch (IOException e) {
      System.out.println(e);
    }

  } // end readFile


  public static void main(String[] args) throws Exception {

    if (args.length > 0) {
      String file = "";
      file = args[0];
      QuasigroupCompletion.readFile(file);
    } else {
      problem = default_problem;
      n = default_n;
    }

    QuasigroupCompletion.solve();
  }
}
