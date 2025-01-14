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
// Solves a multiple knapsack problem using the CP-SAT solver.
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
        // Instantiate the data problem.
        // [START data]
        int[] Weights = { 48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36 };
        int[] Values = { 10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25 };
        int NumItems = Weights.Length;
        int[] allItems = Enumerable.Range(0, NumItems).ToArray();

        int[] BinCapacities = { 100, 100, 100, 100, 100 };
        int NumBins = BinCapacities.Length;
        int[] allBins = Enumerable.Range(0, NumBins).ToArray();
        // [END data]

        // Model.
        // [START model]
        CpModel model = new CpModel();
        // [END model]

        // Variables.
        // [START variables]
        ILiteral[,] x = new ILiteral[NumItems, NumBins];
        foreach (int i in allItems)
        {
            foreach (int b in allBins)
            {
                x[i, b] = model.NewBoolVar($"x_{i}_{b}");
            }
        }
        // [END variables]

        // Constraints.
        // [START constraints]
        // Each item is assigned to at most one bin.
        foreach (int i in allItems)
        {
            List<ILiteral> literals = new List<ILiteral>();
            foreach (int b in allBins)
            {
                literals.Add(x[i, b]);
            }
            model.AddAtMostOne(literals);
        }

        // The amount packed in each bin cannot exceed its capacity.
        foreach (int b in allBins)
        {
            List<ILiteral> items = new List<ILiteral>();
            foreach (int i in allItems)
            {
                items.Add(x[i, b]);
            }
            model.Add(LinearExpr.WeightedSum(items, Weights) <= BinCapacities[b]);
        }
        // [END constraints]

        // Objective.
        // [START objective]
        LinearExprBuilder obj = LinearExpr.NewBuilder();
        foreach (int i in allItems)
        {
            foreach (int b in allBins)
            {
                obj.AddTerm(x[i, b], Values[i]);
            }
        }
        model.Maximize(obj);
        //  [END objective]

        // Solve
        // [START solve]
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        // [END solve]

        // Print solution.
        // [START print_solution]
        // Check that the problem has a feasible solution.
        if (status == CpSolverStatus.Optimal || status == CpSolverStatus.Feasible)
        {
            Console.WriteLine($"Total packed value: {solver.ObjectiveValue}");
            double TotalWeight = 0.0;
            foreach (int b in allBins)
            {
                double BinWeight = 0.0;
                double BinValue = 0.0;
                Console.WriteLine($"Bin {b}");
                foreach (int i in allItems)
                {
                    if (solver.BooleanValue(x[i, b]))
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
