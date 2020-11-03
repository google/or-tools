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
using Google.OrTools.ConstraintSolver;

public class Seseman
{
    /**
     *
     * Solves the Seseman convent problem.
     * See http://www.hakank.org/google_or_tools/seseman.py
     *
     */
    private static void Solve(int n = 3)
    {
        Solver solver = new Solver("Seseman");

        //
        // data
        //
        int border_sum = n * n;

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(n, n, 0, n * n, "x");
        IntVar[] x_flat = x.Flatten();
        IntVar total_sum = x_flat.Sum().Var();

        //
        // Constraints
        //

        // zero in all middle cells
        for (int i = 1; i < n - 1; i++)
        {
            for (int j = 1; j < n - 1; j++)
            {
                solver.Add(x[i, j] == 0);
            }
        }

        // all borders must be >= 1
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (i == 0 || j == 0 || i == n - 1 || j == n - 1)
                {
                    solver.Add(x[i, j] >= 1);
                }
            }
        }

        // sum the four borders
        IntVar[] border1 = new IntVar[n];
        IntVar[] border2 = new IntVar[n];
        IntVar[] border3 = new IntVar[n];
        IntVar[] border4 = new IntVar[n];
        for (int i = 0; i < n; i++)
        {
            border1[i] = x[i, 0];
            border2[i] = x[i, n - 1];
            border3[i] = x[0, i];
            border4[i] = x[n - 1, i];
        }
        solver.Add(border1.Sum() == border_sum);
        solver.Add(border2.Sum() == border_sum);
        solver.Add(border3.Sum() == border_sum);
        solver.Add(border4.Sum() == border_sum);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.CHOOSE_PATH, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);
        while (solver.NextSolution())
        {
            Console.WriteLine("total_sum: {0} ", total_sum.Value());
            for (int i = 0; i < n; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    Console.Write("{0} ", x[i, j].Value());
                }
                Console.WriteLine();
            }

            Console.WriteLine();
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        int n = 3;

        if (args.Length > 0)
        {
            n = Convert.ToInt32(args[0]);
        }

        Solve(n);
    }
}
