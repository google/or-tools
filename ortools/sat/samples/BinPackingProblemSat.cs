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

using System;
using Google.OrTools.Sat;

public class BinPackingProblemSat
{
    static void Main()
    {
        // Data.
        int bin_capacity = 100;
        int slack_capacity = 20;
        int num_bins = 5;

        int[,] items = new int[,] { { 20, 6 }, { 15, 6 }, { 30, 4 }, { 45, 3 } };
        int num_items = items.GetLength(0);

        // Model.
        CpModel model = new CpModel();

        // Main variables.
        IntVar[,] x = new IntVar[num_items, num_bins];
        for (int i = 0; i < num_items; ++i)
        {
            int num_copies = items[i, 1];
            for (int b = 0; b < num_bins; ++b)
            {
                x[i, b] = model.NewIntVar(0, num_copies, String.Format("x_{0}_{1}", i, b));
            }
        }

        // Load variables.
        IntVar[] load = new IntVar[num_bins];
        for (int b = 0; b < num_bins; ++b)
        {
            load[b] = model.NewIntVar(0, bin_capacity, String.Format("load_{0}", b));
        }

        // Slack variables.
        IntVar[] slacks = new IntVar[num_bins];
        for (int b = 0; b < num_bins; ++b)
        {
            slacks[b] = model.NewBoolVar(String.Format("slack_{0}", b));
        }

        // Links load and x.
        int[] sizes = new int[num_items];
        for (int i = 0; i < num_items; ++i)
        {
            sizes[i] = items[i, 0];
        }
        for (int b = 0; b < num_bins; ++b)
        {
            IntVar[] tmp = new IntVar[num_items];
            for (int i = 0; i < num_items; ++i)
            {
                tmp[i] = x[i, b];
            }
            model.Add(load[b] == LinearExpr.ScalProd(tmp, sizes));
        }

        // Place all items.
        for (int i = 0; i < num_items; ++i)
        {
            IntVar[] tmp = new IntVar[num_bins];
            for (int b = 0; b < num_bins; ++b)
            {
                tmp[b] = x[i, b];
            }
            model.Add(LinearExpr.Sum(tmp) == items[i, 1]);
        }

        // Links load and slack.
        int safe_capacity = bin_capacity - slack_capacity;
        for (int b = 0; b < num_bins; ++b)
        {
            //  slack[b] => load[b] <= safe_capacity.
            model.Add(load[b] <= safe_capacity).OnlyEnforceIf(slacks[b]);
            // not(slack[b]) => load[b] > safe_capacity.
            model.Add(load[b] > safe_capacity).OnlyEnforceIf(slacks[b].Not());
        }

        // Maximize sum of slacks.
        model.Maximize(LinearExpr.Sum(slacks));

        // Solves and prints out the solution.
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Console.WriteLine(String.Format("Solve status: {0}", status));
        if (status == CpSolverStatus.Optimal)
        {
            Console.WriteLine(String.Format("Optimal objective value: {0}", solver.ObjectiveValue));
            for (int b = 0; b < num_bins; ++b)
            {
                Console.WriteLine(String.Format("load_{0} = {1}", b, solver.Value(load[b])));
                for (int i = 0; i < num_items; ++i)
                {
                    Console.WriteLine(string.Format("  item_{0}_{1} = {2}", i, b, solver.Value(x[i, b])));
                }
            }
        }
        Console.WriteLine("Statistics");
        Console.WriteLine(String.Format("  - conflicts : {0}", solver.NumConflicts()));
        Console.WriteLine(String.Format("  - branches  : {0}", solver.NumBranches()));
        Console.WriteLine(String.Format("  - wall time : {0} s", solver.WallTime()));
    }
}
