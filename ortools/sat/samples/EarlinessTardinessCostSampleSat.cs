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

public class EarlinessTardinessCostSampleSat
{
  static void Main()
  {
    long earliness_date = 5;
    long earliness_cost = 8;
    long lateness_date = 15;
    long lateness_cost = 12;

    // Create the CP-SAT model.
    CpModel model = new CpModel();

    // Declare our primary variable.
    IntVar x = model.NewIntVar(0, 20, "x");

    // Create the expression variable and implement the piecewise linear
    // function.
    //
    //  \        /
    //   \______/
    //   ed    ld
    //
    long large_constant = 1000;
    IntVar expr = model.NewIntVar(0, large_constant, "expr");

    // First segment.
    IntVar s1 = model.NewIntVar(-large_constant, large_constant, "s1");
    model.Add(s1 == earliness_cost * (earliness_date - x));

    // Second segment.
    IntVar s2 = model.NewConstant(0);

    // Third segment.
    IntVar s3 = model.NewIntVar(-large_constant, large_constant, "s3");
    model.Add(s3 == lateness_cost * (x - lateness_date));

    // Link together expr and x through s1, s2, and s3.
    model.AddMaxEquality(expr, new IntVar[] {s1, s2, s3});

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
