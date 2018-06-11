// Copyright 2010-2017 Google
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
      Console.WriteLine(String.Format("Solution #{0}: time = {1:F2} s",
                                      solution_count_, WallTime()));
      foreach (IntVar v in variables_)
      {
        Console.WriteLine(
            String.Format("  {0} = {1}", v.ShortString(), Value(v)));
      }
      solution_count_++;
    }
  }

  public int SolutionCount()
  {
    return solution_count_;
  }

  private int solution_count_;
  private IntVar[] variables_;
}

public class VarArraySolutionPrinterWithObjective : CpSolverSolutionCallback
{
  public VarArraySolutionPrinterWithObjective(IntVar[] variables)
  {
    variables_ = variables;
  }

  public override void OnSolutionCallback()
  {
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
  }

  public int SolutionCount()
  {
    return solution_count_;
  }

  private int solution_count_;
  private IntVar[] variables_;
}


public class CodeSamplesSat
{
  static void CodeSample()
  {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the Boolean variable.
    IntVar x = model.NewBoolVar("x");
  }

  static void LiteralSample()
  {
    CpModel model = new CpModel();
    IntVar x = model.NewBoolVar("x");
    ILiteral not_x = x.Not();
  }

  static void BoolOrSample()
  {
    CpModel model = new CpModel();
    IntVar x = model.NewBoolVar("x");
    IntVar y = model.NewBoolVar("y");
    model.AddBoolOr(new ILiteral[] {x, y.Not()});
  }

  static void ReifiedSample()
  {
    CpModel model = new CpModel();

    IntVar x = model.NewBoolVar("x");
    IntVar y = model.NewBoolVar("y");
    IntVar b = model.NewBoolVar("b");

    //  First version using a half-reified bool and.
    model.AddBoolAnd(new ILiteral[] {x, y.Not()}).OnlyEnforceIf(b);

    // Second version using implications.
    model.AddImplication(b, x);
    model.AddImplication(b, y.Not());

    // Third version using bool or.
    model.AddBoolOr(new ILiteral[] {b.Not(), x});
    model.AddBoolOr(new ILiteral[] {b.Not(), y.Not()});
  }

  static void RabbitsAndPheasants()
  {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    IntVar r = model.NewIntVar(0, 100, "r");
    IntVar p = model.NewIntVar(0, 100, "p");
    // 20 heads.
    model.Add(r + p == 20);
    // 56 legs.
    model.Add(4 * r + 2 * p == 56);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);

