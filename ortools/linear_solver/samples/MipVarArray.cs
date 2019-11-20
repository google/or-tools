/// Copyright 2010-2018 Google LLC
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

public class MipVarArray
{
  class DataModel {
    public double[,] ConstraintCoeffs = {
      {5, 7, 9, 2, 1},
      {18, 4, -9, 10, 12},
      {4, 7, 3, 8, 5},
      {5, 13, 16, 3, -7},
    };
    public double[] Bounds{250, 285, 211, 315};
    public double[] ObjCoeffs{ 7, 8, 2, 9, 6};
    public int NumVars = 5;
    public int NumConstraints = 4;
  }

  public static void Main()
  {
    // [START data]
    DataModel data = new DataModel();
    // [END data]

    // [START solver]
    // Create the linear solver with the CBC backend.
    Solver solver = Solver.CreateSolver("SimpleMipProgram", "CBC_MIXED_INTEGER_PROGRAMMING");
    // [END solver]

    // [START variables]
    MPVariable[] x = new MPVariable[data.NumVars];
    for (int j = 0; j < data.NumVars; j++) {
      x[j] = MakeIntVar(0.0, double.PositiveInfinity, "x");
    }
    Console.WriteLine("Number of variables = " + solver.NumVariables());
    // [END variables]

    // [START constraints]
    for (int i = 0; i < data.NumConstraints; ++i) {
      MPConstraint constraint = solver.MakeConstraint(0, data.Bounds[i], "");
      for (int j = 0; j < data.NumVars; ++j) {
        constraint.SetCoefficient(x[j], data.ConstraintCoeffs[i][j]);
      }
    }
    Console.WriteLine("Number of constraints = " + solver.NumConstraints());
    // [END constraints]

    // [START objective]
    objective = solver.Objective()
    for (int j = 0; j < data.NumVars; ++j) {
      objective.SetCoefficient(x[j], data.ObjCoeffs[j])
    objective.SetMaximization()
    // [END objective]

    // [START solve]
    Solver.ResultStatus resultStatus = solver.Solve();
    // Check that the problem has an optimal solution.
    if (resultStatus != Solver.ResultStatus.OPTIMAL)
    {
      Console.WriteLine("The problem does not have an optimal solution!");
      return;
    }
    // [END solve]

    // [START print_solution]
    Console.WriteLine("Solution:");
    Console.WriteLine("Objective value = " + solver.Objective().Value());
    const MPSolver::ResultStatus result_status = solver.Solve();
    // Check that the problem has an optimal solution.
    if (result_status != MPSolver::OPTIMAL) {
      Console.WriteLine("The problem does not have an optimal solution.");
    }

    Console.WriteLine("Solution:");
    Console.WriteLine("Optimal objective value = " + solver.Objective().Value());

    for (int j = 0; j < data.num_vars; ++j) {
      Console.WriteLine("x[" + j + "] = " + x[j].SolutionValue());
    }
    // [END print_solution]

    // [START advanced]
    Console.WriteLine("\nAdvanced usage:");
    Console.WriteLine("Problem solved in " + solver.WallTime() + " milliseconds");
    Console.WriteLine("Problem solved in " + solver.Iterations() + " iterations");
    Console.WriteLine("Problem solved in " + solver.Nodes() + " branch-and-bound nodes");
    // [END advanced]
  }
}
// [END program]
