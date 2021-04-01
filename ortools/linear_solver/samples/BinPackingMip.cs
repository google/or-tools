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
using Google.OrTools.LinearSolver;
// [END import]

// [START program_part1]
public class BinPackingMip
{
    // [START data_model]
    class DataModel
    {
        public static double[] Weights = { 48, 30, 19, 36, 36, 27, 42, 42, 36, 24, 30 };
        public int NumItems = Weights.Length;
        public int NumBins = Weights.Length;
        public double BinCapacity = 100.0;
    }
    // [END data_model]
    public static void Main()
    {
        // [START data]
        DataModel data = new DataModel();
        // [END data]
        // [END program_part1]

        // [START solver]
        // Create the linear solver with the SCIP backend.
        Solver solver = Solver.CreateSolver("SCIP");
        // [END solver]

        // [START program_part2]
        // [START variables]
        Variable[,] x = new Variable[data.NumItems, data.NumBins];
        for (int i = 0; i < data.NumItems; i++)
        {
            for (int j = 0; j < data.NumBins; j++)
            {
                x[i, j] = solver.MakeIntVar(0, 1, $"x_{i}_{j}");
            }
        }
        Variable[] y = new Variable[data.NumBins];
        for (int j = 0; j < data.NumBins; j++)
        {
            y[j] = solver.MakeIntVar(0, 1, $"y_{j}");
        }
        // [END variables]

        // [START constraints]
        for (int i = 0; i < data.NumItems; ++i)
        {
            Constraint constraint = solver.MakeConstraint(1, 1, "");
            for (int j = 0; j < data.NumBins; ++j)
            {
                constraint.SetCoefficient(x[i, j], 1);
            }
        }

        for (int j = 0; j < data.NumBins; ++j)
        {
            Constraint constraint = solver.MakeConstraint(0, Double.PositiveInfinity, "");
            constraint.SetCoefficient(y[j], data.BinCapacity);
            for (int i = 0; i < data.NumItems; ++i)
            {
                constraint.SetCoefficient(x[i, j], -DataModel.Weights[i]);
            }
        }
        // [END constraints]

        // [START objective]
        Objective objective = solver.Objective();
        for (int j = 0; j < data.NumBins; ++j)
        {
            objective.SetCoefficient(y[j], 1);
        }
        objective.SetMinimization();
        // [END objective]

        // [START solve]
        Solver.ResultStatus resultStatus = solver.Solve();
        // [END solve]

        // [START print_solution]
        // Check that the problem has an optimal solution.
        if (resultStatus != Solver.ResultStatus.OPTIMAL)
        {
            Console.WriteLine("The problem does not have an optimal solution!");
            return;
        }
        Console.WriteLine($"Number of bins used: {solver.Objective().Value()}");
        double TotalWeight = 0.0;
        for (int j = 0; j < data.NumBins; ++j)
        {
            double BinWeight = 0.0;
            if (y[j].SolutionValue() == 1)
            {
                Console.WriteLine($"Bin {j}");
                for (int i = 0; i < data.NumItems; ++i)
                {
                    if (x[i, j].SolutionValue() == 1)
                    {
                        Console.WriteLine($"Item {i} weight: {DataModel.Weights[i]}");
                        BinWeight += DataModel.Weights[i];
                    }
                }
                Console.WriteLine($"Packed bin weight: {BinWeight}");
                TotalWeight += BinWeight;
            }
        }
        Console.WriteLine($"Total packed weight: {TotalWeight}");
        // [END print_solution]
    }
}
// [END program_part2]
// [END program]
