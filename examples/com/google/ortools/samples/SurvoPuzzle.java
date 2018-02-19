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

public class SurvoPuzzle {

  static {
    System.loadLibrary("jniortools");
  }

  /*
   * default problem
   */
  static int default_r = 3;
  static int default_c = 4;
  static int[] default_rowsums = {30, 18, 30};
  static int[] default_colsums = {27, 16, 10, 25};
  static int[][] default_game = {{0, 6, 0, 0},
                                 {8, 0, 0, 0},
                                 {0, 0, 3, 0}};


  // for the actual problem
  static int r;
  static int c;
  static int[] rowsums;
  static int[] colsums;
  static int[][] game;


  /**
   *
   * Solves the Survo puzzle problem.
   * See http://www.hakank.org/google_or_tools/survo_puzzle.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("Survopuzzle");

    //
    // data
    //
    System.out.println("Problem:");
    for(int i = 0; i < r; i++) {
      for(int j = 0; j < c; j++) {
        System.out.print(game[i][j] + " ");
      }
      System.out.println();
    }
    System.out.println();


    //
    // Variables
    //
    IntVar[][] x =  new IntVar[r][c];
    IntVar[] x_flat = new IntVar[r * c]; // for branching
    for(int i = 0; i < r; i++) {
      for(int j = 0; j < c; j++) {
        x[i][j] = solver.makeIntVar(1, r * c, "x[" + i + "," + j + "]");
        x_flat[i * c + j] = x[i][j];
      }
    }

    //
    // Constraints
    //
    for(int i = 0; i < r; i++) {
      for(int j = 0; j < c; j++) {
        if (game[i][j] > 0) {
          solver.addConstraint(
              solver.makeEquality(x[i][j], game[i][j]));
        }
      }
    }

    solver.addConstraint(solver.makeAllDifferent(x_flat));

    //
    // calculate rowsums and colsums
    //
    for(int i = 0; i < r; i++) {
      IntVar[] row = new IntVar[c];
      for(int j = 0; j < c; j++) {
        row[j] = x[i][j];
      }
      solver.addConstraint(
          solver.makeEquality(solver.makeSum(row).var(), rowsums[i]));
    }

    for(int j = 0; j < c; j++) {
      IntVar[] col = new IntVar[r];
      for(int i = 0; i < r; i++) {
        col[i] = x[i][j];
      }
      solver.addConstraint(
          solver.makeEquality(solver.makeSum(col).var(), colsums[j]));
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
      for(int i = 0; i < r; i++) {
        for(int j = 0; j < c; j++) {
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
   * readFile()
   *
   * Reads a Survo puzzle in the following format
   * r
   * c
   * rowsums
   * olsums
   * data
   * ...
   *
   * Example:
   * 3
   * 4
   * 30,18,30
   * 27,16,10,25
   * 0,6,0,0
   * 8,0,0,0
   * 0,0,3,0
   *
   */
  private static void readFile(String file) {

    System.out.println("readFile(" + file + ")");

    try {

      BufferedReader inr = new BufferedReader(new FileReader(file));

      r = Integer.parseInt(inr.readLine());
      c = Integer.parseInt(inr.readLine());
      rowsums = new int[r];
      colsums = new int[c];
      System.out.println("r: " + r + " c: " + c);

      String[] rowsums_str = inr.readLine().split(",\\s*");
      String[] colsums_str = inr.readLine().split(",\\s*");
      System.out.println("rowsums:");
      for(int i = 0; i < r; i++) {
        System.out.print(rowsums_str[i] + " ");
        rowsums[i] = Integer.parseInt(rowsums_str[i]);
      }
      System.out.println("\ncolsums:");
      for(int j = 0; j < c; j++) {
        System.out.print(colsums_str[j] + " ");
        colsums[j] = Integer.parseInt(colsums_str[j]);
      }
      System.out.println();

      // init the game matrix and read data from file
      game = new int[r][c];
      String str;
      int line_count = 0;
      while ((str = inr.readLine()) != null && str.length() > 0) {
        str = str.trim();

        // ignore comments
        // starting with either # or %
        if(str.startsWith("#") || str.startsWith("%")) {
          continue;
        }

        String this_row[] = str.split(",\\s*");
        for(int j = 0; j < this_row.length; j++) {
          game[line_count][j] = Integer.parseInt(this_row[j]);
        }

        line_count++;

      } // end while

      inr.close();

    } catch (IOException e) {
      System.out.println(e);
    }

  } // end readFile


  public static void main(String[] args) throws Exception {

    if (args.length > 0) {
      String file = args[0];
      SurvoPuzzle.readFile(file);
    } else {
      r = default_r;
      c = default_c;
      game = default_game;
      rowsums = default_rowsums;
      colsums = default_colsums;
    }

    SurvoPuzzle.solve();

  }
}
