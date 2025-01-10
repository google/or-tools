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

// Minimal example to call the GLOP solver.
// [START program]
// [START import]
using System;
using Google.OrTools.Init;
using Google.OrTools.LinearSolver;
// [END import]

public class BasicExample
{
    static void Main()
    {
        Console.WriteLine("Google.OrTools version: " + OrToolsVersion.VersionString());

        // [START solver]
        // Create the linear solver with the GLOP backend.
        Solver solver = Solver.CreateSolver("GLOP");
        if (solver is null)
        {
            Console.WriteLine("Could not create solver GLOP");
            return;
        }
        // [END solver]

        // [START variables]
        // Create the variables x and y.
        Variable x = solver.MakeNumVar(0.0, 1.0, "x");
        Variable y = solver.MakeNumVar(0.0, 2.0, "y");

        Console.WriteLine("Number of variables = " + solver.NumVariables());
        // [END variables]

        // [START constraints]
        // Create a linear constraint, x + y <= 2.
        Constraint constraint = solver.MakeConstraint(double.NegativeInfinity, 2.0, "constraint");
        constraint.SetCoefficient(x, 1);
        constraint.SetCoefficient(y, 1);

        Console.WriteLine("Number of constraints = " + solver.NumConstraints());
        // [END constraints]

        // [START objective]
        // Create the objective function, 3 * x + y.
        Objective objective = solver.Objective();
        objective.SetCoefficient(x, 3);
        objective.SetCoefficient(y, 1);
        objective.SetMaximization();
        // [END objective]

        // [START solve]
        Console.WriteLine("Solving with " + solver.SolverVersion());
        Solver.ResultStatus resultStatus = solver.Solve();
        // [END solve]

        // [START print_solution]
        Console.WriteLine("Status: " + resultStatus);
        if (resultStatus != Solver.ResultStatus.OPTIMAL)
        {
            Console.WriteLine("The problem does not have an optimal solution!");
            if (resultStatus == Solver.ResultStatus.FEASIBLE)
            {
                Console.WriteLine("A potentially suboptimal solution was found");
            }
            else
            {
                Console.WriteLine("The solver could not solve the problem.");
                return;
            }
        }

        Console.WriteLine("Solution:");
        Console.WriteLine("Objective value = " + solver.Objective().Value());
        Console.WriteLine("x = " + x.SolutionValue());
        Console.WriteLine("y = " + y.SolutionValue());
        // [END print_solution]

        // [START advanced]
        Console.WriteLine("Advanced usage:");
        Console.WriteLine("Problem solved in " + solver.WallTime() + " milliseconds");
        Console.WriteLine("Problem solved in " + solver.Iterations() + " iterations");
        // [END advanced]
    }
}
// [END program]
