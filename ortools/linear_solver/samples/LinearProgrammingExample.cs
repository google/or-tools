// Copyright 2010-2021 Google LLC
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
// [START import]
using System;
using Google.OrTools.LinearSolver;
// [END import]

public class LinearProgrammingExample
{
    static void Main()
    {
        // [START solver]
        Solver solver = Solver.CreateSolver("GLOP");
        // [END solver]
        // x and y are continuous non-negative variables.
        // [START variables]
        Variable x = solver.MakeNumVar(0.0, double.PositiveInfinity, "x");
        Variable y = solver.MakeNumVar(0.0, double.PositiveInfinity, "y");

        Console.WriteLine("Number of variables = " + solver.NumVariables());
        // [END variables]

        // [START constraints]
        // x + 2y <= 14.
        solver.Add(x + 2 * y <= 14.0);

        // 3x - y >= 0.
        solver.Add(3 * x - y >= 0.0);

        // x - y <= 2.
        solver.Add(x - y <= 2.0);

        Console.WriteLine("Number of constraints = " + solver.NumConstraints());
        // [END constraints]

        // [START objective]
        // Objective function: 3x + 4y.
        solver.Maximize(3 * x + 4 * y);
        // [END objective]

        // [START solve]
        Solver.ResultStatus resultStatus = solver.Solve();
        // [END solve]

        // [START print_solution]
        // Check that the problem has an optimal solution.
        if (resultStatus != Solver.ResultStatus.OPTIMAL)
        {
            Console.WriteLine("The problem does not have an optimal solution!");
            return;
        }
        Console.WriteLine("Solution:");
        Console.WriteLine("Objective value = " + solver.Objective().Value());
        Console.WriteLine("x = " + x.SolutionValue());
        Console.WriteLine("y = " + y.SolutionValue());
        // [END print_solution]

        // [START advanced]
        Console.WriteLine("\nAdvanced usage:");
        Console.WriteLine("Problem solved in " + solver.WallTime() + " milliseconds");
        Console.WriteLine("Problem solved in " + solver.Iterations() + " iterations");
        // [END advanced]
    }
}
// [END program]
