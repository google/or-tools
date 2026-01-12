// Copyright 2010-2025 Google LLC
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
package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.BoolVar;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.SatParameters;

/** Link integer constraints together. */
public final class ChannelingSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our two primary variables.
    IntVar[] vars = new IntVar[] {model.newIntVar(0, 10, "x"), model.newIntVar(0, 10, "y")};

    // Declare our intermediate boolean variable.
    BoolVar b = model.newBoolVar("b");

    // Implement b == (x >= 5).
    model.addGreaterOrEqual(vars[0], 5).onlyEnforceIf(b);
    model.addLessOrEqual(vars[0], 4).onlyEnforceIf(b.not());

    // Create our two half-reified constraints.
    // First, b implies (y == 10 - x).
    model.addEquality(LinearExpr.sum(vars), 10).onlyEnforceIf(b);
    // Second, not(b) implies y == 0.
    model.addEquality(vars[1], 0).onlyEnforceIf(b.not());

    // Search for x values in increasing order.
    model.addDecisionStrategy(new IntVar[] {vars[0]},
        DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_FIRST,
        DecisionStrategyProto.DomainReductionStrategy.SELECT_MIN_VALUE);

    // Create the solver.
    CpSolver solver = new CpSolver();

    // Force the solver to follow the decision strategy exactly.
    solver.getParameters().setSearchBranching(SatParameters.SearchBranching.FIXED_SEARCH);
    // Tell the solver to enumerate all solutions.
    solver.getParameters().setEnumerateAllSolutions(true);

    // Solve the problem with the printer callback.
    CpSolverStatus unusedStatus = solver.solve(model, new CpSolverSolutionCallback() {
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
    }.init(new IntVar[] {vars[0], vars[1], b}));
  }

  private ChannelingSampleSat() {}
}
// [END program]
