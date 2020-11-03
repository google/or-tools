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

public class CoinsGrid
{
    /**
     *
     * Solves the Coins Grid problm.
     * See http://www.hakank.org/google_or_tools/coins_grid.py
     *
     */
    private static void Solve(int n = 31, int c = 14)
    {
        Solver solver = new Solver("CoinsGrid");

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(n, n, 0, 1, "x");
        IntVar[] x_flat = x.Flatten();

        //
        // Constraints
        //

        // sum row/columns == c
        for (int i = 0; i < n; i++)
        {
            IntVar[] row = new IntVar[n];
            IntVar[] col = new IntVar[n];
            for (int j = 0; j < n; j++)
            {
                row[j] = x[i, j];
                col[j] = x[j, i];
            }
            solver.Add(row.Sum() == c);
            solver.Add(col.Sum() == c);
        }

        // quadratic horizonal distance
        IntVar[] obj_tmp = new IntVar[n * n];
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                obj_tmp[i * n + j] = (x[i, j] * (i - j) * (i - j)).Var();
            }
        }
        IntVar obj_var = obj_tmp.Sum().Var();

        //
        // Objective
        //
        OptimizeVar obj = obj_var.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MAX_VALUE);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("obj: " + obj_var.Value());
            for (int i = 0; i < n; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    Console.Write(x[i, j].Value() + " ");
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
        int n = 31;
        int c = 14;

        if (args.Length > 0)
        {
            n = Convert.ToInt32(args[0]);
        }

        if (args.Length > 1)
        {
            c = Convert.ToInt32(args[1]);
        }

        Solve(n, c);
    }
}
