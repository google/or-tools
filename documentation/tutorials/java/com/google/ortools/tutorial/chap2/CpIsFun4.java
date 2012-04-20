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
// Use of SolutionCollectors.
// Use of Solve().
// Use of gflags to choose the base.
// Change the time limit of the solver.
// Use of ExportProfilingOverview().
package com.google.ortools.tutorial.chap2;

import com.google.ortools.constraintsolver.*;

public class CpIsFun4 {

  static {
    System.loadLibrary("jniconstraintsolver");
  }

//  private static final int kBase = 10;

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


  private static void CPIsFun(int base, long time_limit, boolean print) {

    // Use some profiling and change the default parameters of the solver
    SolverParameters solver_params = new SolverParameters();
    // Change the profile level
    solver_params.setProfile_level(SolverParameters.NORMAL_PROFILING);

    // Constraint programming engine
    Solver solver = new Solver("Cp is fun!", solver_params);

    IntVar c = solver.makeIntVar(1, base - 1, "C");
    IntVar p = solver.makeIntVar(0, base - 1, "P");
    IntVar i = solver.makeIntVar(1, base - 1, "I");
    IntVar s = solver.makeIntVar(0, base - 1, "S");
    IntVar f = solver.makeIntVar(1, base - 1, "F");
    IntVar u = solver.makeIntVar(0, base - 1, "U");
    IntVar n = solver.makeIntVar(0, base - 1, "N");
    IntVar t = solver.makeIntVar(1, base - 1, "T");
    IntVar r = solver.makeIntVar(0, base - 1, "R");
    IntVar e = solver.makeIntVar(0, base - 1, "E");

    // We need to group variables in an array to be able to use
    // the global constraint AllDifferent
    IntVar[] letters = {c, p, i, s, f, u, n, t, r, e};

    // Check if we have enough digits
    assert(base >= letters.length);

    // Constraints
    solver.addConstraint(solver.makeAllDifferent(letters));

    // CP + IS + FUN = TRUE
    IntVar term1 = makeBaseLine2(solver, c, p, base);
    IntVar term2 = makeBaseLine2(solver, i, s, base);
    IntVar term3 = makeBaseLine3(solver, f, u, n, base);
    IntVar sum_terms = solver.makeSum(solver.makeSum(term1,term2).var(),
                                           term3).var();

    IntVar[] sum_vars = {t, r, u, e};
    IntVar sum   = makeBaseLine(solver, sum_vars, base);

    solver.addConstraint(solver.makeEquality(sum_terms, sum));

    SolutionCollector all_solutions = solver.makeAllSolutionCollector();
    //  Add the interesting variables to the SolutionCollector
    all_solutions.add(letters);

    DecisionBuilder db = solver.makePhase(letters,
                                          solver.CHOOSE_FIRST_UNBOUND,
                                          solver.ASSIGN_MIN_VALUE);

    // Add some time limit
    SearchLimit t_limit = solver.makeTimeLimit(time_limit);
    solver.solve(db, all_solutions, t_limit);

    //  Retrieve the solutions
    final int numberSolutions = all_solutions.solutionCount();
    System.out.println("Number of solutions: " + numberSolutions);

    if (print) {
      for (int index = 0; index < numberSolutions; ++index) {
        System.out.println("C=" + all_solutions.value(index, c) +
                           " P=" + all_solutions.value(index, p) +
                           " I=" + all_solutions.value(index, i) +
                           " S=" + all_solutions.value(index, s) +
                           " F=" + all_solutions.value(index, f) +
                           " U=" + all_solutions.value(index, u) +
                           " N=" + all_solutions.value(index, n) +
                           " T=" + all_solutions.value(index, t) +
                           " R=" + all_solutions.value(index, r) +
                           " E=" + all_solutions.value(index, e)
        );
        // Is CP + IS + FUN = TRUE?
        assert(all_solutions.value(index, p) + all_solutions.value(index, s)
               + all_solutions.value(index, n)
               + base * (all_solutions.value(index, c)
               + all_solutions.value(index, i)
               + all_solutions.value(index, u))
               + base * base * all_solutions.value(index, f) ==
               all_solutions.value(index, e)
               + base * all_solutions.value(index, u)
               + base * base * all_solutions.value(index, r)
               + base * base * base * all_solutions.value(index, t));
      }
    }

  }

  public static void main(String[] args) throws Exception {
    int kBase = 10;
    long kTimeLimit = 10000;
    boolean kPrintSolution = false;
    if (args.length > 0) {
      try {
       kBase = Integer.parseInt(args[0]);
      } catch (NumberFormatException e) {
        System.err.println("First argument must be an integer (base)");
        System.exit(1);
      }
      if (args.length > 1) {
        try {
         kTimeLimit = Long.parseLong(args[1]);
        } catch (NumberFormatException e) {
          System.err.println("Second argument must be an integer" +
                                                    " (time limit in ms)");
          System.exit(1);
        }
        if (args.length > 2) {
          try {
            kPrintSolution = Boolean.parseBoolean(args[2]);
          } catch (NumberFormatException e) {
            System.err.println("Thrid argument must be a boolean (print?)");
            System.exit(1);
          }
        }
      }
    }
    CpIsFun4.CPIsFun(kBase, kTimeLimit, kPrintSolution);
  }
}
