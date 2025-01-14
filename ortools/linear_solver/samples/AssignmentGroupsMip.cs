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
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.LinearSolver;
// [END import]

public class AssignmentGroupsMip
{
    static void Main()
    {
        // Data.
        // [START data]
        int[,] costs = {
            { 90, 76, 75, 70, 50, 74 },    { 35, 85, 55, 65, 48, 101 }, { 125, 95, 90, 105, 59, 120 },
            { 45, 110, 95, 115, 104, 83 }, { 60, 105, 80, 75, 59, 62 }, { 45, 65, 110, 95, 47, 31 },
            { 38, 51, 107, 41, 69, 99 },   { 47, 85, 57, 71, 92, 77 },  { 39, 63, 97, 49, 118, 56 },
            { 47, 101, 71, 60, 88, 109 },  { 17, 39, 103, 64, 61, 92 }, { 101, 45, 83, 59, 92, 27 },
        };
        int numWorkers = costs.GetLength(0);
        int numTasks = costs.GetLength(1);

        int[] allWorkers = Enumerable.Range(0, numWorkers).ToArray();
        int[] allTasks = Enumerable.Range(0, numTasks).ToArray();
        // [END data]

        // Allowed groups of workers:
        // [START allowed_groups]
        int[,] group1 = {
            // group of worker 0-3
            { 2, 3 }, { 1, 3 }, { 1, 2 }, { 0, 1 }, { 0, 2 },
        };

        int[,] group2 = {
            // group of worker 4-7
            { 6, 7 }, { 5, 7 }, { 5, 6 }, { 4, 5 }, { 4, 7 },
        };

        int[,] group3 = {
            // group of worker 8-11
            { 10, 11 }, { 9, 11 }, { 9, 10 }, { 8, 10 }, { 8, 11 },
        };
        // [END allowed_groups]

        // Solver.
        // [START solver]
        Solver solver = Solver.CreateSolver("SCIP");
        if (solver is null)
        {
            return;
        }
        // [END solver]

        // Variables.
        // [START variables]
        // x[i, j] is an array of 0-1 variables, which will be 1
        // if worker i is assigned to task j.
        Variable[,] x = new Variable[numWorkers, numTasks];
        foreach (int worker in allWorkers)
        {
            foreach (int task in allTasks)
            {
                x[worker, task] = solver.MakeBoolVar($"x[{worker},{task}]");
            }
        }
        // [END variables]

        // Constraints
        // [START constraints]
        // Each worker is assigned to at most one task.
        foreach (int worker in allWorkers)
        {
            Constraint constraint = solver.MakeConstraint(0, 1, "");
            foreach (int task in allTasks)
            {
                constraint.SetCoefficient(x[worker, task], 1);
            }
        }
        // Each task is assigned to exactly one worker.
        foreach (int task in allTasks)
        {
            Constraint constraint = solver.MakeConstraint(1, 1, "");
            foreach (int worker in allWorkers)
            {
                constraint.SetCoefficient(x[worker, task], 1);
            }
        }
        // [END constraints]

        // [START assignments]
        // Create variables for each worker, indicating whether they work on some task.
        Variable[] work = new Variable[numWorkers];
        foreach (int worker in allWorkers)
        {
            work[worker] = solver.MakeBoolVar($"work[{worker}]");
        }

        foreach (int worker in allWorkers)
        {
            Variable[] vars = new Variable[numTasks];
            foreach (int task in allTasks)
            {
                vars[task] = x[worker, task];
            }
            solver.Add(work[worker] == LinearExprArrayHelper.Sum(vars));
        }

        // Group1
        Constraint constraint_g1 = solver.MakeConstraint(1, 1, "");
        for (int i = 0; i < group1.GetLength(0); ++i)
        {
            // a*b can be transformed into 0 <= a + b - 2*p <= 1 with p in [0,1]
            // p is True if a AND b, False otherwise
            Constraint constraint = solver.MakeConstraint(0, 1, "");
            constraint.SetCoefficient(work[group1[i, 0]], 1);
            constraint.SetCoefficient(work[group1[i, 1]], 1);
            Variable p = solver.MakeBoolVar($"g1_p{i}");
            constraint.SetCoefficient(p, -2);

            constraint_g1.SetCoefficient(p, 1);
        }
        // Group2
        Constraint constraint_g2 = solver.MakeConstraint(1, 1, "");
        for (int i = 0; i < group2.GetLength(0); ++i)
        {
            // a*b can be transformed into 0 <= a + b - 2*p <= 1 with p in [0,1]
            // p is True if a AND b, False otherwise
            Constraint constraint = solver.MakeConstraint(0, 1, "");
            constraint.SetCoefficient(work[group2[i, 0]], 1);
            constraint.SetCoefficient(work[group2[i, 1]], 1);
            Variable p = solver.MakeBoolVar($"g2_p{i}");
            constraint.SetCoefficient(p, -2);

            constraint_g2.SetCoefficient(p, 1);
        }
        // Group3
        Constraint constraint_g3 = solver.MakeConstraint(1, 1, "");
        for (int i = 0; i < group3.GetLength(0); ++i)
        {
            // a*b can be transformed into 0 <= a + b - 2*p <= 1 with p in [0,1]
            // p is True if a AND b, False otherwise
            Constraint constraint = solver.MakeConstraint(0, 1, "");
            constraint.SetCoefficient(work[group3[i, 0]], 1);
            constraint.SetCoefficient(work[group3[i, 1]], 1);
            Variable p = solver.MakeBoolVar($"g3_p{i}");
            constraint.SetCoefficient(p, -2);

            constraint_g3.SetCoefficient(p, 1);
        }
        // [END assignments]

        // Objective
        // [START objective]
        Objective objective = solver.Objective();
        foreach (int worker in allWorkers)
        {
            foreach (int task in allTasks)
            {
                objective.SetCoefficient(x[worker, task], costs[worker, task]);
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
            foreach (int worker in allWorkers)
            {
                foreach (int task in allTasks)
                {
                    // Test if x[i, j] is 0 or 1 (with tolerance for floating point
                    // arithmetic).
                    if (x[worker, task].SolutionValue() > 0.5)
                    {
                        Console.WriteLine($"Worker {worker} assigned to task {task}. Cost: {costs[worker, task]}");
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
