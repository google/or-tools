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
public class MipVarArray
{
    // [START data_model]
    class DataModel
    {
        public double[,] ConstraintCoeffs = {
            { 5, 7, 9, 2, 1 },
            { 18, 4, -9, 10, 12 },
            { 4, 7, 3, 8, 5 },
            { 5, 13, 16, 3, -7 },
        };
        public double[] Bounds = { 250, 285, 211, 315 };
        public double[] ObjCoeffs = { 7, 8, 2, 9, 6 };
        public int NumVars = 5;
        public int NumConstraints = 4;
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
        Variable[] x = new Variable[data.NumVars];
        for (int j = 0; j < data.NumVars; j++)
        {
            x[j] = solver.MakeIntVar(0.0, double.PositiveInfinity, $"x_{j}");
        }
        Console.WriteLine("Number of variables = " + solver.NumVariables());
        // [END variables]

        // [START constraints]
        for (int i = 0; i < data.NumConstraints; ++i)
        {
            Constraint constraint = solver.MakeConstraint(0, data.Bounds[i], "");
            for (int j = 0; j < data.NumVars; ++j)
            {
                constraint.SetCoefficient(x[j], data.ConstraintCoeffs[i, j]);
            }
        }
        Console.WriteLine("Number of constraints = " + solver.NumConstraints());
        // [END constraints]

        // [START objective]
        Objective objective = solver.Objective();
        for (int j = 0; j < data.NumVars; ++j)
        {
            objective.SetCoefficient(x[j], data.ObjCoeffs[j]);
        }
        objective.SetMaximization();
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

        Console.WriteLine("Solution:");
        Console.WriteLine("Optimal objective value = " + solver.Objective().Value());

        for (int j = 0; j < data.NumVars; ++j)
        {
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
// [END program_part2]
// [END program]
