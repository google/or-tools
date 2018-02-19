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

public class Minesweeper {

  static {
    System.loadLibrary("jniortools");
  }

  static int X = -1;

  //
  // Default problem.
  // It has 4 solutions.
  //
  static int default_r = 8;
  static int default_c = 8;
  static int[][] default_game =  {{2, 3, X, 2, 2, X, 2, 1},
                                  {X, X, 4, X, X, 4, X, 2},
                                  {X, X, X, X, X, X, 4, X},
                                  {X, 5, X, 6, X, X, X, 2},
                                  {2, X, X, X, 5, 5, X, 2},
                                  {1, 3, 4, X, X, X, 4, X},
                                  {0, 1, X, 4, X, X, X, 3},
                                  {0, 1, 2, X, 2, 3, X, 2}};

  // for the actual problem
  static int r;
  static int c;
  static int[][] game;


  /**
   *
   * Solves the Minesweeper problems.
   * See http://www.hakank.org/google_or_tools/minesweeper.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("Minesweeper");

    int[] S = {-1, 0, 1};

    //
    // data
    //
    System.out.println("Problem:");
    for(int i = 0; i < r; i++) {
      for(int j = 0; j < c; j++) {
        if (game[i][j] > X) {
          System.out.print(game[i][j] + " ");
        } else {
          System.out.print("X ");
        }
      }
      System.out.println();
    }
    System.out.println();


    //
    // Variables
    //
    IntVar[][] mines =  new IntVar[r][c];
    IntVar[] mines_flat = new IntVar[r * c]; // for branching
    for(int i = 0; i < r; i++) {
      for(int j = 0; j < c; j++) {
        mines[i][j] = solver.makeIntVar(0, 1, "mines[" + i + ", " + j + "]");
        mines_flat[i * c + j] = mines[i][j];
      }
    }

    //
    // Constraints
    //
    for(int i = 0; i < r; i++) {
      for(int j = 0; j < c; j++) {
        if (game[i][j] >= 0) {
          solver.addConstraint(
              solver.makeEquality(mines[i][j], 0));

          // this cell is the sum of all its neighbours
          ArrayList<IntVar> neighbours = new ArrayList<IntVar>();
          for(int a: S) {
            for(int b: S) {
              if (i + a >= 0 &&
                  j + b >= 0 &&
                  i + a < r &&
                  j + b < c) {
                neighbours.add(mines[i + a][j + b]);
              }
            }
          }
          solver.addConstraint(
              solver.makeSumEquality(
                  neighbours.toArray(new IntVar[1]), game[i][j]));
        }

        if (game[i][j] > X) {
          // This cell cannot be a mine since it
          // has some value assigned to it
          solver.addConstraint(
              solver.makeEquality(mines[i][j], 0));
        }
      }
    }

    //
    // Search
    //
    DecisionBuilder db = solver.makePhase(mines_flat,
                                          solver.INT_VAR_SIMPLE,
                                          solver.ASSIGN_MIN_VALUE);
    solver.newSearch(db);

    int sol = 0;
    while (solver.nextSolution()) {
      sol++;
      System.out.println("Solution #" + sol + ":");
      for(int i = 0; i < r; i++) {
        for(int j = 0; j < c; j++) {
          System.out.print(mines[i][j].value() + " ");
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
   * Reads a minesweeper file.
   * File format:
   *  # a comment which is ignored
   *  % a comment which also is ignored
   *  number of rows
   *  number of columns
   *  <
   *    row number of neighbours lines...
   *  >
   *
   * 0..8 means number of neighbours, "." mean unknown (may be a mine)
   *
   * Example (from minesweeper0.txt)
   * # Problem from Gecode/examples/minesweeper.cc  problem 0
   * 6
   * 6
   * ..2.3.
   * 2.....
   * ..24.3
   * 1.34..
   * .....3
   * .3.3..
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
          r = Integer.parseInt(str); // number of rows
        } else if (lineCount == 1) {
          c = Integer.parseInt(str); // number of columns
          game = new int[r][c];
        } else {
          // the problem matrix
          String row[] = str.split("");
          for(int j = 1; j <= c; j++) {
            String s = row[j];
            if (s.equals(".")) {
              game[lineCount-2][j-1] = -1;
            } else {
              game[lineCount-2][j-1] = Integer.parseInt(s);
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

    String file = "";
    if (args.length > 0) {
      file = args[0];
      Minesweeper.readFile(file);
    } else {
      game = default_game;
      r = default_r;
      c = default_c;
    }

    Minesweeper.solve();
  }
}
