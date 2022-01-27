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
// Solve a multiple knapsack problem using a MIP solver.
// [START import]
using System;
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.LinearSolver;
// [END import]

public class MultipleKnapsackMip
{
    public static void Main()
    {
        // Instantiate the data problem.
        // [START data]
        double[] Weights = { 48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36 };
        double[] Values = { 10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25 };
        int NumItems = Weights.Length;
        int[] allItems = Enumerable.Range(0, NumItems).ToArray();

        double[] BinCapacities = { 100, 100, 100, 100, 100 };
        int NumBins = BinCapacities.Length;
        int[] allBins = Enumerable.Range(0, NumBins).ToArray();
        // [END data]

        // [START solver]
        // Create the linear solver with the SCIP backend.
        Solver solver = Solver.CreateSolver("SCIP");
        // [END solver]

        // Variables.
        // [START variables]
        Variable[,] x = new Variable[NumItems, NumBins];
        foreach (int i in allItems)
        {
            foreach (int b in allBins)
            {
                x[i, b] = solver.MakeBoolVar($"x_{i}_{b}");
            }
        }
        // [END variables]

        // Constraints.
        // [START constraints]
        // Each item is assigned to at most one bin.
        foreach (int i in allItems)
        {
            Constraint constraint = solver.MakeConstraint(0, 1, "");
            foreach (int b in allBins)
            {
                constraint.SetCoefficient(x[i, b], 1);
            }
        }

        // The amount packed in each bin cannot exceed its capacity.
        foreach (int b in allBins)
        {
            Constraint constraint = solver.MakeConstraint(0, BinCapacities[b], "");
            foreach (int i in allItems)
            {
                constraint.SetCoefficient(x[i, b], Weights[i]);
            }
        }
        // [END constraints]

        // Objective.
        // [START objective]
        Objective objective = solver.Objective();
        foreach (int i in allItems)
        {
            foreach (int b in allBins)
            {
                objective.SetCoefficient(x[i, b], Values[i]);
            }
        }
        objective.SetMaximization();
        // [END objective]

        // [START solve]
        Solver.ResultStatus resultStatus = solver.Solve();
        // [END solve]

        // [START print_solution]
        // Check that the problem has an optimal solution.
        if (resultStatus == Solver.ResultStatus.OPTIMAL)
        {
            Console.WriteLine($"Total packed value: {solver.Objective().Value()}");
            double TotalWeight = 0.0;
            foreach (int b in allBins)
            {
                double BinWeight = 0.0;
                double BinValue = 0.0;
                Console.WriteLine("Bin " + b);
                foreach (int i in allItems)
                {
                    if (x[i, b].SolutionValue() == 1)
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
            Console.WriteLine("The problem does not have an optimal solution!");
        }
        // [END print_solution]
    }
}
// [END program]
