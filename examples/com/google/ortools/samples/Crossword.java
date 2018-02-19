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

public class Crossword {

  static {
    System.loadLibrary("jniortools");
  }

  /**
   *
   * Solving a simple crossword.
   * See http://www.hakank.org/google_or_tools/crossword2.py
   *
   */
  private static void solve() {

    Solver solver = new Solver("Crossword");

    //
    // data
    //
    String[] alpha = {"_","a","b","c","d","e","f",
                      "g","h","i","j","k","l","m",
                      "n","o","p","q","r","s","t",
                      "u","v","w","x","y","z"};

    int a=1;  int b=2;  int c=3; int d=4;  int e=5;  int f=6;
    int g=7;  int h=8;  int i=9; int j=10; int k=11; int l=12;
    int m=13; int n=14; int o=15; int p=16; int q=17; int r=18;
    int s=19; int t=20; int u=21; int v=22; int w=23; int x=24;
    int y=25; int z=26;

    int num_words = 15;
    int word_len = 5;

    int[][] AA = {{h, o, s, e, s},  //  HOSES
                  {l, a, s, e, r},  //  LASER
                  {s, a, i, l, s},  //  SAILS
                  {s, h, e, e, t},  //  SHEET
                  {s, t, e, e, r},  //  STEER
                  {h, e, e, l, 0},  //  HEEL
                  {h, i, k, e, 0},  //  HIKE
                  {k, e, e, l, 0},  //  KEEL
                  {k, n, o, t, 0},  //  KNOT
                  {l, i, n, e, 0},  //  LINE
                  {a, f, t, 0, 0},  //  AFT
                  {a, l, e, 0, 0},  //  ALE
                  {e, e, l, 0, 0},  //  EEL
                  {l, e, e, 0, 0},  //  LEE
                  {t, i, e, 0, 0}}; //  TIE

    int num_overlapping = 12;
    int[][] overlapping = {{0, 2, 1, 0},  //  s
                           {0, 4, 2, 0},  //  s

                           {3, 1, 1, 2},  //  i
                           {3, 2, 4, 0},  //  k
                           {3, 3, 2, 2},  //  e

                           {6, 0, 1, 3},  //  l
                           {6, 1, 4, 1},  //  e
                           {6, 2, 2, 3},  //  e

                           {7, 0, 5, 1},  //  l
                           {7, 2, 1, 4},  //  s
                           {7, 3, 4, 2},  //  e
                           {7, 4, 2, 4}}; //  r

    int N = 8;


    //
    // variables
    //
    IntVar[][] A = new IntVar[num_words][word_len];
    IntVar[] A_flat = new IntVar[num_words * word_len];
    // for labeling on A and E
    IntVar[] all = new IntVar[(num_words * word_len) + N];

    for(int I = 0; I < num_words; I++) {
      for(int J = 0; J < word_len; J++) {
        A[I][J] = solver.makeIntVar(0, 26, "A[" + I + "," + J + "]");
        A_flat[I * word_len + J] = A[I][J];
        all[I * word_len + J] = A[I][J];
      }
    }

    IntVar[] E = solver.makeIntVarArray(N, 0, num_words, "E");
    for(int I = 0; I < N; I++) {
      all[num_words * word_len + I] = E[I];
    }


    //
    // constraints
    //
    solver.addConstraint(solver.makeAllDifferent(E));

    for(int I = 0; I < num_words; I++) {
      for(int J = 0; J < word_len; J++) {
        solver.addConstraint(solver.makeEquality(A[I][J], AA[I][J]));
      }
    }

    for(int I = 0; I < num_overlapping; I++) {
      solver.addConstraint(
          solver.makeEquality(
              solver.makeElement(A_flat,
                  solver.makeSum(
                      solver.makeProd(
                          E[overlapping[I][0]], word_len).var(),
                      overlapping[I][1]).var()).var(),
              solver.makeElement(A_flat,
                  solver.makeSum(
                      solver.makeProd(
                          E[overlapping[I][2]], word_len).var(),
                      overlapping[I][3]).var()).var() ));
    }

    //
    // search
    //
    DecisionBuilder db = solver.makePhase(all,
                                          solver.INT_VAR_DEFAULT,
                                          solver.INT_VALUE_DEFAULT);

    solver.newSearch(db);

    //
    // output
    //
    while (solver.nextSolution()) {
      System.out.println("E:");
      for(int ee = 0; ee < N; ee++) {
        int e_val = (int)E[ee].value();
        System.out.print(ee + ": (" + e_val + ") ");
        for(int ii = 0; ii < word_len; ii++) {
          System.out.print(alpha[(int)A[ee][ii].value()]);
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

  public static void main(String[] args) throws Exception {

    Crossword.solve();

  }
}
