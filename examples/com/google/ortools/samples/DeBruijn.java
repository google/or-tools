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

public class DeBruijn {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   *
   *  toNum(solver, a, num, base)
   *
   *  channelling between the array a and the number num
   *
   */
  private static void toNum(Solver solver, IntVar[] a, IntVar num, int base) {
    int len = a.length;

    IntVar[] tmp = new IntVar[len];
    for(int i = 0; i < len; i++) {
      tmp[i] = solver.makeProd(a[i], (int)Math.pow(base,(len-i-1))).var();
    }
    solver.addConstraint(
        solver.makeEquality(solver.makeSum(tmp).var(), num));
  }


  /**
   *
   * Implements "arbitrary" de Bruijn sequences.
   * See http://www.hakank.org/google_or_tools/debruijn_binary.py
   *
   */
  private static void solve(int base, int n, int m) {

    Solver solver = new Solver("DeBruijn");

    System.out.println("base: " + base + " n: " + n + " m: " + m);

    // Ensure that the number of each digit in bin_code is
    // the same. Nice feature, but it can slow things down...
    boolean check_same_gcc = false; // true;

    //
    // variables
    //
    IntVar[] x =
      solver.makeIntVarArray(m, 0, (int)Math.pow(base, n) - 1, "x");

    IntVar[][] binary = new IntVar[m][n];
    for(int i = 0; i < m; i++) {
      for(int j = 0; j < n; j++) {
        binary[i][j] =
          solver.makeIntVar(0, base - 1, "binary[" + i + "," + j + "]");
      }
    }

    // this is the de Bruijn sequence
    IntVar[] bin_code =
      solver.makeIntVarArray(m, 0, base - 1, "bin_code");

    // occurences of each number in bin_code
    IntVar[] gcc = solver.makeIntVarArray(base, 0, m, "gcc");

    // for the branching
    IntVar[] all = new IntVar[2 * m + base];
    for(int i = 0; i < m; i++) {
      all[i] = x[i];
      all[m + i] = bin_code[i];
    }
    for(int i = 0; i < base; i++) {
      all[2 * m + i] = gcc[i];
    }

    //
    // constraints
    //
    solver.addConstraint(solver.makeAllDifferent(x));

    // converts x <-> binary
    for(int i = 0; i < m; i++) {
      IntVar[] t = new IntVar[n];
      for(int j = 0; j < n; j++) {
        t[j] = binary[i][j];
      }
      toNum(solver, t, x[i], base);
    }

    // the de Bruijn condition:
    // the first elements in binary[i] is the same as the last
    // elements in binary[i-1]
    for(int i = 1; i < m; i++) {
      for(int j = 1; j < n; j++) {
        solver.addConstraint(
            solver.makeEquality(binary[i - 1][j], binary[i][j - 1]));
      }
    }

    // ... and around the corner
    for(int j = 1; j < n; j++) {
      solver.addConstraint(
          solver.makeEquality(binary[m - 1][j], binary[0][j - 1]));
    }

    // converts binary -> bin_code (de Bruijn sequence)
    for(int i = 0; i < m; i++) {
      solver.addConstraint(solver.makeEquality(bin_code[i], binary[i][0]));
    }


    // extra: ensure that all the numbers in the de Bruijn sequence
    // (bin_code) has the same occurrences (if check_same_gcc is True
    // and mathematically possible)
    solver.addConstraint(solver.makeDistribute(bin_code, gcc));
    if (check_same_gcc && m % base == 0) {
      for(int i = 1; i < base; i++) {
        solver.addConstraint(solver.makeEquality(gcc[i], gcc[i - 1]));
      }
    }

    // symmetry breaking:
    // the minimum value of x should be first
    solver.addConstraint(solver.makeEquality(x[0], solver.makeMin(x).var()));


    //
    // search
    //
    DecisionBuilder db = solver.makePhase(all,
                                          solver.CHOOSE_MIN_SIZE_LOWEST_MAX,
                                          solver.ASSIGN_MIN_VALUE);

    solver.newSearch(db);

    //
    // output
    //
    while (solver.nextSolution()) {
      System.out.print("x: ");
      for(int i = 0; i < m; i++) {
        System.out.print(x[i].value() + " ");
      }

      System.out.print("\nde Bruijn sequence:");
      for(int i = 0; i < m; i++) {
        System.out.print(bin_code[i].value() + " ");
      }

      System.out.print("\ngcc: ");
      for(int i = 0; i < base; i++) {
        System.out.print(gcc[i].value() + " ");
      }
      System.out.println("\n");


      // for debugging etc: show the full binary table
      /*
      System.out.println("binary:");
      for(int i = 0; i < m; i++) {
        for(int j = 0; j < n; j++) {
          System.out.print(binary[i][j].value() + " ");
        }
        System.out.println();
      }
      System.out.println();
      */

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

    int base = 2;
    int n = 3;
    int m = 8;

    if (args.length > 0) {
      base = Integer.parseInt(args[0]);
    }

    if (args.length > 1) {
      n = Integer.parseInt(args[1]);
      m = (int)Math.pow(base, n);
    }

    if (args.length > 2) {
      int m_max = (int) Math.pow(base, n);
      m = Integer.parseInt(args[2]);
      if (m > m_max) {
        System.out.println("m(" + m + ") is too large. Set m to " +
                           m_max + ".");
        m = m_max;
      }
    }

    DeBruijn.solve(base, n, m);
  }
}