    if (status == CpSolverStatus.ModelSat)
    {
      Console.WriteLine(solver.Value(r) + " rabbits, and " +
                        solver.Value(p) + " pheasants");
    }
  }

  static void BinpackingProblem()
  {
    // Data.
    int bin_capacity = 100;
    int slack_capacity = 20;
    int num_bins = 10;

    int[,] items = new int[,] { {20, 12}, {15, 12}, {30, 8}, {45, 5} };
    int num_items = items.GetLength(0);

    // Model.
    CpModel model = new CpModel();

    // Main variables.
    IntVar[,] x = new IntVar[num_items, num_bins];
    for (int i = 0; i < num_items; ++i)
    {
      int num_copies = items[i, 1];
      for (int b = 0; b < num_bins; ++b)
      {
        x[i, b] = model.NewIntVar(0, num_copies, String.Format("x_{0}_{1}", i, b));
      }
    }

    // Load variables.
    IntVar[] load = new IntVar[num_bins];
    for (int b = 0; b < num_bins; ++b)
    {
      load[b] = model.NewIntVar(0, bin_capacity, String.Format("load_{0}", b));
    }

    // Slack variables.
    IntVar[] slacks = new IntVar[num_bins];
    for (int b = 0; b < num_bins; ++b)
    {
      slacks[b] = model.NewBoolVar(String.Format("slack_{0}", b));
    }

    // Links load and x.
    int[] sizes = new int[num_items];
    for (int i = 0; i < num_items; ++i) {
      sizes[i] = items[i, 0];
    }
    for (int b = 0; b < num_bins; ++b)
    {
      IntVar[] tmp = new IntVar[num_items];
      for (int i = 0; i < num_items; ++i)
      {
        tmp[i] = x[i, b];
      }
      model.Add(load[b] == tmp.ScalProd(sizes));
    }

    // Place all items.
    for (int i = 0; i < num_items; ++i)
    {
      IntVar[] tmp = new IntVar[num_bins];
      for (int b = 0; b < num_bins; ++b)
      {
        tmp[b] = x[i, b];
      }
      model.Add(tmp.Sum() == items[i, 1]);
    }

    // Links load and slack.
    int safe_capacity = bin_capacity - slack_capacity;
    for (int b = 0; b < num_bins; ++b)
    {
      //  slack[b] => load[b] <= safe_capacity.
      model.Add(load[b] <= safe_capacity).OnlyEnforceIf(slacks[b]);
      // not(slack[b]) => load[b] > safe_capacity.
      model.Add(load[b] > safe_capacity).OnlyEnforceIf(slacks[b].Not());
    }

    // Maximize sum of slacks.
    model.Maximize(slacks.Sum());

    // Solves and prints out the solution.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);
    Console.WriteLine(String.Format("Solve status: {0}", status));
    if (status == CpSolverStatus.Optimal) {
      Console.WriteLine(String.Format("Optimal objective value: {0}",
                                      solver.ObjectiveValue));
      for (int b = 0; b < num_bins; ++b)
      {
        Console.WriteLine(String.Format("load_{0} = {1}",
                                        b, solver.Value(load[b])));
        for (int i = 0; i < num_items; ++i)
        {
          Console.WriteLine(string.Format("  item_{0}_{1} = {2}",
                                          i, b, solver.Value(x[i, b])));
        }
      }
    }
    Console.WriteLine("Statistics");
    Console.WriteLine(
        String.Format("  - conflicts : {0}", solver.NumConflicts()));
    Console.WriteLine(
        String.Format("  - branches  : {0}", solver.NumBranches()));
    Console.WriteLine(
        String.Format("  - wall time : {0} s", solver.WallTime()));
  }

  static void IntervalSample()
  {
    CpModel model = new CpModel();
    int horizon = 100;
    IntVar start_var = model.NewIntVar(0, horizon, "start");
    // C# code supports IntVar or integer constants in intervals.
    int duration = 10;
    IntVar end_var = model.NewIntVar(0, horizon, "end");
    IntervalVar interval =
        model.NewIntervalVar(start_var, duration, end_var, "interval");
  }

  static void OptionalIntervalSample()
  {
    CpModel model = new CpModel();
    int horizon = 100;
    IntVar start_var = model.NewIntVar(0, horizon, "start");
    // C# code supports IntVar or integer constants in intervals.
    int duration = 10;
    IntVar end_var = model.NewIntVar(0, horizon, "end");
    IntVar presence_var = model.NewBoolVar("presence");
    IntervalVar interval = model.NewOptionalIntervalVar(
        start_var, duration, end_var, presence_var, "interval");
  }
  
  static void MinimalCpSat()
  {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    int num_vals = 3;

    IntVar x = model.NewIntVar(0, num_vals - 1, "x");
    IntVar y = model.NewIntVar(0, num_vals - 1, "y");
    IntVar z = model.NewIntVar(0, num_vals - 1, "z");
    // Creates the constraints.
    model.Add(x != y);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.Solve(model);

    if (status == CpSolverStatus.ModelSat)
    {
      Console.WriteLine("x = " + solver.Value(x));
      Console.WriteLine("y = " + solver.Value(y));
      Console.WriteLine("z = " + solver.Value(z));
    }
  }

  static void MinimalCpSatWithTimeLimit()
  {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    int num_vals = 3;

    IntVar x = model.NewIntVar(0, num_vals - 1, "x");
    IntVar y = model.NewIntVar(0, num_vals - 1, "y");
    IntVar z = model.NewIntVar(0, num_vals - 1, "z");
    // Creates the constraints.
    model.Add(x != y);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();

    // Adds a time limit. Parameters are stored as strings in the solver.
    solver.StringParameters = "max_time_in_seconds:10.0" ;

    CpSolverStatus status = solver.Solve(model);

    if (status == CpSolverStatus.ModelSat)
    {
      Console.WriteLine("x = " + solver.Value(x));
      Console.WriteLine("y = " + solver.Value(y));
      Console.WriteLine("z = " + solver.Value(z));
    }
  }

  static void MinimalCpSatPrintIntermediateSolutions()
  {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    int num_vals = 3;

    IntVar x = model.NewIntVar(0, num_vals - 1, "x");
    IntVar y = model.NewIntVar(0, num_vals - 1, "y");
    IntVar z = model.NewIntVar(0, num_vals - 1, "z");
    // Creates the constraints.
    model.Add(x != y);
    // Create the objective.
    model.Maximize(x + 2 * y + 3 * z);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinterWithObjective cb =
        new VarArraySolutionPrinterWithObjective(new IntVar[] {x, y, z});
    solver.SearchAllSolutions(model, cb);
    Console.WriteLine(String.Format("Number of solutions found: {0}",
                                    cb.SolutionCount()));
  }

  static void MinimalCpSatAllSolutions()
  {
    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    int num_vals = 3;

    IntVar x = model.NewIntVar(0, num_vals - 1, "x");
    IntVar y = model.NewIntVar(0, num_vals - 1, "y");
    IntVar z = model.NewIntVar(0, num_vals - 1, "z");
    // Creates the constraints.
    model.Add(x != y);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    VarArraySolutionPrinter cb =
        new VarArraySolutionPrinter(new IntVar[] {x, y, z});
    solver.SearchAllSolutions(model, cb);
    Console.WriteLine(String.Format("Number of solutions found: {0}",
                                    cb.SolutionCount()));
  }

  static void Main()
  {
    Console.WriteLine("--- CodeSample ---");
    CodeSample();
    Console.WriteLine("--- LiteralSample ---");
    LiteralSample();
    Console.WriteLine("--- BoolOrSample ---");
    BoolOrSample();
    Console.WriteLine("--- ReifiedSample ---");
    ReifiedSample();
    Console.WriteLine("--- RabbitsAndPheasants ---");
    RabbitsAndPheasants();
    Console.WriteLine("--- BinpackingProblem ---");
    BinpackingProblem();
    Console.WriteLine("--- IntervalSample ---");
    IntervalSample();
    Console.WriteLine("--- OptionalIntervalSample ---");
    OptionalIntervalSample();
    Console.WriteLine("--- MinimalCpSat ---");
    MinimalCpSat();
    Console.WriteLine("--- MinimalCpSatWithTimeLimit ---");
    MinimalCpSatWithTimeLimit();
    Console.WriteLine("--- MinimalCpSatPrintIntermediateSolutions ---");
    MinimalCpSatPrintIntermediateSolutions();
    Console.WriteLine("--- MinimalCpSatAllSolutions ---");
    MinimalCpSatAllSolutions();
  }
}
