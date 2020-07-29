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

// [START program]
using System;
using Google.OrTools.LinearSolver;

public class LinearProgrammingExample
{
  static void Main()
  {
      // [START solver]
      Solver solver = Solver.CreateSolver("LinearProgrammingExample", "GLOP");
      // [END solver]
      // x and y are continuous non-negative variables.
      // [START variables]
      Variable x = solver.MakeNumVar(0.0, double.PositiveInfinity, "x");
      Variable y = solver.MakeNumVar(0.0, double.PositiveInfinity, "y");
      // [END variables]

      // [START constraints]
      // x + 2y <= 14.
      Constraint c0 = solver.MakeConstraint(double.NegativeInfinity, 14.0);
      c0.SetCoefficient(x, 1);
      c0.SetCoefficient(y, 2);

      // 3x - y >= 0.
      Constraint c1 = solver.MakeConstraint(0.0, double.PositiveInfinity);
      c1.SetCoefficient(x, 3);
      c1.SetCoefficient(y, -1);

      // x - y <= 2.
      Constraint c2 = solver.MakeConstraint(double.NegativeInfinity, 2.0);
      c2.SetCoefficient(x, 1);
      c2.SetCoefficient(y, -1);
      // [END constraints]

      // [START objective]
      // Objective function: 3x + 4y.
      Objective objective = solver.Objective();
      objective.SetCoefficient(x, 3);
      objective.SetCoefficient(y, 4);
      objective.SetMaximization();
      // [END objective]

      // [START solve]
      solver.Solve();
      // [END solve]

      // [START print_solution]
      Console.WriteLine("Number of variables = " + solver.NumVariables());
      Console.WriteLine("Number of constraints = " + solver.NumConstraints());
      // The value of each variable in the solution.
      Console.WriteLine("Solution:");
      Console.WriteLine("x = " + x.SolutionValue());
      Console.WriteLine("y = " + y.SolutionValue());
      // The objective value of the solution.
      Console.WriteLine("Optimal objective value = " +
                      solver.Objective().Value());
      // [END print_solution]
    }
}
// [END program]
