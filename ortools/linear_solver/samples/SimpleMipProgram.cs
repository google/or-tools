// Copyright 2010-2022 Google LLC
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

public class SimpleMipProgram
{
    static void Main()
    {
        // [START solver]
        // Create the linear solver with the SCIP backend.
        Solver solver = Solver.CreateSolver("SCIP");
        if (solver is null)
        {
            return;
        }
        // [END solver]

        // [START variables]
        // x and y are integer non-negative variables.
        Variable x = solver.MakeIntVar(0.0, double.PositiveInfinity, "x");
        Variable y = solver.MakeIntVar(0.0, double.PositiveInfinity, "y");

        Console.WriteLine("Number of variables = " + solver.NumVariables());
        // [END variables]

        // [START constraints]
        // x + 7 * y <= 17.5.
        solver.Add(x + 7 * y <= 17.5);

        // x <= 3.5.
        solver.Add(x <= 3.5);

        Console.WriteLine("Number of constraints = " + solver.NumConstraints());
        // [END constraints]

        // [START objective]
        // Maximize x + 10 * y.
        solver.Maximize(x + 10 * y);
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
        Console.WriteLine("Problem solved in " + solver.Nodes() + " branch-and-bound nodes");
        // [END advanced]
    }
}
// [END program]
