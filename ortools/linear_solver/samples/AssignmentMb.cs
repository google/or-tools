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

// [START program]
// [START import]
using System;
using Google.OrTools.ModelBuilder;
// [END import]

public class AssignmentMb
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

        // [START model]
        Model model = new Model();
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
                x[i, j] = model.NewBoolVar($"worker_{i}_task_{j}");
            }
        }
        // [END variables]

        // Constraints
        // [START constraints]
        // Each worker is assigned to at most one task.
        for (int i = 0; i < numWorkers; ++i)
        {
            var assignedWork = LinearExpr.NewBuilder();
            for (int j = 0; j < numTasks; ++j)
            {
                assignedWork.Add(x[i, j]);
            }
            model.Add(assignedWork <= 1);
        }

        // Each task is assigned to exactly one worker.
        for (int j = 0; j < numTasks; ++j)
        {
            var assignedWorker = LinearExpr.NewBuilder();
            for (int i = 0; i < numWorkers; ++i)
            {
                assignedWorker.Add(x[i, j]);
            }
            model.Add(assignedWorker == 1);
        }
        // [END constraints]

        // Objective
        // [START objective]
        var objective = LinearExpr.NewBuilder();
        for (int i = 0; i < numWorkers; ++i)
        {
            for (int j = 0; j < numTasks; ++j)
            {
                objective.AddTerm(x[i, j], costs[i, j]);
            }
        }
        model.Minimize(objective);
        // [END objective]

        // [START solver]
        // Create the solver with the SCIP backend and check it is supported.
        Solver solver = new Solver("SCIP");
        if (!solver.SolverIsSupported())
            return;
        // [END solver]

        // Solve
        // [START solve]
        SolveStatus resultStatus = solver.Solve(model);
        // [END solve]

        // Print solution.
        // [START print_solution]
        // Check that the problem has a feasible solution.
        if (resultStatus == SolveStatus.OPTIMAL || resultStatus == SolveStatus.FEASIBLE)
        {
            Console.WriteLine($"Total cost: {solver.ObjectiveValue}\n");
            for (int i = 0; i < numWorkers; ++i)
            {
                for (int j = 0; j < numTasks; ++j)
                {
                    // Test if x[i, j] is 0 or 1 (with tolerance for floating point
                    // arithmetic).
                    if (solver.Value(x[i, j]) > 0.9)
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
