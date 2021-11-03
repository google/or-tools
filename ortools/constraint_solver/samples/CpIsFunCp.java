// Copyright 2010-2021 Google LLC
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

// [START program]
// Cryptarithmetic puzzle
//
// First attempt to solve equation CP + IS + FUN = TRUE
// where each letter represents a unique digit.
//
// This problem has 72 different solutions in base 10.
package com.google.ortools.constraintsolver.samples;
// [START import]
// [END import]
import com.google.ortools.Loader;
import com.google.ortools.constraintsolver.DecisionBuilder;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.Solver;
// [END import]

/** Cryptarithmetic puzzle. */
public final class CpIsFunCp {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Instantiate the solver.
    // [START solver]
    Solver solver = new Solver("CP is fun!");
    // [END solver]

    // [START variables]
    final int base = 10;

    // Decision variables.
    final IntVar c = solver.makeIntVar(1, base - 1, "C");
    final IntVar p = solver.makeIntVar(0, base - 1, "P");
    final IntVar i = solver.makeIntVar(1, base - 1, "I");
    final IntVar s = solver.makeIntVar(0, base - 1, "S");
    final IntVar f = solver.makeIntVar(1, base - 1, "F");
    final IntVar u = solver.makeIntVar(0, base - 1, "U");
    final IntVar n = solver.makeIntVar(0, base - 1, "N");
    final IntVar t = solver.makeIntVar(1, base - 1, "T");
    final IntVar r = solver.makeIntVar(0, base - 1, "R");
    final IntVar e = solver.makeIntVar(0, base - 1, "E");

    // Group variables in a vector so that we can use AllDifferent.
    final IntVar[] letters = new IntVar[] {c, p, i, s, f, u, n, t, r, e};

    // Verify that we have enough digits.
    if (base < letters.length) {
      throw new Exception("base < letters.Length");
    }
    // [END variables]

    // Define constraints.
    // [START constraints]
    solver.addConstraint(solver.makeAllDifferent(letters));

    // CP + IS + FUN = TRUE
    final IntVar sum1 =
          solver.makeSum(new IntVar[] {
            p, s, n,
            solver.makeProd(solver.makeSum(new IntVar[]{c, i, u}).var(), base).var(),
            solver.makeProd(f, base * base).var()}).var();
    final IntVar sum2 =
          solver.makeSum(new IntVar[] {
            e,
            solver.makeProd(u, base).var(),
            solver.makeProd(r, base * base).var(),
            solver.makeProd(t, base * base * base).var()}).var();
    solver.addConstraint(solver.makeEquality(sum1, sum2));
    // [END constraints]

    // [START solve]
    int countSolution = 0;
    // Create the decision builder to search for solutions.
    final DecisionBuilder db =
        solver.makePhase(letters, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
    solver.newSearch(db);
    while (solver.nextSolution()) {
      System.out.println("C=" + c.value() + " P=" + p.value());
      System.out.println(" I=" + i.value() + " S=" + s.value());
      System.out.println(" F=" + f.value() + " U=" + u.value());
      System.out.println(" N=" + n.value() + " T=" + t.value());
      System.out.println(" R=" + r.value() + " E=" + e.value());

      // Is CP + IS + FUN = TRUE?
      if (p.value() + s.value() + n.value() + base * (c.value() + i.value() + u.value())
              + base * base * f.value()
          != e.value() + base * u.value() + base * base * r.value()
              + base * base * base * t.value()) {
        throw new Exception("CP + IS + FUN != TRUE");
      }
      countSolution++;
    }
    solver.endSearch();
    System.out.println("Number of solutions found: " + countSolution);
    // [END solve]
  }

  private CpIsFunCp() {}
}
// [END program]
