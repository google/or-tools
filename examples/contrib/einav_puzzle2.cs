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

public class EinavPuzzle2
{
    /**
     *
     * A programming puzzle from Einav.
     *
     * From
     * "A programming puzzle from Einav"
     * http://gcanyon.wordpress.com/2009/10/28/a-programming-puzzle-from-einav/
     * """
     * My friend Einav gave me this programming puzzle to work on. Given
     * this array of positive and negative numbers:
     * 33   30  -10 -6  18   7  -11 -23   6
     * ...
     * -25   4  16  30  33 -23  -4   4 -23
     *
     * You can flip the sign of entire rows and columns, as many of them
     * as you like. The goal is to make all the rows and columns sum to positive
     * numbers (or zero), and then to find the solution (there are more than one)
     * that has the smallest overall sum. So for example, for this array:
     * 33  30 -10
     * -16  19   9
     * -17 -12 -14
     * You could flip the sign for the bottom row to get this array:
     * 33  30 -10
     * -16  19   9
     * 17  12  14
     * Now all the rows and columns have positive sums, and the overall total is
     * 108.
     * But you could instead flip the second and third columns, and the second
     * row, to get this array:
     * 33  -30  10
     * 16   19    9
     * -17   12   14
     * All the rows and columns still total positive, and the overall sum is just
     * 66. So this solution is better (I don't know if it's the best)
     * A pure brute force solution would have to try over 30 billion solutions.
     * I wrote code to solve this in J. I'll post that separately.
     * """
     *
     * Note:
     * This is a port of Larent Perrons's Python version of my own
     * einav_puzzle.py. He removed some of the decision variables and made it more
     * efficient. Thanks!
     *
     * Also see http://www.hakank.org/or-tools/einav_puzzle2.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("EinavPuzzle2");

        //
        // Data
        //

        // Small problem
        // int rows = 3;
        // int cols = 3;
        // int[,] data = {
        //   { 33,  30, -10},
        //   {-16,  19,   9},
        //   {-17, -12, -14}
        // };

        // Full problem
        int rows = 27;
        int cols = 9;
        int[,] data = { { 33, 30, 10, -6, 18, -7, -11, 23, -6 },     { 16, -19, 9, -26, -8, -19, -8, -21, -14 },
                        { 17, 12, -14, 31, -30, 13, -13, 19, 16 },   { -6, -11, 1, 17, -12, -4, -7, 14, -21 },
                        { 18, -31, 34, -22, 17, -19, 20, 24, 6 },    { 33, -18, 17, -15, 31, -5, 3, 27, -3 },
                        { -18, -20, -18, 31, 6, 4, -2, -12, 24 },    { 27, 14, 4, -29, -3, 5, -29, 8, -12 },
                        { -15, -7, -23, 23, -9, -8, 6, 8, -12 },     { 33, -23, -19, -4, -8, -7, 11, -12, 31 },
                        { -20, 19, -15, -30, 11, 32, 7, 14, -5 },    { -23, 18, -32, -2, -31, -7, 8, 24, 16 },
                        { 32, -4, -10, -14, -6, -1, 0, 23, 23 },     { 25, 0, -23, 22, 12, 28, -27, 15, 4 },
                        { -30, -13, -16, -3, -3, -32, -3, 27, -31 }, { 22, 1, 26, 4, -2, -13, 26, 17, 14 },
                        { -9, -18, 3, -20, -27, -32, -11, 27, 13 },  { -17, 33, -7, 19, -32, 13, -31, -2, -24 },
                        { -31, 27, -31, -29, 15, 2, 29, -15, 33 },   { -18, -23, 15, 28, 0, 30, -4, 12, -32 },
                        { -3, 34, 27, -25, -18, 26, 1, 34, 26 },     { -21, -31, -10, -13, -30, -17, -12, -26, 31 },
                        { 23, -31, -19, 21, -17, -10, 2, -23, 23 },  { -3, 6, 0, -3, -32, 0, -10, -25, 14 },
                        { -19, 9, 14, -27, 20, 15, -5, -27, 18 },    { 11, -6, 24, 7, -17, 26, 20, -31, -25 },
                        { -25, 4, -16, 30, 33, 23, -4, -4, 23 } };

        IEnumerable<int> ROWS = Enumerable.Range(0, rows);
        IEnumerable<int> COLS = Enumerable.Range(0, cols);

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(rows, cols, -100, 100, "x");
        IntVar[] x_flat = x.Flatten();

        int[] signs_domain = { -1, 1 };
        // This don't work at the moment...
        IntVar[] row_signs = solver.MakeIntVarArray(rows, signs_domain, "row_signs");
        IntVar[] col_signs = solver.MakeIntVarArray(cols, signs_domain, "col_signs");

        // To optimize
        IntVar total_sum = x_flat.Sum().VarWithName("total_sum");

        //
        // Constraints
        //
        foreach (int i in ROWS)
        {
            foreach (int j in COLS)
            {
                solver.Add(x[i, j] == data[i, j] * row_signs[i] * col_signs[j]);
            }
        }

        // row sums
        IntVar[] row_sums = (from i in ROWS select(from j in COLS select x[i, j]).ToArray().Sum().Var()).ToArray();

        foreach (int i in ROWS)
        {
            row_sums[i].SetMin(0);
        }

        // col sums
        IntVar[] col_sums = (from j in COLS select(from i in ROWS select x[i, j]).ToArray().Sum().Var()).ToArray();

        foreach (int j in COLS)
        {
            col_sums[j].SetMin(0);
        }

        //
        // Objective
        //
        OptimizeVar obj = total_sum.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(col_signs.Concat(row_signs).ToArray(), Solver.CHOOSE_MIN_SIZE_LOWEST_MIN,
                                              Solver.ASSIGN_MAX_VALUE);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("Sum: {0}", total_sum.Value());
            Console.Write("row_sums: ");
            foreach (int i in ROWS)
            {
                Console.Write(row_sums[i].Value() + " ");
            }
            Console.Write("\nrow_signs: ");
            foreach (int i in ROWS)
            {
                Console.Write(row_signs[i].Value() + " ");
            }

            Console.Write("\ncol_sums: ");
            foreach (int j in COLS)
            {
                Console.Write(col_sums[j].Value() + " ");
            }
            Console.Write("\ncol_signs: ");
            foreach (int j in COLS)
            {
                Console.Write(col_signs[j].Value() + " ");
            }
            Console.WriteLine("\n");
            foreach (int i in ROWS)
            {
                foreach (int j in COLS)
                {
                    Console.Write("{0,3} ", x[i, j].Value());
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
