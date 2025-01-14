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
using Google.OrTools.ModelBuilder;
// [END import]

// [START program_part1]
public class BinPackingMb
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

        // [START model]
        Model model = new Model();
        // [END model]

        // [START program_part2]
        // [START variables]
        Variable[,] x = new Variable[data.NumItems, data.NumBins];
        for (int i = 0; i < data.NumItems; i++)
        {
            for (int j = 0; j < data.NumBins; j++)
            {
                x[i, j] = model.NewBoolVar($"x_{i}_{j}");
            }
        }
        Variable[] y = new Variable[data.NumBins];
        for (int j = 0; j < data.NumBins; j++)
        {
            y[j] = model.NewBoolVar($"y_{j}");
        }
        // [END variables]

        // [START constraints]
        for (int i = 0; i < data.NumItems; ++i)
        {
            var assignedWork = LinearExpr.NewBuilder();
            for (int j = 0; j < data.NumBins; ++j)
            {
                assignedWork.Add(x[i, j]);
            }
            model.Add(assignedWork == 1);
        }

        for (int j = 0; j < data.NumBins; ++j)
        {
            var load = LinearExpr.NewBuilder();
            for (int i = 0; i < data.NumItems; ++i)
            {
                load.AddTerm(x[i, j], DataModel.Weights[i]);
            }
            model.Add(y[j] * data.BinCapacity >= load);
        }
        // [END constraints]

        // [START objective]
        model.Minimize(LinearExpr.Sum(y));
        // [END objective]

        // [START solver]
        // Create the solver with the SCIP backend and check it is supported.
        Solver solver = new Solver("SCIP");
        if (!solver.SolverIsSupported())
        {
            return;
        }
        // [END solver]

        // [START solve]
        SolveStatus resultStatus = solver.Solve(model);
        // [END solve]

        // [START print_solution]
        // Check that the problem has an optimal solution.
        if (resultStatus != SolveStatus.OPTIMAL)
        {
            Console.WriteLine("The problem does not have an optimal solution!");
            return;
        }
        Console.WriteLine($"Number of bins used: {solver.ObjectiveValue}");
        double TotalWeight = 0.0;
        for (int j = 0; j < data.NumBins; ++j)
        {
            double BinWeight = 0.0;
            if (solver.Value(y[j]) == 1)
            {
                Console.WriteLine($"Bin {j}");
                for (int i = 0; i < data.NumItems; ++i)
                {
                    if (solver.Value(x[i, j]) == 1)
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
