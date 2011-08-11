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

public class LinearSolverExample {

  static {
    System.loadLibrary("jnilinearsolver");
  }


  private static void runFirstLinearExample(int solverType) {
    MPSolver solver = new MPSolver("runFirstLinearExample", solverType);
    double infinity = solver.infinity();
    MPVariable x1 = solver.makeNumVar(0.0, infinity, "x1");
    MPVariable x2 = solver.makeNumVar(0.0, infinity, "x2");
    MPVariable x3 = solver.makeNumVar(0.0, infinity, "x3");
    System.out.println("Number of variables = " + solver.numVariables());

    solver.addObjectiveTerm(x1, 10);
    solver.addObjectiveTerm(x2, 6);
    solver.addObjectiveTerm(x3, 4);
    solver.setMaximization();

    MPConstraint c0 = solver.makeConstraint(-infinity, 100.0);
    c0.addTerm(x1, 1);
    c0.addTerm(x2, 1);
    c0.addTerm(x3, 1);
    MPConstraint c1 = solver.makeConstraint(-infinity, 600.0);
    c1.addTerm(x1, 10);
    c1.addTerm(x2, 4);
    c1.addTerm(x3, 5);
    MPConstraint c2 = solver.makeConstraint(-infinity, 300.0);
    c2.addTerm(x1, 2);
    c2.addTerm(x2, 2);
    c2.addTerm(x3, 6);
    System.out.println("Number of constraints = " + solver.numConstraints());

    // The problem has an optimal solution.
    solver.solve();

    System.out.println("Optimal value = " + solver.objectiveValue());
    System.out.println("x1 = " + x1.solutionValue() +
                       " reduced cost = " + x1.reducedCost());
    System.out.println("x2 = " + x2.solutionValue() +
                       " reduced cost = " + x2.reducedCost());
    System.out.println("x3 = " + x3.solutionValue() +
                       " reduced cost = " + x3.reducedCost());
    System.out.println("c0 dual value = " + c0.dualValue() +
                       " activity = " + c0.activity());
    System.out.println("c1 dual value = " + c1.dualValue() +
                       " activity = " + c1.activity());
    System.out.println("c2 dual value = " + c2.dualValue() +
                       " activity = " + c2.activity());
  }

  public static void main(String[] args) throws Exception {
    runFirstLinearExample(MPSolver.CLP_LINEAR_PROGRAMMING);
    runFirstLinearExample(MPSolver.GLPK_LINEAR_PROGRAMMING);
  }
}
