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
using Google.OrTools.Sat;
// [END import]

public class AssignmentSat
{
    public static void Main(String[] args)
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
        CpModel model = new CpModel();
        // [END model]

        // Variables.
        // [START variables]
        BoolVar[,] x = new BoolVar[numWorkers, numTasks];
        // Variables in a 1-dim array.
        for (int worker = 0; worker < numWorkers; ++worker)
        {
            for (int task = 0; task < numTasks; ++task)
            {
                x[worker, task] = model.NewBoolVar($"worker_{worker}_task_{task}");
            }
        }
        // [END variables]

        // Constraints
        // [START constraints]
        // Each worker is assigned to at most one task.
        for (int worker = 0; worker < numWorkers; ++worker)
        {
            List<ILiteral> tasks = new List<ILiteral>();
            for (int task = 0; task < numTasks; ++task)
            {
                tasks.Add(x[worker, task]);
            }
            model.AddAtMostOne(tasks);
        }

        // Each task is assigned to exactly one worker.
        for (int task = 0; task < numTasks; ++task)
        {
            List<ILiteral> workers = new List<ILiteral>();
            for (int worker = 0; worker < numWorkers; ++worker)
            {
                workers.Add(x[worker, task]);
            }
            model.AddExactlyOne(workers);
        }
        // [END constraints]

        // Objective
        // [START objective]
        LinearExprBuilder obj = LinearExpr.NewBuilder();
        for (int worker = 0; worker < numWorkers; ++worker)
        {
            for (int task = 0; task < numTasks; ++task)
            {
                obj.AddTerm((IntVar)x[worker, task], costs[worker, task]);
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
            for (int i = 0; i < numWorkers; ++i)
            {
                for (int j = 0; j < numTasks; ++j)
                {
                    if (solver.Value(x[i, j]) > 0.5)
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

        Console.WriteLine("Statistics");
        Console.WriteLine($"  - conflicts : {solver.NumConflicts()}");
        Console.WriteLine($"  - branches  : {solver.NumBranches()}");
        Console.WriteLine($"  - wall time : {solver.WallTime()}s");
    }
}
// [END program]
