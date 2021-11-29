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

package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.Literal;
import com.google.ortools.sat.SatParameters;
import com.google.ortools.util.Domain;

/** Link integer constraints together. */
public class StepFunctionSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our primary variable.
    IntVar x = model.newIntVar(0, 20, "x");

    // Create the expression variable and implement the step function
    // Note it is not defined for var == 2.
    //
    //        -               3
    // -- --      ---------   2
    //                        1
    //      -- ---            0
    // 0 ================ 20
    //
    IntVar expr = model.newIntVar(0, 3, "expr");

    // expr == 0 on [5, 6] U [8, 10]
    Literal b0 = model.newBoolVar("b0");
    model.addLinearExpressionInDomain(x, Domain.fromValues(new long[] {5, 6, 8, 9, 10}))
        .onlyEnforceIf(b0);
    model.addEquality(expr, 0).onlyEnforceIf(b0);

    // expr == 2 on [0, 1] U [3, 4] U [11, 20]
    Literal b2 = model.newBoolVar("b2");
    model
        .addLinearExpressionInDomain(
            x, Domain.fromIntervals(new long[][] {{0, 1}, {3, 4}, {11, 20}}))
        .onlyEnforceIf(b2);
    model.addEquality(expr, 2).onlyEnforceIf(b2);

    // expr == 3 when x = 7
    Literal b3 = model.newBoolVar("b3");
    model.addEquality(x, 7).onlyEnforceIf(b3);
    model.addEquality(expr, 3).onlyEnforceIf(b3);

    // At least one bi is true. (we could use a sum == 1).
    model.addBoolOr(new Literal[] {b0, b2, b3});

    // Search for x values in increasing order.
    model.addDecisionStrategy(new IntVar[] {x},
        DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_FIRST,
        DecisionStrategyProto.DomainReductionStrategy.SELECT_MIN_VALUE);

    // Create the solver.
    CpSolver solver = new CpSolver();

    // Force the solver to follow the decision strategy exactly.
    solver.getParameters().setSearchBranching(SatParameters.SearchBranching.FIXED_SEARCH);
    // Tell the solver to enumerate all solutions.
    solver.getParameters().setEnumerateAllSolutions(true);

    // Solve the problem with the printer callback.
    solver.solve(model, new CpSolverSolutionCallback() {
      public CpSolverSolutionCallback init(IntVar[] variables) {
        variableArray = variables;
        return this;
      }

      @Override
      public void onSolutionCallback() {
        for (IntVar v : variableArray) {
          System.out.printf("%s=%d ", v.getName(), value(v));
        }
        System.out.println();
      }

      private IntVar[] variableArray;
    }.init(new IntVar[] {x, expr}));
  }
}
