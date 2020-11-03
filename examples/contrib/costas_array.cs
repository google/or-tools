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

public class CostasArray
{
    /**
     *
     * Costas array
     *
     * From http://mathworld.wolfram.com/CostasArray.html:
     * """
     * An order-n Costas array is a permutation on {1,...,n} such
     * that the distances in each row of the triangular difference
     * table are distinct. For example, the permutation {1,3,4,2,5}
     * has triangular difference table {2,1,-2,3}, {3,-1,1}, {1,2},
     * and {4}. Since each row contains no duplications, the permutation
     * is therefore a Costas array.
     * """
     *
     * Also see
     * http://en.wikipedia.org/wiki/Costas_array
     * http://hakank.org/or-tools/costas_array.py
     *
     */
    private static void Solve(int n = 6)
    {
        Solver solver = new Solver("CostasArray");

        //
        // Data
        //
        Console.WriteLine("n: {0}", n);

        //
        // Decision variables
        //
        IntVar[] costas = solver.MakeIntVarArray(n, 1, n, "costas");
        IntVar[,] differences = solver.MakeIntVarMatrix(n, n, -n + 1, n - 1, "differences");

        //
        // Constraints
        //

        // Fix the values in the lower triangle in the
        // difference matrix to -n+1. This removes variants
        // of the difference matrix for the the same Costas array.
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j <= i; j++)
            {
                solver.Add(differences[i, j] == -n + 1);
            }
        }

        // hakank: All the following constraints are from
        // Barry O'Sullivans's original model.
        //
        solver.Add(costas.AllDifferent());

        // "How do the positions in the Costas array relate
        //  to the elements of the distance triangle."
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (i < j)
                {
                    solver.Add(differences[i, j] - (costas[j] - costas[j - i - 1]) == 0);
                }
            }
        }

        // "All entries in a particular row of the difference
        //  triangle must be distint."
        for (int i = 0; i < n - 2; i++)
        {
      IntVar[] tmp = (from j in Enumerable.Range(0, n)
                          where j > i select differences[i, j])
                         .ToArray();
      solver.Add(tmp.AllDifferent());
        }

        //
        // "All the following are redundant - only here to speed up search."
        //

        // "We can never place a 'token' in the same row as any other."
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (i < j)
                {
                    solver.Add(differences[i, j] != 0);
                    solver.Add(differences[i, j] != 0);
                }
            }
        }

        for (int k = 2; k < n; k++)
        {
            for (int l = 2; l < n; l++)
            {
                if (k < l)
                {
                    solver.Add((differences[k - 2, l - 1] + differences[k, l]) -
                                   (differences[k - 1, l - 1] + differences[k - 1, l]) ==
                               0);
                }
            }
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(costas, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.Write("costas: ");
            for (int i = 0; i < n; i++)
            {
                Console.Write("{0} ", costas[i].Value());
            }
            Console.WriteLine("\ndifferences:");
            for (int i = 0; i < n; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    long v = differences[i, j].Value();
                    if (v == -n + 1)
                    {
                        Console.Write("   ");
                    }
                    else
                    {
                        Console.Write("{0,2} ", v);
                    }
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
        int n = 6;

        if (args.Length > 0)
        {
            n = Convert.ToInt32(args[0]);
        }

        Solve(n);
    }
}
