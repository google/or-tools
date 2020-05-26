// Copyright 2010-2018 Google LLC
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
public class MultipleKnapsackMip
{
  // [START data_model]
  class DataModel
  {
    public double[] Weights =
      {48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36};
    public double[] Values =
      {10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25};
    public double[] BinCapacities = {100, 100, 100, 100, 100};
    public int NumItems = Weights.Length;
    public int NumBins = 5;
  }
  // [END data_model]
  public static void Main()
  {
    // [START data]
    DataModel data = new DataModel();
    // [END data]
    // [END program_part1]

    // [START solver]
    // Create the linear solver with the CBC backend.
    Solver solver = Solver.CreateSolver("MultipleKnapsackMip", "CBC_MIXED_INTEGER_PROGRAMMING");
    // [END solver]

    // [START program_part2]
    // [START variables]
    MPVariable[,] x = new MPVariable[data.NumItems][data.NumBins];
    for (int i = 0; i < data.NumItems; i++)
    {
      for (int j = 0; j < data.NumBins; j++)
      {
        x[i][j] = MakeIntVar(0, 1, String.Format("x_{0}_{1}", i, j));
      }
    }
    // [END variables]

    // [START constraints]
    for (int i = 0; i < data.NumItems; ++i) {
      LinearExpr sum;
      for (int j = 0; j < data.NumBins; ++j) {
        sum += x[i][j];
      }
      solver.MakeRowConstraint(sum <= 1.0);
    }

    for (int i = 0; i < data.NumConstraints; ++i)
    {
      MPConstraint constraint = solver.MakeConstraint(0, data.Bounds[i], "");
      for (int j = 0; j < data.NumVars; ++j)
      {
        constraint.SetCoefficient(x[j], data.ConstraintCoeffs[i][j]);
      }
    }
    for (int j = 0; j < data.NumBins; ++j)
    {
      LinearExpr Weight;
      for (int i = 0; i < data.NumItems; ++i)
      {
        Weight += data.Weights[i]*LinearExpr(x[i][j]);
      }
    solver.MakeRowConstraint(Weight <= data.BinCapacities[j]);
    }
    // [END constraints]

    // [START objective]
    objective = solver.Objective();
    LinearExpr Value;
    for (int i = 0; i < data.NumItems; ++i)
    {
      for (int j = 0; j < data.NumBins; ++j)
      {
        Value += data.Values[i] * LinearExpr(x[i][j]);
      }
    }
    objective.MaximizeLinearExpr(Value);
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
    Console.WriteLine("Total packed value: " + solver.Objective().Value());
    int TotalWeight = 0;
    for (int j = 0; j < data.NumBins; ++j)
    {
      int BinWeight = 0;
      int BinValue = 0;
      Console.WriteLine("Bin " + j);
      for (int i = 0; i < data.NumItems; ++i)
      {
        if (x[i][j].SolutionValue() == 1)
        {
          Console.WriteLine("Item " + i + " weight: " + data.Weights[i]
                                    + "  values: " + data.Values[i];
          BinWeight += data.Weights[i];
          BinValue += data.Values[i]
        }
      }
      Console.WriteLine("Packed bin weight: " + BinWeight);
      Console.WriteLine("Packed bin value: " + BinValue);
      TotalWeight += BinWeight;
    }
    Console.WriteLine("Total packed weight: " + TotalWeight);
    // [END print_solution]
  }
}
// [END program_part2]
// [END program]
