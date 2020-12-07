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
using System.Linq;
using Google.OrTools.ConstraintSolver;

public class HidatoTable
{
    /*
     * Build closeness pairs for consecutive numbers.
     *
     * Build set of allowed pairs such that two consecutive numbers touch
     * each other in the grid.
     *
     * Returns:
     * A list of pairs for allowed consecutive position of numbers.
     *
     * Args:
     * rows: the number of rows in the grid
     *  cols: the number of columns in the grid
     */
    public static IntTupleSet BuildPairs(int rows, int cols)
    {
        int[] ix = { -1, 0, 1 };
    var result_tmp =
        (from x in Enumerable.Range(0, rows) from y in Enumerable.Range(0, cols)
                     from dx in ix from dy in ix where x +
                 dx >=
             0 &&
         x + dx < rows && y + dy >= 0 && y + dy < cols &&
         (dx != 0 || dy != 0)
             select new int[]{x * cols + y, (x + dx) * cols + (y + dy)})
            .ToArray();

    // Convert to len x 2 matrix
    int len = result_tmp.Length;
    IntTupleSet result = new IntTupleSet(2);
    foreach (int[] r in result_tmp)
    {
        result.Insert(r);
    }
    return result;
    }

    /**
     *
     * Hidato puzzle in Google CP Solver.
     *
     * http://www.hidato.com/
     * """
     * Puzzles start semi-filled with numbered tiles.
     * The first and last numbers are circled.
     * Connect the numbers together to win. Consecutive
     * number must touch horizontally, vertically, or
     * diagonally.
     * """
     *
     * This is a port of the Python model hidato_table.py
     * made by Laurent Perron (using AllowedAssignments),
     * based on my (much slower) model hidato.py.
     *
     */
    private static void Solve(int model = 1)
    {
        Solver solver = new Solver("HidatoTable");

        //
        // models, a 0 indicates an open cell which number is not yet known.
        //

        int[,] puzzle = null;
        if (model == 1)
        {
            // Simple problem

            // Solution 1:
            // 6  7  9
            // 5  2  8
            // 1  4  3
            int[,] puzzle1 = { { 6, 0, 9 }, { 0, 2, 8 }, { 1, 0, 0 } };
            puzzle = puzzle1;
        }
        else if (model == 2)
        {
            int[,] puzzle2 = { { 0, 44, 41, 0, 0, 0, 0 },  { 0, 43, 0, 28, 29, 0, 0 }, { 0, 1, 0, 0, 0, 33, 0 },
                               { 0, 2, 25, 4, 34, 0, 36 }, { 49, 16, 0, 23, 0, 0, 0 }, { 0, 19, 0, 0, 12, 7, 0 },
                               { 0, 0, 0, 14, 0, 0, 0 } };
            puzzle = puzzle2;
        }
        else if (model == 3)
        {
            // Problems from the book:
            // Gyora Bededek: "Hidato: 2000 Pure Logic Puzzles"
            // Problem 1 (Practice)
            int[,] puzzle3 = {
                { 0, 0, 20, 0, 0 }, { 0, 0, 0, 16, 18 }, { 22, 0, 15, 0, 0 }, { 23, 0, 1, 14, 11 }, { 0, 25, 0, 0, 12 }
            };
            puzzle = puzzle3;
        }
        else if (model == 4)
        {
            // problem 2 (Practice)
            int[,] puzzle4 = {
                { 0, 0, 0, 0, 14 }, { 0, 18, 12, 0, 0 }, { 0, 0, 17, 4, 5 }, { 0, 0, 7, 0, 0 }, { 9, 8, 25, 1, 0 }
            };
            puzzle = puzzle4;
        }
        else if (model == 5)
        {
            // problem 3 (Beginner)
            int[,] puzzle5 = { { 0, 26, 0, 0, 0, 18 }, { 0, 0, 27, 0, 0, 19 }, { 31, 23, 0, 0, 14, 0 },
                               { 0, 33, 8, 0, 15, 1 }, { 0, 0, 0, 5, 0, 0 },   { 35, 36, 0, 10, 0, 0 } };
            puzzle = puzzle5;
        }
        else if (model == 6)
        {
            // Problem 15 (Intermediate)
            int[,] puzzle6 = { { 64, 0, 0, 0, 0, 0, 0, 0 },   { 1, 63, 0, 59, 15, 57, 53, 0 },
                               { 0, 4, 0, 14, 0, 0, 0, 0 },   { 3, 0, 11, 0, 20, 19, 0, 50 },
                               { 0, 0, 0, 0, 22, 0, 48, 40 }, { 9, 0, 0, 32, 23, 0, 0, 41 },
                               { 27, 0, 0, 0, 36, 0, 46, 0 }, { 28, 30, 0, 35, 0, 0, 0, 0 } };
            puzzle = puzzle6;
        }

        int r = puzzle.GetLength(0);
        int c = puzzle.GetLength(1);

        Console.WriteLine();
        Console.WriteLine("----- Solving problem {0} -----", model);
        Console.WriteLine();

        PrintMatrix(puzzle);

        //
        // Decision variables
        //
        IntVar[] positions = solver.MakeIntVarArray(r * c, 0, r * c - 1, "p");

        //
        // Constraints
        //
        solver.Add(positions.AllDifferent());

        //
        // Fill in the clues
        //
        for (int i = 0; i < r; i++)
        {
            for (int j = 0; j < c; j++)
            {
                if (puzzle[i, j] > 0)
                {
                    solver.Add(positions[puzzle[i, j] - 1] == i * c + j);
                }
            }
        }

        // Consecutive numbers much touch each other in the grid.
        // We use an allowed assignment constraint to model it.
        IntTupleSet close_tuples = BuildPairs(r, c);
        for (int k = 1; k < r * c - 1; k++)
        {
            IntVar[] tmp = new IntVar[] { positions[k], positions[k + 1] };
            solver.Add(tmp.AllowedAssignments(close_tuples));
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(positions, Solver.CHOOSE_MIN_SIZE_LOWEST_MIN, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        int num_solution = 0;
        while (solver.NextSolution())
        {
            num_solution++;
            PrintOneSolution(positions, r, c, num_solution);
        }

        Console.WriteLine("\nSolutions: " + solver.Solutions());
        Console.WriteLine("WallTime: " + solver.WallTime() + "ms ");
        Console.WriteLine("Failures: " + solver.Failures());
        Console.WriteLine("Branches: " + solver.Branches());

        solver.EndSearch();
    }

    // Print the current solution
    public static void PrintOneSolution(IntVar[] positions, int rows, int cols, int num_solution)
    {
        Console.WriteLine("Solution {0}", num_solution);

        // Create empty board
        int[,] board = new int[rows, cols];
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
                board[i, j] = 0;
            }
        }

        // Fill board with solution value
        for (int k = 0; k < rows * cols; k++)
        {
            int position = (int)positions[k].Value();
            board[position / cols, position % cols] = k + 1;
        }

        PrintMatrix(board);
    }

    // Pretty print of the matrix
    public static void PrintMatrix(int[,] game)
    {
        int rows = game.GetLength(0);
        int cols = game.GetLength(1);

        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
                if (game[i, j] == 0)
                {
                    Console.Write("  .");
                }
                else
                {
                    Console.Write(" {0,2}", game[i, j]);
                }
            }
            Console.WriteLine();
        }
        Console.WriteLine();
    }

    public static void Main(String[] args)
    {
        int model = 1;
        if (args.Length > 0)
        {
            model = Convert.ToInt32(args[0]);
            Solve(model);
        }
        else
        {
            for (int m = 1; m <= 6; m++)
            {
                Solve(m);
            }
        }
    }
}
