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
        IntVar[,] x = new IntVar[NumItems, NumBins];
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
            IntVar[] vars = new IntVar[NumBins];
            foreach (int b in allBins)
            {
                vars[b] = x[i, b];
            }
            model.Add(LinearExpr.Sum(vars) <= 1);
        }

        // The amount packed in each bin cannot exceed its capacity.
        foreach (int b in allBins)
        {
            IntVar[] vars = new IntVar[NumItems];
            foreach (int i in allItems)
            {
                vars[i] = x[i, b];
            }
            model.Add(LinearExpr.ScalProd(vars, Weights) <= BinCapacities[b]);
        }
        // [END constraints]

        // Objective.
        // [START objective]
        IntVar[] objectiveVars = new IntVar[NumItems * NumBins];
        int[] objectiveValues = new int[NumItems * NumBins];
        foreach (int i in allItems)
        {
            foreach (int b in allBins)
            {
                int k = i * NumBins + b;
                objectiveVars[k] = x[i, b];
                objectiveValues[k] = Values[i];
            }
        }
        model.Maximize(LinearExpr.ScalProd(objectiveVars, objectiveValues));
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
