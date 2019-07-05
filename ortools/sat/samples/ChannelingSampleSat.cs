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
using Google.OrTools.Util;

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

public class ChannelingSampleSat
{
  static void Main()
  {
    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our two primary variables.
    IntVar x = model.NewIntVar(0, 10, "x");
    IntVar y = model.NewIntVar(0, 10, "y");

    // Declare our intermediate boolean variable.
    IntVar b = model.NewBoolVar("b");

    // Implement b == (x >= 5).
    model.Add(x >= 5).OnlyEnforceIf(b);
    model.Add(x < 5).OnlyEnforceIf(b.Not());

    // Create our two half-reified constraints.
    // First, b implies (y == 10 - x).
    model.Add(y == 10 - x).OnlyEnforceIf(b);
    // Second, not(b) implies y == 0.
    model.Add(y == 0).OnlyEnforceIf(b.Not());

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
        new VarArraySolutionPrinter(new IntVar[] {x, y, b});
    solver.SearchAllSolutions(model, cb);
  }
}
