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

public class AssignmentTeamsMip
{
    static void Main()
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

        // Each team takes at most two tasks.
        Constraint team1Tasks = solver.MakeConstraint(0, teamMax, "");
        foreach (int worker in team1)
        {
            foreach (int task in allTasks)
            {
                team1Tasks.SetCoefficient(x[worker, task], 1);
            }
        }

        Constraint team2Tasks = solver.MakeConstraint(0, teamMax, "");
        foreach (int worker in team2)
        {
            foreach (int task in allTasks)
            {
                team2Tasks.SetCoefficient(x[worker, task], 1);
            }
        }
        // [END constraints]

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
