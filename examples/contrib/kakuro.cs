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

public class Kakuro
{
    /**
     * Ensure that the sum of the segments
     * in cc == res
     *
     */
    public static void calc(Solver solver, int[] cc, IntVar[,] x, int res)
    {
        // ensure that the values are positive
        int len = cc.Length / 2;
        for (int i = 0; i < len; i++)
        {
            solver.Add(x[cc[i * 2] - 1, cc[i * 2 + 1] - 1] >= 1);
        }

        // sum the numbers
        solver.Add((from i in Enumerable.Range(0, len) select x[cc[i * 2] - 1, cc[i * 2 + 1] - 1]).ToArray().Sum() ==
                   res);
    }

    /**
     *
     * Kakuru puzzle.
     *
     * http://en.wikipedia.org/wiki/Kakuro
     * """
     * The object of the puzzle is to insert a digit from 1 to 9 inclusive
     * into each white cell such that the sum of the numbers in each entry
     * matches the clue associated with it and that no digit is duplicated in
     * any entry. It is that lack of duplication that makes creating Kakuro
     * puzzles with unique solutions possible, and which means solving a Kakuro
     * puzzle involves investigating combinations more, compared to Sudoku in
     * which the focus is on permutations. There is an unwritten rule for
     * making Kakuro puzzles that each clue must have at least two numbers
     * that add up to it. This is because including one number is mathematically
     * trivial when solving Kakuro puzzles; one can simply disregard the
     * number entirely and subtract it from the clue it indicates.
     * """
     *
     * This model solves the problem at the Wikipedia page.
     * For a larger picture, see
     * http://en.wikipedia.org/wiki/File:Kakuro_black_box.svg
     *
     * The solution:
     *  9 7 0 0 8 7 9
     *  8 9 0 8 9 5 7
     *  6 8 5 9 7 0 0
     *  0 6 1 0 2 6 0
     *  0 0 4 6 1 3 2
     *  8 9 3 1 0 1 4
     *  3 1 2 0 0 2 1
     *
     * Also see http://www.hakank.org/or-tools/kakuro.py
     * though this C# model has another representation of
     * the problem instance.
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Kakuro");

        // size of matrix
        int n = 7;

        // segments:
        //  sum, the segments
        // Note: this is 1-based
        int[][] problem = { new int[] { 16, 1, 1, 1, 2 },
                            new int[] { 24, 1, 5, 1, 6, 1, 7 },
                            new int[] { 17, 2, 1, 2, 2 },
                            new int[] { 29, 2, 4, 2, 5, 2, 6, 2, 7 },
                            new int[] { 35, 3, 1, 3, 2, 3, 3, 3, 4, 3, 5 },
                            new int[] { 7, 4, 2, 4, 3 },
                            new int[] { 8, 4, 5, 4, 6 },
                            new int[] { 16, 5, 3, 5, 4, 5, 5, 5, 6, 5, 7 },
                            new int[] { 21, 6, 1, 6, 2, 6, 3, 6, 4 },
                            new int[] { 5, 6, 6, 6, 7 },
                            new int[] { 6, 7, 1, 7, 2, 7, 3 },
                            new int[] { 3, 7, 6, 7, 7 },

                            new int[] { 23, 1, 1, 2, 1, 3, 1 },
                            new int[] { 30, 1, 2, 2, 2, 3, 2, 4, 2 },
                            new int[] { 27, 1, 5, 2, 5, 3, 5, 4, 5, 5, 5 },
                            new int[] { 12, 1, 6, 2, 6 },
                            new int[] { 16, 1, 7, 2, 7 },
                            new int[] { 17, 2, 4, 3, 4 },
                            new int[] { 15, 3, 3, 4, 3, 5, 3, 6, 3, 7, 3 },
                            new int[] { 12, 4, 6, 5, 6, 6, 6, 7, 6 },
                            new int[] { 7, 5, 4, 6, 4 },
                            new int[] { 7, 5, 7, 6, 7, 7, 7 },
                            new int[] { 11, 6, 1, 7, 1 },
                            new int[] { 10, 6, 2, 7, 2 }

        };

        int num_p = 24; // Number of segments

        // The blanks
        // Note: 1-based
        int[,] blanks = { { 1, 3 }, { 1, 4 }, { 2, 3 }, { 3, 6 }, { 3, 7 }, { 4, 1 }, { 4, 4 },
                          { 4, 7 }, { 5, 1 }, { 5, 2 }, { 6, 5 }, { 7, 4 }, { 7, 5 } };

        int num_blanks = blanks.GetLength(0);

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(n, n, 0, 9, "x");
        IntVar[] x_flat = x.Flatten();

        //
        // Constraints
        //

        // fill the blanks with 0
        for (int i = 0; i < num_blanks; i++)
        {
            solver.Add(x[blanks[i, 0] - 1, blanks[i, 1] - 1] == 0);
        }

        for (int i = 0; i < num_p; i++)
        {
            int[] segment = problem[i];

            // Remove the sum from the segment
            int[] s2 = new int[segment.Length - 1];
            for (int j = 1; j < segment.Length; j++)
            {
                s2[j - 1] = segment[j];
            }

            // sum this segment
            calc(solver, s2, x, segment[0]);

            // all numbers in this segment must be distinct
            int len = segment.Length / 2;
            solver.Add((from j in Enumerable.Range(0, len) select x[s2[j * 2] - 1, s2[j * 2 + 1] - 1])
                           .ToArray()
                           .AllDifferent());
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            for (int i = 0; i < n; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    int v = (int)x[i, j].Value();
                    if (v > 0)
                    {
                        Console.Write(v + " ");
                    }
                    else
                    {
                        Console.Write("  ");
                    }
                }
                Console.WriteLine();
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
        Solve();
    }
}
