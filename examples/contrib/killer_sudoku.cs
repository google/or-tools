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

public class KillerSudoku
{
    /**
     * Ensure that the sum of the segments
     * in cc == res
     *
     */
    public static void calc(Solver solver, int[] cc, IntVar[,] x, int res)
    {
        // sum the numbers
        int len = cc.Length / 2;
        solver.Add((from i in Enumerable.Range(0, len) select x[cc[i * 2] - 1, cc[i * 2 + 1] - 1]).ToArray().Sum() ==
                   res);
    }

    /**
     *
     * Killer Sudoku.
     *
     * http://en.wikipedia.org/wiki/Killer_Sudoku
     * """
     * Killer sudoku (also killer su doku, sumdoku, sum doku, addoku, or
     * samunamupure) is a puzzle that combines elements of sudoku and kakuro.
     * Despite the name, the simpler killer sudokus can be easier to solve
     * than regular sudokus, depending on the solver's skill at mental arithmetic;
     * the hardest ones, however, can take hours to crack.
     *
     * ...
     *
     * The objective is to fill the grid with numbers from 1 to 9 in a way that
     * the following conditions are met:
     *
     * - Each row, column, and nonet contains each number exactly once.
     * - The sum of all numbers in a cage must match the small number printed
     *   in its corner.
     * - No number appears more than once in a cage. (This is the standard rule
     *   for killer sudokus, and implies that no cage can include more
     *   than 9 cells.)
     *
     * In 'Killer X', an additional rule is that each of the long diagonals
     * contains each number once.
     * """
     *
     * Here we solve the problem from the Wikipedia page, also shown here
     * http://en.wikipedia.org/wiki/File:Killersudoku_color.svg
     *
     * The output is:
     *   2 1 5 6 4 7 3 9 8
     *   3 6 8 9 5 2 1 7 4
     *   7 9 4 3 8 1 6 5 2
     *   5 8 6 2 7 4 9 3 1
     *   1 4 2 5 9 3 8 6 7
     *   9 7 3 8 1 6 4 2 5
     *   8 2 1 7 3 9 5 4 6
     *   6 5 9 4 2 8 7 1 3
     *   4 3 7 1 6 5 2 8 9
     *
     * Also see http://www.hakank.org/or-tools/killer_sudoku.py
     * though this C# model has another representation of
     * the problem instance.
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("KillerSudoku");

        // size of matrix
        int cell_size = 3;
        IEnumerable<int> CELL = Enumerable.Range(0, cell_size);
        int n = cell_size * cell_size;
        IEnumerable<int> RANGE = Enumerable.Range(0, n);

        // For a better view of the problem, see
        //  http://en.wikipedia.org/wiki/File:Killersudoku_color.svg

        // hints
        //  sum, the hints
        // Note: this is 1-based
        int[][] problem = { new int[] { 3, 1, 1, 1, 2 },
                            new int[] { 15, 1, 3, 1, 4, 1, 5 },
                            new int[] { 22, 1, 6, 2, 5, 2, 6, 3, 5 },
                            new int[] { 4, 1, 7, 2, 7 },
                            new int[] { 16, 1, 8, 2, 8 },
                            new int[] { 15, 1, 9, 2, 9, 3, 9, 4, 9 },
                            new int[] { 25, 2, 1, 2, 2, 3, 1, 3, 2 },
                            new int[] { 17, 2, 3, 2, 4 },
                            new int[] { 9, 3, 3, 3, 4, 4, 4 },
                            new int[] { 8, 3, 6, 4, 6, 5, 6 },
                            new int[] { 20, 3, 7, 3, 8, 4, 7 },
                            new int[] { 6, 4, 1, 5, 1 },
                            new int[] { 14, 4, 2, 4, 3 },
                            new int[] { 17, 4, 5, 5, 5, 6, 5 },
                            new int[] { 17, 4, 8, 5, 7, 5, 8 },
                            new int[] { 13, 5, 2, 5, 3, 6, 2 },
                            new int[] { 20, 5, 4, 6, 4, 7, 4 },
                            new int[] { 12, 5, 9, 6, 9 },
                            new int[] { 27, 6, 1, 7, 1, 8, 1, 9, 1 },
                            new int[] { 6, 6, 3, 7, 2, 7, 3 },
                            new int[] { 20, 6, 6, 7, 6, 7, 7 },
                            new int[] { 6, 6, 7, 6, 8 },
                            new int[] { 10, 7, 5, 8, 4, 8, 5, 9, 4 },
                            new int[] { 14, 7, 8, 7, 9, 8, 8, 8, 9 },
                            new int[] { 8, 8, 2, 9, 2 },
                            new int[] { 16, 8, 3, 9, 3 },
                            new int[] { 15, 8, 6, 8, 7 },
                            new int[] { 13, 9, 5, 9, 6, 9, 7 },
                            new int[] { 17, 9, 8, 9, 9 }

        };

        int num_p = 29; // Number of segments

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(n, n, 0, 9, "x");
        IntVar[] x_flat = x.Flatten();

        //
        // Constraints
        //

        //
        // The first three constraints is the same as for sudokus.cs
        //
        //  alldifferent rows and columns
        foreach (int i in RANGE)
        {
            // rows
            solver.Add((from j in RANGE select x[i, j]).ToArray().AllDifferent());

            // cols
            solver.Add((from j in RANGE select x[j, i]).ToArray().AllDifferent());
        }

        // cells
        foreach (int i in CELL)
        {
            foreach (int j in CELL)
            {
                solver.Add((from di in CELL from dj in CELL select x[i * cell_size + di, j * cell_size + dj])
                               .ToArray()
                               .AllDifferent());
            }
        }

        // Sum the segments and ensure alldifferent
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
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

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
