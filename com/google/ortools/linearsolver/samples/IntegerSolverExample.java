// Copyright 2010-2011 Google
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

package com.google.ortools.linearsolver.samples;

import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;

/**
 * Example of linear solver usage.
 *
 */

public class IntegerSolverExample {

  static {
    System.loadLibrary("jnilinearsolver");
  }


  private static void runFirstIntegerExample(int solverType) {
    MPSolver solver = new MPSolver("runFirstIntegerExample", solverType);
    double infinity = solver.infinity();
    MPVariable x1 = solver.makeIntVar(0.0, infinity, "x1");
    MPVariable x2 = solver.makeIntVar(0.0, infinity, "x2");

    solver.addObjectiveTerm(x1, 1);
    solver.addObjectiveTerm(x2, 2);

    MPConstraint ct = solver.makeConstraint(17, infinity);
    ct.addTerm(x1, 3);
    ct.addTerm(x2, 2);

    // Check the solution.
    solver.solve();
    System.out.println("Objective value = " +  solver.objectiveValue());
  }

  public static void main(String[] args) throws Exception {
    runFirstIntegerExample(MPSolver.CBC_MIXED_INTEGER_PROGRAMMING);
    runFirstIntegerExample(MPSolver.GLPK_MIXED_INTEGER_PROGRAMMING);
    runFirstIntegerExample(MPSolver.SCIP_MIXED_INTEGER_PROGRAMMING);
  }
}
