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

public class DeBruijn
{
    /**
     *
     *  ToNum(solver, a, num, base)
     *
     *  channelling between the array a and the number num.
     *
     */
    private static Constraint ToNum(IntVar[] a, IntVar num, int bbase)
    {
        int len = a.Length;

        IntVar[] tmp = new IntVar[len];
        for (int i = 0; i < len; i++)
        {
            tmp[i] = (a[i] * (int)Math.Pow(bbase, (len - i - 1))).Var();
        }
        return tmp.Sum() == num;
    }

    /**
     *
     * Implements "arbitrary" de Bruijn sequences.
     * See http://www.hakank.org/or-tools/debruijn_binary.py
     *
     */
    private static void Solve(int bbase, int n, int m)
    {
        Solver solver = new Solver("DeBruijn");

        // Ensure that the number of each digit in bin_code is
        // the same. Nice feature, but it can slow things down...
        bool check_same_gcc = false; // true;

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(m, 0, (int)Math.Pow(bbase, n) - 1, "x");
        IntVar[,] binary = solver.MakeIntVarMatrix(m, n, 0, bbase - 1, "binary");

        // this is the de Bruijn sequence
        IntVar[] bin_code = solver.MakeIntVarArray(m, 0, bbase - 1, "bin_code");

        // occurences of each number in bin_code
        IntVar[] gcc = solver.MakeIntVarArray(bbase, 0, m, "gcc");

        // for the branching
        IntVar[] all = new IntVar[2 * m + bbase];
        for (int i = 0; i < m; i++)
        {
            all[i] = x[i];
            all[m + i] = bin_code[i];
        }
        for (int i = 0; i < bbase; i++)
        {
            all[2 * m + i] = gcc[i];
        }

        //
        // Constraints
        //

        solver.Add(x.AllDifferent());

        // converts x <-> binary
        for (int i = 0; i < m; i++)
        {
            IntVar[] t = new IntVar[n];
            for (int j = 0; j < n; j++)
            {
                t[j] = binary[i, j];
            }
            solver.Add(ToNum(t, x[i], bbase));
        }

        // the de Bruijn condition:
        // the first elements in binary[i] is the same as the last
        // elements in binary[i-1]
        for (int i = 1; i < m; i++)
        {
            for (int j = 1; j < n; j++)
            {
                solver.Add(binary[i - 1, j] == binary[i, j - 1]);
            }
        }

        // ... and around the corner
        for (int j = 1; j < n; j++)
        {
            solver.Add(binary[m - 1, j] == binary[0, j - 1]);
        }

        // converts binary -> bin_code (de Bruijn sequence)
        for (int i = 0; i < m; i++)
        {
            solver.Add(bin_code[i] == binary[i, 0]);
        }

        // extra: ensure that all the numbers in the de Bruijn sequence
        // (bin_code) has the same occurrences (if check_same_gcc is True
        // and mathematically possible)
        solver.Add(bin_code.Distribute(gcc));
        if (check_same_gcc && m % bbase == 0)
        {
            for (int i = 1; i < bbase; i++)
            {
                solver.Add(gcc[i] == gcc[i - 1]);
            }
        }

        // symmetry breaking:
        // the minimum value of x should be first
        // solver.Add(x[0] == x.Min());

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(all, Solver.CHOOSE_MIN_SIZE_LOWEST_MAX, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.Write("x: ");
            for (int i = 0; i < m; i++)
            {
                Console.Write(x[i].Value() + " ");
            }

            Console.Write("\nde Bruijn sequence:");
            for (int i = 0; i < m; i++)
            {
                Console.Write(bin_code[i].Value() + " ");
            }

            Console.Write("\ngcc: ");
            for (int i = 0; i < bbase; i++)
            {
                Console.Write(gcc[i].Value() + " ");
            }
            Console.WriteLine("\n");

            // for debugging etc: show the full binary table
            /*
            Console.Write("binary:");
            for(int i = 0; i < m; i++) {
              for(int j = 0; j < n; j++) {
                Console.Write(binary[i][j].Value() + " ");
              }
              Console.WriteLine();
            }
            Console.WriteLine();
            */
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        int bbase = 2;
        int n = 3;
        int m = 8;

        if (args.Length > 0)
        {
            bbase = Convert.ToInt32(args[0]);
        }

        if (args.Length > 1)
        {
            n = Convert.ToInt32(args[1]);
        }

        if (args.Length > 2)
        {
            m = Convert.ToInt32(args[2]);
        }

        Solve(bbase, n, m);
    }
}
