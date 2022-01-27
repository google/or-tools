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
        IntVar[,] x = new IntVar[numWorkers, numTasks];
        // Variables in a 1-dim array.
        IntVar[] xFlat = new IntVar[numWorkers * numTasks];
        int[] costsFlat = new int[numWorkers * numTasks];
        for (int i = 0; i < numWorkers; ++i)
        {
            for (int j = 0; j < numTasks; ++j)
            {
                x[i, j] = model.NewIntVar(0, 1, $"worker_{i}_task_{j}");
                int k = i * numTasks + j;
                xFlat[k] = x[i, j];
                costsFlat[k] = costs[i, j];
            }
        }
        // [END variables]

        // Constraints
        // [START constraints]
        // Each worker is assigned to at most one task.
        for (int i = 0; i < numWorkers; ++i)
        {
            IntVar[] vars = new IntVar[numTasks];
            for (int j = 0; j < numTasks; ++j)
            {
                vars[j] = x[i, j];
            }
            model.Add(LinearExpr.Sum(vars) <= 1);
        }

        // Each task is assigned to exactly one worker.
        for (int j = 0; j < numTasks; ++j)
        {
            IntVar[] vars = new IntVar[numWorkers];
            for (int i = 0; i < numWorkers; ++i)
            {
                vars[i] = x[i, j];
            }
            model.Add(LinearExpr.Sum(vars) == 1);
        }
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
