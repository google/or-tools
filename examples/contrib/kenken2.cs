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
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.ConstraintSolver;

public class KenKen2
{
    /**
     * Ensure that the sum of the segments
     * in cc == res
     *
     */
    public static void calc(Solver solver, int[] cc, IntVar[,] x, int res)
    {
        int ccLen = cc.Length;
        if (ccLen == 4)
        {
            // for two operands there's
            // a lot of possible variants
            IntVar a = x[cc[0] - 1, cc[1] - 1];
            IntVar b = x[cc[2] - 1, cc[3] - 1];

            IntVar r1 = a + b == res;
            IntVar r2 = a * b == res;
            IntVar r3 = a * res == b;
            IntVar r4 = b * res == a;
            IntVar r5 = a - b == res;
            IntVar r6 = b - a == res;

            solver.Add(r1 + r2 + r3 + r4 + r5 + r6 >= 1);
        }
        else
        {
            // For length > 2 then res is either the sum
            // the the product of the segment

            // sum the numbers
            int len = cc.Length / 2;
            IntVar[] xx = (from i in Enumerable.Range(0, len) select x[cc[i * 2] - 1, cc[i * 2 + 1] - 1]).ToArray();

            // Sum
            IntVar this_sum = xx.Sum() == res;

            // Product
            // IntVar this_prod = (xx.Prod() == res).Var(); // don't work
            IntVar this_prod;
            if (xx.Length == 3)
            {
                this_prod = (x[cc[0] - 1, cc[1] - 1] * x[cc[2] - 1, cc[3] - 1] * x[cc[4] - 1, cc[5] - 1]) == res;
            }
            else
            {
                this_prod = (x[cc[0] - 1, cc[1] - 1] * x[cc[2] - 1, cc[3] - 1] * x[cc[4] - 1, cc[5] - 1] *
                             x[cc[6] - 1, cc[7] - 1]) == res;
            }

            solver.Add(this_sum + this_prod >= 1);
        }
    }

    /**
     *
     * KenKen puzzle.
     *
     * http://en.wikipedia.org/wiki/KenKen
     * """
     * KenKen or KEN-KEN is a style of arithmetic and logical puzzle sharing
     * several characteristics with sudoku. The name comes from Japanese and
     * is translated as 'square wisdom' or 'cleverness squared'.
     * ...
     * The objective is to fill the grid in with the digits 1 through 6 such that:
     *
     * * Each row contains exactly one of each digit
     * * Each column contains exactly one of each digit
     * * Each bold-outlined group of cells is a cage containing digits which
     *   achieve the specified result using the specified mathematical operation:
     *     addition (+),
     *     subtraction (-),
     *     multiplication (x),
     *     and division (/).
     *    (Unlike in Killer sudoku, digits may repeat within a group.)
     *
     * ...
     * More complex KenKen problems are formed using the principles described
     * above but omitting the symbols +, -, x and /, thus leaving them as
     * yet another unknown to be determined.
     * """
     *
     * The solution is:
     *
     *    5 6 3 4 1 2
     *    6 1 4 5 2 3
     *    4 5 2 3 6 1
     *    3 4 1 2 5 6
     *    2 3 6 1 4 5
     *    1 2 5 6 3 4
     *
     *
     * Also see http://www.hakank.org/or-tools/kenken2.py
     * though this C# model has another representation of
     * the problem instance.
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("KenKen2");

        // size of matrix
        int n = 6;
        IEnumerable<int> RANGE = Enumerable.Range(0, n);

        // For a better view of the problem, see
        //  http://en.wikipedia.org/wiki/File:KenKenProblem.svg

        // hints
        //  sum, the hints
        // Note: this is 1-based
        int[][] problem = { new int[] { 11, 1, 1, 2, 1 },
                            new int[] { 2, 1, 2, 1, 3 },
                            new int[] { 20, 1, 4, 2, 4 },
                            new int[] { 6, 1, 5, 1, 6, 2, 6, 3, 6 },
                            new int[] { 3, 2, 2, 2, 3 },
                            new int[] { 3, 2, 5, 3, 5 },
                            new int[] { 240, 3, 1, 3, 2, 4, 1, 4, 2 },
                            new int[] { 6, 3, 3, 3, 4 },
                            new int[] { 6, 4, 3, 5, 3 },
                            new int[] { 7, 4, 4, 5, 4, 5, 5 },
                            new int[] { 30, 4, 5, 4, 6 },
                            new int[] { 6, 5, 1, 5, 2 },
                            new int[] { 9, 5, 6, 6, 6 },
                            new int[] { 8, 6, 1, 6, 2, 6, 3 },
                            new int[] { 2, 6, 4, 6, 5 } };

        int num_p = problem.GetLength(0); // Number of segments

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(n, n, 1, n, "x");
        IntVar[] x_flat = x.Flatten();

        //
        // Constraints
        //

        //
        //  alldifferent rows and columns
        foreach (int i in RANGE)
        {
            // rows
            solver.Add((from j in RANGE select x[i, j]).ToArray().AllDifferent());

            // cols
            solver.Add((from j in RANGE select x[j, i]).ToArray().AllDifferent());
        }

        // Calculate the segments
        for (int i = 0; i < num_p; i++)
        {
            int[] segment = problem[i];

            // Remove the sum from the segment
            int len = segment.Length - 1;
            int[] s2 = new int[len];
            Array.Copy(segment, 1, s2, 0, len);

            // sum this segment
            calc(solver, s2, x, segment[0]);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
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
