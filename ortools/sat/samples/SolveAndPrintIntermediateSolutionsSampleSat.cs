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

public class VarArraySolutionPrinterWithObjective : CpSolverSolutionCallback
{
  public VarArraySolutionPrinterWithObjective(IntVar[] variables)
  {
    variables_ = variables;
  }

  public override void OnSolutionCallback()
  {
    Console.WriteLine(String.Format("Solution #{0}: time = {1:F2} s",
          solution_count_, WallTime()));
    Console.WriteLine(
        String.Format("  objective value = {0}", ObjectiveValue()));
    foreach (IntVar v in variables_)
    {
      Console.WriteLine(
          String.Format("  {0} = {1}", v.ShortString(), Value(v)));
    }
    solution_count_++;
  }

  public int SolutionCount()
  {
    return solution_count_;
  }

  private int solution_count_;
  private IntVar[] variables_;
}

public class SolveAndPrintIntermediateSolutionsSampleSat
{
  static void Main()
  {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    int num_vals = 3;

    IntVar x = model.NewIntVar(0, num_vals - 1, "x");
    IntVar y = model.NewIntVar(0, num_vals - 1, "y");
    IntVar z = model.NewIntVar(0, num_vals - 1, "z");

    // Adds a different constraint.
    model.Add(x != y);

    // Maximizes a linear combination of variables.
    model.Maximize(x + 2 * y + 3 * z);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinterWithObjective cb =
      new VarArraySolutionPrinterWithObjective(new IntVar[] { x, y, z });
    solver.SolveWithSolutionCallback(model, cb);
    Console.WriteLine(String.Format("Number of solutions found: {0}",
          cb.SolutionCount()));
  }
}
