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
using Google.OrTools.LinearSolver;

public class CsLinearProgramming
{
  private static void RunLinearProgrammingExample(String solverType)
  {
    Solver solver = Solver.CreateSolver("IntegerProgramming", solverType);
    if (solver == null)
    {
      Console.WriteLine("Could not create solver " + solverType);
      return;
    }
    // x1, x2 and x3 are continuous non-negative variables.
    Variable x1 = solver.MakeNumVar(0.0, double.PositiveInfinity, "x1");
    Variable x2 = solver.MakeNumVar(0.0, double.PositiveInfinity, "x2");
    Variable x3 = solver.MakeNumVar(0.0, double.PositiveInfinity, "x3");

    // Maximize 10 * x1 + 6 * x2 + 4 * x3.
    Objective objective = solver.Objective();
    objective.SetCoefficient(x1, 10);
    objective.SetCoefficient(x2, 6);
    objective.SetCoefficient(x3, 4);
    objective.SetMaximization();

    // x1 + x2 + x3 <= 100.
    Constraint c0 = solver.MakeConstraint(double.NegativeInfinity, 100.0);
    c0.SetCoefficient(x1, 1);
    c0.SetCoefficient(x2, 1);
    c0.SetCoefficient(x3, 1);

    // 10 * x1 + 4 * x2 + 5 * x3 <= 600.
    Constraint c1 = solver.MakeConstraint(double.NegativeInfinity, 600.0);
    c1.SetCoefficient(x1, 10);
    c1.SetCoefficient(x2, 4);
    c1.SetCoefficient(x3, 5);

    // 2 * x1 + 2 * x2 + 6 * x3 <= 300.
    Constraint c2 = solver.MakeConstraint(double.NegativeInfinity, 300.0);
    c2.SetCoefficient(x1, 2);
    c2.SetCoefficient(x2, 2);
    c2.SetCoefficient(x3, 6);

    Console.WriteLine("Number of variables = " + solver.NumVariables());
    Console.WriteLine("Number of constraints = " + solver.NumConstraints());

    int resultStatus = solver.Solve();

    // Check that the problem has an optimal solution.
    if (resultStatus != Solver.OPTIMAL) {
      Console.WriteLine("The problem does not have an optimal solution!");
      return;
    }

    Console.WriteLine("Problem solved in " + solver.WallTime() +
                      " milliseconds");

    // The objective value of the solution.
    Console.WriteLine("Optimal objective value = " +
                      solver.Objective().Value());

    // The value of each variable in the solution.
    Console.WriteLine("x1 = " + x1.SolutionValue());
    Console.WriteLine("x2 = " + x2.SolutionValue());
    Console.WriteLine("x3 = " + x3.SolutionValue());

    Console.WriteLine("Advanced usage:");
    double[] activities = solver.ComputeConstraintActivities();

    Console.WriteLine("Problem solved in " + solver.Iterations() +
                       " iterations");
    Console.WriteLine("x1: reduced cost = " + x1.ReducedCost());
    Console.WriteLine("x2: reduced cost = " + x2.ReducedCost());
    Console.WriteLine("x3: reduced cost = " + x3.ReducedCost());
    Console.WriteLine("c0: dual value = " + c0.DualValue());
    Console.WriteLine("    activity = " + activities[c0.Index()]);
    Console.WriteLine("c1: dual value = " + c1.DualValue());
    Console.WriteLine("    activity = " + activities[c1.Index()]);
    Console.WriteLine("c2: dual value = " + c2.DualValue());
    Console.WriteLine("    activity = " + activities[c2.Index()]);
  }

  private static void RunLinearProgrammingExampleNaturalApi(
      String solverType, bool printModel)
  {
    Solver solver = Solver.CreateSolver("IntegerProgramming", solverType);
    if (solver == null)
    {
      Console.WriteLine("Could not create solver " + solverType);
      return;
    }
    // x1, x2 and x3 are continuous non-negative variables.
    Variable x1 = solver.MakeNumVar(0.0, double.PositiveInfinity, "x1");
    Variable x2 = solver.MakeNumVar(0.0, double.PositiveInfinity, "x2");
    Variable x3 = solver.MakeNumVar(0.0, double.PositiveInfinity, "x3");

    solver.Maximize(10 * x1 + 6 * x2 + 4 * x3);
    Constraint c0 = solver.Add(x1 + x2 + x3 <= 100);
    Constraint c1 = solver.Add(10 * x1 + x2 * 4 + 5 * x3 <= 600);
    Constraint c2 = solver.Add(2 * x1 + 2 * x2 + 6 * x3 <= 300);

    Console.WriteLine("Number of variables = " + solver.NumVariables());
    Console.WriteLine("Number of constraints = " + solver.NumConstraints());

    if (printModel) {
      string model = solver.ExportModelAsLpFormat(false);
      Console.WriteLine(model);
    }

    int resultStatus = solver.Solve();

    // Check that the problem has an optimal solution.
    if (resultStatus != Solver.OPTIMAL) {
      Console.WriteLine("The problem does not have an optimal solution!");
      return;
    }

    Console.WriteLine("Problem solved in " + solver.WallTime() +
                      " milliseconds");

    // The objective value of the solution.
    Console.WriteLine("Optimal objective value = " +
                      solver.Objective().Value());

    // The value of each variable in the solution.
    Console.WriteLine("x1 = " + x1.SolutionValue());
    Console.WriteLine("x2 = " + x2.SolutionValue());
    Console.WriteLine("x3 = " + x3.SolutionValue());

    Console.WriteLine("Advanced usage:");
    double[] activities = solver.ComputeConstraintActivities();
    Console.WriteLine("Problem solved in " + solver.Iterations() +
                       " iterations");
    Console.WriteLine("x1: reduced cost = " + x1.ReducedCost());
    Console.WriteLine("x2: reduced cost = " + x2.ReducedCost());
    Console.WriteLine("x3: reduced cost = " + x3.ReducedCost());
    Console.WriteLine("c0: dual value = " + c0.DualValue());
    Console.WriteLine("    activity = " + activities[c0.Index()]);
    Console.WriteLine("c1: dual value = " + c1.DualValue());
    Console.WriteLine("    activity = " + activities[c1.Index()]);
    Console.WriteLine("c2: dual value = " + c2.DualValue());
    Console.WriteLine("    activity = " + activities[c2.Index()]);
  }

  static void Main()
  {
    Console.WriteLine("---- Linear programming example with GLOP ----");
    RunLinearProgrammingExample("GLOP_LINEAR_PROGRAMMING");
    Console.WriteLine("---- Linear programming example with GLPK ----");
    RunLinearProgrammingExample("GLPK_LINEAR_PROGRAMMING");
    Console.WriteLine("---- Linear programming example with CLP ----");
    RunLinearProgrammingExample("CLP_LINEAR_PROGRAMMING");
    Console.WriteLine(
        "---- Linear programming example (Natural API) with GLOP ----");
    RunLinearProgrammingExampleNaturalApi("GLOP_LINEAR_PROGRAMMING", true);
    Console.WriteLine(
        "---- Linear programming example (Natural API) with GLPK ----");
    RunLinearProgrammingExampleNaturalApi("GLPK_LINEAR_PROGRAMMING", false);
    Console.WriteLine(
        "---- Linear programming example (Natural API) with CLP ----");
    RunLinearProgrammingExampleNaturalApi("CLP_LINEAR_PROGRAMMING", false);
  }
}
