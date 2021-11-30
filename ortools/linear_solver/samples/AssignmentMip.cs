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

public class AssignmentMip
{
    static void Main()
    {
        // Data.
        // [START data_model]
        int[,] costs = {
            { 90, 80, 75, 70 }, { 35, 85, 55, 65 }, { 125, 95, 90, 95 }, { 45, 110, 95, 115 }, { 50, 100, 90, 100 },
        };
        int numWorkers = costs.GetLength(0);
        int numTasks = costs.GetLength(1);
        // [END data_model]

        // Model.
        // [START model]
        Solver solver = Solver.CreateSolver("SCIP");
        // [END model]

        // Variables.
        // [START variables]
        // x[i, j] is an array of 0-1 variables, which will be 1
        // if worker i is assigned to task j.
        Variable[,] x = new Variable[numWorkers, numTasks];
        for (int i = 0; i < numWorkers; ++i)
        {
            for (int j = 0; j < numTasks; ++j)
            {
                x[i, j] = solver.MakeIntVar(0, 1, $"worker_{i}_task_{j}");
            }
        }
        // [END variables]

        // Constraints
        // [START constraints]
        // Each worker is assigned to at most one task.
        for (int i = 0; i < numWorkers; ++i)
        {
            Constraint constraint = solver.MakeConstraint(0, 1, "");
            for (int j = 0; j < numTasks; ++j)
            {
                constraint.SetCoefficient(x[i, j], 1);
            }
        }
        // Each task is assigned to exactly one worker.
        for (int j = 0; j < numTasks; ++j)
        {
            Constraint constraint = solver.MakeConstraint(1, 1, "");
            for (int i = 0; i < numWorkers; ++i)
            {
                constraint.SetCoefficient(x[i, j], 1);
            }
        }
        // [END constraints]

        // Objective
        // [START objective]
        Objective objective = solver.Objective();
        for (int i = 0; i < numWorkers; ++i)
        {
            for (int j = 0; j < numTasks; ++j)
            {
                objective.SetCoefficient(x[i, j], costs[i, j]);
            }
        }
        objective.SetMinimization();
        // [END objective]

        // Solve
        // [START solve]
        Solver.ResultStatus resultStatus = solver.Solve();
        // [END solve]

        // Print solution.
        // [START print_solution]
        // Check that the problem has a feasible solution.
        if (resultStatus == Solver.ResultStatus.OPTIMAL || resultStatus == Solver.ResultStatus.FEASIBLE)
        {
            Console.WriteLine($"Total cost: {solver.Objective().Value()}\n");
            for (int i = 0; i < numWorkers; ++i)
            {
                for (int j = 0; j < numTasks; ++j)
                {
                    // Test if x[i, j] is 0 or 1 (with tolerance for floating point
                    // arithmetic).
                    if (x[i, j].SolutionValue() > 0.5)
                    {
                        Console.WriteLine($"Worker {i} assigned to task {j}. Cost: {costs[i, j]}");
                    }
                }
            }
        }
        else
        {
            Console.WriteLine("No solution found.");
        }
        // [END print_solution]
    }
}
// [END program]
