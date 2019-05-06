// Copyright 2010-2018 Google LLC
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

import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.SatParameters;

/** Link integer constraints together. */
public class ChannelingSampleSat {
  static {
    System.loadLibrary("jniortools");
  }

  public static void main(String[] args) throws Exception {
    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our two primary variables.
    IntVar x = model.newIntVar(0, 10, "x");
    IntVar y = model.newIntVar(0, 10, "y");

    // Declare our intermediate boolean variable.
    IntVar b = model.newBoolVar("b");

    // Implement b == (x >= 5).
    model.addGreaterOrEqual(x, 5).onlyEnforceIf(b);
    model.addLessOrEqual(x, 4).onlyEnforceIf(b.not());

    // Create our two half-reified constraints.
    // First, b implies (y == 10 - x).
    model.addEquality(LinearExpr.sum(new IntVar[] {x, y}), 10).onlyEnforceIf(b);
    // Second, not(b) implies y == 0.
    model.addEquality(y, 0).onlyEnforceIf(b.not());

    // Search for x values in increasing order.
    model.addDecisionStrategy(new IntVar[] {x},
        DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_FIRST,
        DecisionStrategyProto.DomainReductionStrategy.SELECT_MIN_VALUE);

    // Create the solver.
    CpSolver solver = new CpSolver();

    // Force the solver to follow the decision strategy exactly.
    solver.getParameters().setSearchBranching(SatParameters.SearchBranching.FIXED_SEARCH);

    // Solve the problem with the printer callback.
    solver.searchAllSolutions(model, new CpSolverSolutionCallback() {
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
    }.init(new IntVar[] {x, y, b}));
  }
}
