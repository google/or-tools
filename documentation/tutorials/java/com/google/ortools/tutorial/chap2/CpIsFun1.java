// Copyright 2012 Google
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
//
// Cryptoarithmetic puzzle
//
// First attempt to solve equation CP + IS + FUN = TRUE
// where each letter represents a unique digit.
//
// This problem has 72 different solutions in base 10.
//
// Use of NewSearch.
package com.google.ortools.tutorial.chap2;

import com.google.ortools.constraintsolver.*;

public class CpIsFun1 {

  static {
    System.loadLibrary("jniconstraintsolver");
  }

  private static final int kBase = 10;

  // helper functions
  static IntVar makeBaseLine2(Solver  s,
                              IntVar v1,
                              IntVar v2,
                              int base) {
    return s.makeSum(s.makeProd(v1, base).var(), v2).var();
  }

  static IntVar makeBaseLine3(Solver s,
                              IntVar v1,
                              IntVar v2,
                              IntVar v3,
                              int base) {
    IntVar[] tmp_vars = {v1, v2, v3};
    int[] coefficients = {base * base, base, 1};

    return s.makeScalProd(tmp_vars, coefficients).var();
  }

  //  We introduce the generic version
  static IntVar makeBaseLine(Solver s,
                            IntVar[] vars,
                            int base) {
    int len = vars.length;
    int coeff = 1;
    int[] coeffs = new int[len];
    for(int i = len-1; i >= 0; --i) {
      coeffs[i] = coeff;
      coeff *= base;
    }

    return s.makeScalProd(vars, coeffs).var();
  }


  private static void CPIsFun() {

    Solver solver = new Solver("Cp is fun!");

    IntVar c = solver.makeIntVar(1, kBase - 1, "C");
    IntVar p = solver.makeIntVar(0, kBase - 1, "P");
    IntVar i = solver.makeIntVar(1, kBase - 1, "I");
    IntVar s = solver.makeIntVar(0, kBase - 1, "S");
    IntVar f = solver.makeIntVar(1, kBase - 1, "F");
    IntVar u = solver.makeIntVar(0, kBase - 1, "U");
    IntVar n = solver.makeIntVar(0, kBase - 1, "N");
    IntVar t = solver.makeIntVar(1, kBase - 1, "T");
    IntVar r = solver.makeIntVar(0, kBase - 1, "R");
    IntVar e = solver.makeIntVar(0, kBase - 1, "E");

    // We need to group variables in an array to be able to use
    // the global constraint AllDifferent
    IntVar[] letters = {c, p, i, s, f, u, n, t, r, e};

    // Check if we have enough digits
    assert(letters.length <= kBase);

    // Constraints
    solver.addConstraint(solver.makeAllDifferent(letters));

    // CP + IS + FUN = TRUE
    IntVar term1 = makeBaseLine2(solver, c, p, kBase);
    IntVar term2 = makeBaseLine2(solver, i, s, kBase);
    IntVar term3 = makeBaseLine3(solver, f, u, n, kBase);
    IntVar sum_terms = solver.makeSum(solver.makeSum(term1,term2).var(),
                                           term3).var();

    IntVar[] sum_vars = {t, r, u, e};
    IntVar sum   = makeBaseLine(solver, sum_vars, kBase);

    solver.addConstraint(solver.makeEquality(sum_terms, sum));


    DecisionBuilder db = solver.makePhase(letters,
                                          solver.CHOOSE_FIRST_UNBOUND,
                                          solver.ASSIGN_MIN_VALUE);

    solver.newSearch(db);

    if (solver.nextSolution()) {
       System.out.println("Solution found:");
       System.out.println("C=" + c.value() + " " + "P=" + p.value() + " "
                        + "I=" + i.value() + " " + "S=" + s.value() + " "
              + "F=" + f.value() + " " + "U=" + u.value() + " "
              + "N=" + n.value() + " " + "T=" + t.value() + " "
              + "R=" + r.value() + " " + "E=" + e.value());
       // Is CP + IS + FUN = TRUE?
       assert(p.value() + s.value() + n.value() +
           kBase * (c.value() + i.value() + u.value()) +
           kBase * kBase * f.value() ==
           e.value() +
           kBase * u.value() +
           kBase * kBase * r.value() +
           kBase * kBase * kBase * t.value());

    } else {
      System.out.println("Cannot solve problem.");
  }  // if (solver.nextSolution())

    solver.endSearch();

  }

  public static void main(String[] args) throws Exception {
    CpIsFun1.CPIsFun();
  }
}
