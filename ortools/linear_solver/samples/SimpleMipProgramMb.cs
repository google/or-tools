// Copyright 2010-2025 Google LLC
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

// Minimal example to solve a MIP problem.
// [START program]
// [START import]
using System;
using Google.OrTools.ModelBuilder;
// [END import]

public class SimpleMipProgramMb
{
    static void Main()
    {
        // [START model]
        // Create the model builder.
        Model model = new Model();
        // [END model]

        // [START variables]
        // Create the variables x and y.
        Variable x = model.NewIntVar(0.0, double.PositiveInfinity, "x");
        Variable y = model.NewIntVar(0.0, double.PositiveInfinity, "y");

        Console.WriteLine("Number of variables = " + model.VariablesCount());
        // [END variables]

        // [START constraints]
        // x + 7 * y <= 17.5.
        model.Add(x + 7 * y <= 17.5);

        // x <= 3.5.
        model.Add(x <= 3.5);

        Console.WriteLine("Number of constraints = " + model.ConstraintsCount());
        // [END constraints]

        // [START objective]
        // Maximize x + 10 * y.
        model.Maximize(x + 10 * y);
        // [END objective]

        // [START solver]
        // Create the model solver with the SCIP backend.
        Solver solver = new Solver("SCIP");
        if (!solver.SolverIsSupported())
        {
            return;
        }
        // [END solver]

        // [START solve]
        var resultStatus = solver.Solve(model);
        // [END solve]

        // [START print_solution]
        // Check that the problem has an optimal solution.
        if (resultStatus != SolveStatus.OPTIMAL)
        {
            Console.WriteLine("The problem does not have an optimal solution!");
            return;
        }
        Console.WriteLine("Solution:");
        Console.WriteLine("Objective value = " + solver.ObjectiveValue);
        Console.WriteLine("x = " + solver.Value(x));
        Console.WriteLine("y = " + solver.Value(y));
        // [END print_solution]

        // [START advanced]
        Console.WriteLine("\nAdvanced usage:");
        Console.WriteLine("Problem solved in " + solver.WallTime + " milliseconds");
        // [END advanced]
    }
}
// [END program]
