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

using System;
using Google.OrTools.Sat;

public class VarArraySolutionPrinter : CpSolverSolutionCallback
{
  public VarArraySolutionPrinter(IntVar[] variables)
  {
    variables_ = variables;
  }

  public override void OnSolutionCallback()
  {
    {
      foreach (IntVar v in variables_)
      {
        Console.Write(String.Format("{0}={1} ", v.ShortString(), Value(v)));
      }
      Console.WriteLine();
    }
  }

  private IntVar[] variables_;
}

public class StepFunctionSampleSat
{
  static void Main()
  {
    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our primary variable.
    IntVar x = model.NewIntVar(0, 20, "x");

    // Create the expression variable and implement the step function
    // Note it is not defined for var == 2.
    //
    //        -               3
    // -- --      ---------   2
    //                        1
    //      -- ---            0
    // 0 ================ 20
    //
    IntVar expr = model.NewIntVar(0, 3, "expr");

    // expr == 0 on [5, 6] U [8, 10]
    ILiteral b0 = model.NewBoolVar("b0");
    model.AddLinearConstraintWithBounds(
        new IntVar[] {x},
        new long[] {1},
        new long[] {5, 6, 8, 10}).OnlyEnforceIf(b0);
    model.Add(expr == 0).OnlyEnforceIf(b0);

    // expr == 2 on [0, 1] U [3, 4] U [11, 20]
    ILiteral b2 = model.NewBoolVar("b2");
    model.AddLinearConstraintWithBounds(
        new IntVar[] {x},
        new long[] {1},
        new long[] {0, 1, 3, 4, 11, 20}).OnlyEnforceIf(b2);
    model.Add(expr == 2).OnlyEnforceIf(b2);

    // expr == 3 when x == 7
    ILiteral b3 = model.NewBoolVar("b3");
    model.Add(x == 7).OnlyEnforceIf(b3);
    model.Add(expr == 3).OnlyEnforceIf(b3);

    // At least one bi is true. (we could use a sum == 1).
    model.AddBoolOr(new ILiteral[] {b0, b2, b3});

    // Search for x values in increasing order.
    model.AddDecisionStrategy(
        new IntVar[] {x},
        DecisionStrategyProto.Types.VariableSelectionStrategy.ChooseFirst,
        DecisionStrategyProto.Types.DomainReductionStrategy.SelectMinValue);

    // Create the solver.
    CpSolver solver = new CpSolver();

    // Force solver to follow the decision strategy exactly.
    solver.StringParameters = "search_branching:FIXED_SEARCH";

    VarArraySolutionPrinter cb =
        new VarArraySolutionPrinter(new IntVar[] {x, expr});
    solver.SearchAllSolutions(model, cb);
  }
}
