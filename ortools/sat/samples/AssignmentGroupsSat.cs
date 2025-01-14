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
using Google.OrTools.Sat;
// [END import]

public class AssignmentGroupsSat
{
    public static void Main(String[] args)
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
        long[,] group1 = {
            { 0, 0, 1, 1 }, // Workers 2, 3
            { 0, 1, 0, 1 }, // Workers 1, 3
            { 0, 1, 1, 0 }, // Workers 1, 2
            { 1, 1, 0, 0 }, // Workers 0, 1
            { 1, 0, 1, 0 }, // Workers 0, 2
        };

        long[,] group2 = {
            { 0, 0, 1, 1 }, // Workers 6, 7
            { 0, 1, 0, 1 }, // Workers 5, 7
            { 0, 1, 1, 0 }, // Workers 5, 6
            { 1, 1, 0, 0 }, // Workers 4, 5
            { 1, 0, 0, 1 }, // Workers 4, 7
        };

        long[,] group3 = {
            { 0, 0, 1, 1 }, // Workers 10, 11
            { 0, 1, 0, 1 }, // Workers 9, 11
            { 0, 1, 1, 0 }, // Workers 9, 10
            { 1, 0, 1, 0 }, // Workers 8, 10
            { 1, 0, 0, 1 }, // Workers 8, 11
        };
        // [END allowed_groups]

        // Model.
        // [START model]
        CpModel model = new CpModel();
        // [END model]

        // Variables.
        // [START variables]
        BoolVar[,] x = new BoolVar[numWorkers, numTasks];
        // Variables in a 1-dim array.
        foreach (int worker in allWorkers)
        {
            foreach (int task in allTasks)
            {
                x[worker, task] = model.NewBoolVar($"x[{worker},{task}]");
            }
        }
        // [END variables]

        // Constraints
        // [START constraints]
        // Each worker is assigned to at most one task.
        foreach (int worker in allWorkers)
        {
            List<ILiteral> tasks = new List<ILiteral>();
            foreach (int task in allTasks)
            {
                tasks.Add(x[worker, task]);
            }
            model.AddAtMostOne(tasks);
        }

        // Each task is assigned to exactly one worker.
        foreach (int task in allTasks)
        {
            List<ILiteral> workers = new List<ILiteral>();
            foreach (int worker in allWorkers)
            {
                workers.Add(x[worker, task]);
            }
            model.AddExactlyOne(workers);
        }
        // [END constraints]

        // [START assignments]
        // Create variables for each worker, indicating whether they work on some task.
        BoolVar[] work = new BoolVar[numWorkers];
        foreach (int worker in allWorkers)
        {
            work[worker] = model.NewBoolVar($"work[{worker}]");
        }

        foreach (int worker in allWorkers)
        {
            List<ILiteral> tasks = new List<ILiteral>();
            foreach (int task in allTasks)
            {
                tasks.Add(x[worker, task]);
            }
            model.Add(work[worker] == LinearExpr.Sum(tasks));
        }

        // Define the allowed groups of worders
        model.AddAllowedAssignments(new IntVar[] { work[0], work[1], work[2], work[3] }).AddTuples(group1);
        model.AddAllowedAssignments(new IntVar[] { work[4], work[5], work[6], work[7] }).AddTuples(group2);
        model.AddAllowedAssignments(new IntVar[] { work[8], work[9], work[10], work[11] }).AddTuples(group3);
        // [END assignments]

        // Objective
        // [START objective]
        LinearExprBuilder obj = LinearExpr.NewBuilder();
        foreach (int worker in allWorkers)
        {
            foreach (int task in allTasks)
            {
                obj.AddTerm(x[worker, task], costs[worker, task]);
            }
        }
        model.Minimize(obj);
        // [END objective]

        // Solve
        // [START solve]
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Console.WriteLine($"Solve status: {status}");
        // [END solve]

        // Print solution.
        // [START print_solution]
        // Check that the problem has a feasible solution.
        if (status == CpSolverStatus.Optimal || status == CpSolverStatus.Feasible)
        {
            Console.WriteLine($"Total cost: {solver.ObjectiveValue}\n");
            foreach (int worker in allWorkers)
            {
                foreach (int task in allTasks)
                {
                    if (solver.Value(x[worker, task]) > 0.5)
                    {
                        Console.WriteLine($"Worker {worker} assigned to task {task}. " +
                                          $"Cost: {costs[worker, task]}");
                    }
                }
            }
        }
        else
        {
            Console.WriteLine("No solution found.");
        }
        // [END print_solution]

        Console.WriteLine("Statistics");
        Console.WriteLine($"  - conflicts : {solver.NumConflicts()}");
        Console.WriteLine($"  - branches  : {solver.NumBranches()}");
        Console.WriteLine($"  - wall time : {solver.WallTime()}s");
    }
}
// [END program]
