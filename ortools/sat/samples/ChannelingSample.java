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

import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.SatParameters;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.IntVar;

public class ChannelingSample {

  static { System.loadLibrary("jniortools"); }

  public static void main(String[] args) throws Exception {
    // Model.
    CpModel model = new CpModel();

    // Variables.
    IntVar x = model.newIntVar(0, 10, "x");
    IntVar y = model.newIntVar(0, 10, "y");

    IntVar b = model.newBoolVar("b");

    // Implements b == (x >= 5).
    model.addGreaterOrEqual(x, 5).onlyEnforceIf(b);
    model.addLessOrEqual(x, 4).onlyEnforceIf(b.not());

    // b implies (y == 10 - x).
    model.addLinearSumEqual(new IntVar[] {x, y}, 10).onlyEnforceIf(b);
    // not(b) implies y == 0.
    model.addEquality(y, 0).onlyEnforceIf(b.not());

    // Searches for x values in increasing order.
    model.addDecisionStrategy(
        new IntVar[] {x},
        DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_FIRST,
        DecisionStrategyProto.DomainReductionStrategy.SELECT_MIN_VALUE);

    // Creates the solver.
    CpSolver solver = new CpSolver();

    // Forces the solver to follow the decision strategy exactly.
    solver.getParameters().setSearchBranching(
        SatParameters.SearchBranching.FIXED_SEARCH);

    // Solves the problem with the printer callback.
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
