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
    CodeSample();
    MinimalCpSat();
    MinimalCpSatWithTimeLimit();
    MinimalCpSatPrintIntermediateSolutions();
    MinimalCpSatAllSolutions();
  }
}
