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
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class YoungTableaux
{
    /**
     *
     * Implements Young tableaux and partitions.
     * See http://www.hakank.org/or-tools/young_tableuax.py
     *
     */
    private static void Solve(int n)
    {
        Solver solver = new Solver("YoungTableaux");

        //
        // data
        //
        Console.WriteLine("n: {0}\n", n);

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(n, n, 1, n + 1, "x");
        IntVar[] x_flat = x.Flatten();

        // partition structure
        IntVar[] p = solver.MakeIntVarArray(n, 0, n + 1, "p");

        //
        // Constraints
        //
        // 1..n is used exactly once
        for (int i = 1; i <= n; i++)
        {
            solver.Add(x_flat.Count(i, 1));
        }

        solver.Add(x[0, 0] == 1);

        // row wise
        for (int i = 0; i < n; i++)
        {
            for (int j = 1; j < n; j++)
            {
                solver.Add(x[i, j] >= x[i, j - 1]);
            }
        }

        // column wise
        for (int j = 0; j < n; j++)
        {
            for (int i = 1; i < n; i++)
            {
                solver.Add(x[i, j] >= x[i - 1, j]);
            }
        }

        // calculate the structure (i.e. the partition)
        for (int i = 0; i < n; i++)
        {
            IntVar[] b = new IntVar[n];
            for (int j = 0; j < n; j++)
            {
                b[j] = x[i, j] <= n;
            }
            solver.Add(p[i] == b.Sum());
        }

        solver.Add(p.Sum() == n);

        for (int i = 1; i < n; i++)
        {
            solver.Add(p[i - 1] >= p[i]);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.Write("p: ");
            for (int i = 0; i < n; i++)
            {
                Console.Write(p[i].Value() + " ");
            }
            Console.WriteLine("\nx:");

            for (int i = 0; i < n; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    long val = x[i, j].Value();
                    if (val <= n)
                    {
                        Console.Write(val + " ");
                    }
                }
                if (p[i].Value() > 0)
                {
                    Console.WriteLine();
                }
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
        int n = 5;
        if (args.Length > 0)
        {
            n = Convert.ToInt32(args[0]);
        }
        Solve(n);
    }
}
