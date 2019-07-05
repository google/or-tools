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

/** Encode the piecewise linear expression. */
public class EarlinessTardinessCostSampleSat {
  static {
    System.loadLibrary("jniortools");
  }

  public static void main(String[] args) throws Exception {
    long earlinessDate = 5;
    long earlinessCost = 8;
    long latenessDate = 15;
    long latenessCost = 12;

    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our primary variable.
    IntVar x = model.newIntVar(0, 20, "x");

    // Create the expression variable and implement the piecewise linear function.
    //
    //  \        /
    //   \______/
    //   ed    ld
    //
    long largeConstant = 1000;
    IntVar expr = model.newIntVar(0, largeConstant, "expr");

    // First segment: s1 == earlinessCost * (earlinessDate - x).
    IntVar s1 = model.newIntVar(-largeConstant, largeConstant, "s1");
    model.addEquality(LinearExpr.scalProd(new IntVar[] {s1, x}, new long[] {1, earlinessCost}),
        earlinessCost * earlinessDate);

    // Second segment.
    IntVar s2 = model.newConstant(0);

    // Third segment: s3 == latenessCost * (x - latenessDate).
    IntVar s3 = model.newIntVar(-largeConstant, largeConstant, "s3");
    model.addEquality(LinearExpr.scalProd(new IntVar[] {s3, x}, new long[] {1, -latenessCost}),
        -latenessCost * latenessDate);

    // Link together expr and x through s1, s2, and s3.
    model.addMaxEquality(expr, new IntVar[] {s1, s2, s3});

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
    }.init(new IntVar[] {x, expr}));
  }
}
