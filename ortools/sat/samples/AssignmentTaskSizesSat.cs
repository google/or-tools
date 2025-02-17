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

public class AssignmentTaskSizesSat
{
    public static void Main(String[] args)
    {
        // Data.
        // [START data]
        int[,] costs = {
            { 90, 76, 75, 70, 50, 74, 12, 68 },    { 35, 85, 55, 65, 48, 101, 70, 83 },
            { 125, 95, 90, 105, 59, 120, 36, 73 }, { 45, 110, 95, 115, 104, 83, 37, 71 },
            { 60, 105, 80, 75, 59, 62, 93, 88 },   { 45, 65, 110, 95, 47, 31, 81, 34 },
            { 38, 51, 107, 41, 69, 99, 115, 48 },  { 47, 85, 57, 71, 92, 77, 109, 36 },
            { 39, 63, 97, 49, 118, 56, 92, 61 },   { 47, 101, 71, 60, 88, 109, 52, 90 },
        };
        int numWorkers = costs.GetLength(0);
        int numTasks = costs.GetLength(1);

        int[] allWorkers = Enumerable.Range(0, numWorkers).ToArray();
        int[] allTasks = Enumerable.Range(0, numTasks).ToArray();

        int[] taskSizes = { 10, 7, 3, 12, 15, 4, 11, 5 };
        // Maximum total of task sizes for any worker
        int totalSizeMax = 15;
        // [END data]

        // Model.
        // [START model]
        CpModel model = new CpModel();
        // [END model]

        // Variables.
        // [START variables]
        BoolVar[,] x = new BoolVar[numWorkers, numTasks];
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
        // Each worker is assigned to at most max task size.
        foreach (int worker in allWorkers)
        {
            BoolVar[] vars = new BoolVar[numTasks];
            foreach (int task in allTasks)
            {
                vars[task] = x[worker, task];
            }
            model.Add(LinearExpr.WeightedSum(vars, taskSizes) <= totalSizeMax);
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
