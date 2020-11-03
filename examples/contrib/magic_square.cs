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

public class MagicSquare
{
    /**
     *
     * Solves the Magic Square problem.
     * See http://www.hakank.org/or-tools/magic_square.py
     *
     */
    private static void Solve(int n = 4, int num = 0, int print = 1)
    {
        Solver solver = new Solver("MagicSquare");

        Console.WriteLine("n: {0}", n);

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(n, n, 1, n * n, "x");
        // for the branching
        IntVar[] x_flat = x.Flatten();

        //
        // Constraints
        //
        long s = (n * (n * n + 1)) / 2;
        Console.WriteLine("s: " + s);

        IntVar[] diag1 = new IntVar[n];
        IntVar[] diag2 = new IntVar[n];
        for (int i = 0; i < n; i++)
        {
            IntVar[] row = new IntVar[n];
            for (int j = 0; j < n; j++)
            {
                row[j] = x[i, j];
            }
            // sum row to s
            solver.Add(row.Sum() == s);

            diag1[i] = x[i, i];
            diag2[i] = x[i, n - i - 1];
        }

        // sum diagonals to s
        solver.Add(diag1.Sum() == s);
        solver.Add(diag2.Sum() == s);

        // sum columns to s
        for (int j = 0; j < n; j++)
        {
            IntVar[] col = new IntVar[n];
            for (int i = 0; i < n; i++)
            {
                col[i] = x[i, j];
            }
            solver.Add(col.Sum() == s);
        }

        // all are different
        solver.Add(x_flat.AllDifferent());

        // symmetry breaking: upper left is 1
        // solver.Add(x[0,0] == 1);

        //
        // Search
        //

        DecisionBuilder db = solver.MakePhase(x_flat, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_CENTER_VALUE);

        solver.NewSearch(db);

        int c = 0;
        while (solver.NextSolution())
        {
            if (print != 0)
            {
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

            c++;
            if (num > 0 && c >= num)
            {
                break;
            }
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        int n = 4;
        int num = 0;
        int print = 1;

        if (args.Length > 0)
        {
            n = Convert.ToInt32(args[0]);
        }

        if (args.Length > 1)
        {
            num = Convert.ToInt32(args[1]);
        }

        if (args.Length > 2)
        {
            print = Convert.ToInt32(args[2]);
        }

        Solve(n, num, print);
    }
}
