//
// Copyright 2012 Hakan Kjellerstrand
//
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
using System.Collections;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class SetCoveringDeployment
{
    /**
     *
     * Solves a set covering deployment problem.
     * See  See http://www.hakank.org/or-tools/set_covering_deployment.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("SetCoveringDeployment");

        //
        // data
        //

        // From http://mathworld.wolfram.com/SetCoveringDeployment.html
        string[] countries = { "Alexandria", "Asia Minor", "Britain", "Byzantium", "Gaul", "Iberia", "Rome", "Tunis" };

        int n = countries.Length;

        // the incidence matrix (neighbours)
        int[,] mat = { { 0, 1, 0, 1, 0, 0, 1, 1 }, { 1, 0, 0, 1, 0, 0, 0, 0 }, { 0, 0, 0, 0, 1, 1, 0, 0 },
                       { 1, 1, 0, 0, 0, 0, 1, 0 }, { 0, 0, 1, 0, 0, 1, 1, 0 }, { 0, 0, 1, 0, 1, 0, 1, 1 },
                       { 1, 0, 0, 1, 1, 1, 0, 1 }, { 1, 0, 0, 0, 0, 1, 1, 0 } };

        //
        // Decision variables
        //

        // First army
        IntVar[] x = solver.MakeIntVarArray(n, 0, 1, "x");

        // Second (reserve) army
        IntVar[] y = solver.MakeIntVarArray(n, 0, 1, "y");

        // total number of armies
        IntVar num_armies = (x.Sum() + y.Sum()).Var();

        //
        // Constraints
        //

        //
        //  Constraint 1: There is always an army in a city
        //                (+ maybe a backup)
        //                Or rather: Is there a backup, there
        //                must be an an army
        //
        for (int i = 0; i < n; i++)
        {
            solver.Add(x[i] >= y[i]);
        }

        //
        // Constraint 2: There should always be an backup
        //               army near every city
        //
        for (int i = 0; i < n; i++)
        {
      IntVar[] count_neighbours =
          (from j in Enumerable.Range(0, n) where mat[i, j] == 1 select(y[j]))
              .ToArray();

      solver.Add((x[i] + count_neighbours.Sum()) >= 1);
        }

        //
        // objective
        //
        OptimizeVar objective = num_armies.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db, objective);

        while (solver.NextSolution())
        {
            Console.WriteLine("num_armies: " + num_armies.Value());
            for (int i = 0; i < n; i++)
            {
                if (x[i].Value() == 1)
                {
                    Console.Write("Army: " + countries[i] + " ");
                }

                if (y[i].Value() == 1)
                {
                    Console.WriteLine(" Reverse army: " + countries[i]);
                }
            }
            Console.WriteLine("\n");
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
