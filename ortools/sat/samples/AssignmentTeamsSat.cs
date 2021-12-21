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
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.Sat;
// [END import]

public class AssignmentTeamsSat
{
    public static void Main(String[] args)
    {
        // Data.
        // [START data]
        int[,] costs = {
            { 90, 76, 75, 70 },   { 35, 85, 55, 65 },  { 125, 95, 90, 105 },
            { 45, 110, 95, 115 }, { 60, 105, 80, 75 }, { 45, 65, 110, 95 },
        };
        int numWorkers = costs.GetLength(0);
        int numTasks = costs.GetLength(1);

        int[] allWorkers = Enumerable.Range(0, numWorkers).ToArray();
        int[] allTasks = Enumerable.Range(0, numTasks).ToArray();

        int[] team1 = { 0, 2, 4 };
        int[] team2 = { 1, 3, 5 };
        // Maximum total of tasks for any team
        int teamMax = 2;
        // [END data]

        // Model.
        // [START model]
        CpModel model = new CpModel();
        // [END model]

        // Variables.
        // [START variables]
        IntVar[,] x = new IntVar[numWorkers, numTasks];
        // Variables in a 1-dim array.
        IntVar[] xFlat = new IntVar[numWorkers * numTasks];
        int[] costsFlat = new int[numWorkers * numTasks];
        foreach (int worker in allWorkers)
        {
            foreach (int task in allTasks)
            {
                x[worker, task] = model.NewBoolVar($"x[{worker},{task}]");
                int k = worker * numTasks + task;
                xFlat[k] = x[worker, task];
                costsFlat[k] = costs[worker, task];
            }
        }
        // [END variables]

        // Constraints
        // [START constraints]
        // Each worker is assigned to at most one task.
        foreach (int worker in allWorkers)
        {
            IntVar[] vars = new IntVar[numTasks];
            foreach (int task in allTasks)
            {
                vars[task] = x[worker, task];
            }
            model.Add(LinearExpr.Sum(vars) <= 1);
        }

        // Each task is assigned to exactly one worker.
        foreach (int task in allTasks)
        {
            IntVar[] vars = new IntVar[numWorkers];
            foreach (int worker in allWorkers)
            {
                vars[worker] = x[worker, task];
            }
            model.Add(LinearExpr.Sum(vars) == 1);
        }

        // Each team takes at most two tasks.
        List<IntVar> team1Tasks = new List<IntVar>();
        foreach (int worker in team1)
        {
            foreach (int task in allTasks)
            {
                team1Tasks.Add(x[worker, task]);
            }
        }
        model.Add(LinearExpr.Sum(team1Tasks.ToArray()) <= teamMax);

        List<IntVar> team2Tasks = new List<IntVar>();
        foreach (int worker in team2)
        {
            foreach (int task in allTasks)
            {
                team2Tasks.Add(x[worker, task]);
            }
        }
        model.Add(LinearExpr.Sum(team2Tasks.ToArray()) <= teamMax);
        // [END constraints]

        // Objective
        // [START objective]
        model.Minimize(LinearExpr.ScalProd(xFlat, costsFlat));
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
