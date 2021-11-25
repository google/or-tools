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
// [START import]

public class MultipleKnapsackSat
{
    public static void Main(String[] args)
    {
        // [START data]
        long[] Weights = { 48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36 };
        long[] Values = { 10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25 };
        int NumItems = Weights.Length;
        int[] allItems = Enumerable.Range(0, NumItems).ToArray();
        long[] BinCapacities = { 100, 100, 100, 100, 100 };
        int NumBins = BinCapacities.Length;
        int[] allBins = Enumerable.Range(0, NumBins).ToArray();
        // [END data]

        // Model.
        // [START model]
        CpModel model = new CpModel();
        // [END model]

        // Variables
        // [START variables]
        IntVar[,] x = new IntVar[NumItems, NumBins];
        for (int i = 0; i < NumItems; ++i)
        {
            for (int j = 0; j < NumBins; ++j)
            {
                x[i, j] = model.NewBoolVar($"x_{i}_{j}");
            }
        }
        // [END variables]

        // Constraints
        // [START constraints]
        // Each item is assigned to at most one bin.
        for (int i = 0; i < NumItems; ++i)
        {
            IntVar[] vars = new IntVar[NumBins];
            for (int b = 0; b < NumBins; ++b)
            {
                vars[b] = x[i, b];
            }
            model.Add(LinearExpr.Sum(vars) <= 1);
        }

        // The amount packed in each bin cannot exceed its capacity.
        for (int b = 0; b < NumBins; ++b)
        {
            LinearExpr[] exprs = new LinearExpr[NumItems];
            for (int i = 0; i < NumItems; ++i)
            {
                exprs[i] = LinearExpr.Affine(x[i, b], /*coeff=*/Weights[i], /*offset=*/0);
            }
            model.Add(LinearExpr.Sum(exprs) <= 1);
        }
        // [END constraints]

        // Objective
        // [START objective]
        LinearExpr[] obj = new LinearExpr[NumItems * NumBins];
        for (int i = 0; i < NumItems; ++i)
        {
            for (int b = 0; b < NumBins; ++b)
            {
                int k = i * NumBins + b;
                obj[i] = LinearExpr.Affine(x[i, b], /*coeff=*/Values[i], /*offset=*/0);
            }
        }
        model.Maximize(LinearExpr.Sum(obj));
        //  [END objective]

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
            Console.WriteLine($"Total packed value: {solver.ObjectiveValue}");
            double TotalWeight = 0.0;
            for (int b = 0; b < NumBins; ++b)
            {
                double BinWeight = 0.0;
                double BinValue = 0.0;
                Console.WriteLine($"Bin {b}");
                for (int i = 0; i < NumItems; ++i)
                {
                    if (solver.Value(x[i, b]) == 1)
                    {
                        Console.WriteLine($"Item {i} weight: {Weights[i]} values: {Values[i]}");
                        BinWeight += Weights[i];
                        BinValue += Values[i];
                    }
                }
                Console.WriteLine("Packed bin weight: " + BinWeight);
                Console.WriteLine("Packed bin value: " + BinValue);
                TotalWeight += BinWeight;
            }
            Console.WriteLine("Total packed weight: " + TotalWeight);
        }
        else
        {
            Console.WriteLine("No solution found.");
        }
        // [END print_solution]

        // [START statistics]
        Console.WriteLine("Statistics");
        Console.WriteLine($"  conflicts: {solver.NumConflicts()}");
        Console.WriteLine($"  branches : {solver.NumBranches()}");
        Console.WriteLine($"  wall time: {solver.WallTime()}s");
        // [END statistics]
    }
}

// [END program]
