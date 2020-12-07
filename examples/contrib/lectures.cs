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

public class Lectures
{
    /**
     *
     * Lectures problem in Google CP Solver.
     *
     * Biggs: Discrete Mathematics (2nd ed), page 187.
     * """
     * Suppose we wish to schedule six one-hour lectures, v1, v2, v3, v4, v5, v6.
     * Among the the potential audience there are people who wish to hear both
     *
     * - v1 and v2
     * - v1 and v4
     * - v3 and v5
     * - v2 and v6
     * - v4 and v5
     * - v5 and v6
     * - v1 and v6
     *
     * How many hours are necessary in order that the lectures can be given
     * without clashes?
     * """
     *
     * Note: This can be seen as a coloring problem.
     *
     * Also see http://www.hakank.org/or-tools/lectures.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Lectures");

        //
        // The schedule requirements:
        // lecture a cannot be held at the same time as b
        // Note: 1-based (compensated in the constraints).
        int[,] g = { { 1, 2 }, { 1, 4 }, { 3, 5 }, { 2, 6 }, { 4, 5 }, { 5, 6 }, { 1, 6 } };

        // number of nodes
        int n = 6;

        // number of edges
        int edges = g.GetLength(0);

        //
        // Decision variables
        //
        //
        // declare variables
        //
        IntVar[] v = solver.MakeIntVarArray(n, 0, n - 1, "v");

        // Maximum color (hour) to minimize.
        // Note: since C# is 0-based, the
        // number of colors is max_c+1.
        IntVar max_c = v.Max().VarWithName("max_c");

        //
        // Constraints
        //

        // Ensure that there are no clashes
        // also, adjust to 0-base.
        for (int i = 0; i < edges; i++)
        {
            solver.Add(v[g[i, 0] - 1] != v[g[i, 1] - 1]);
        }

        // Symmetry breaking:
        // - v0 has the color 0,
        // - v1 has either color 0 or 1
        solver.Add(v[0] == 0);
        solver.Add(v[1] <= 1);

        //
        // Objective
        //
        OptimizeVar obj = max_c.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(v, Solver.CHOOSE_MIN_SIZE_LOWEST_MIN, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("\nmax hours: {0}", max_c.Value() + 1);
            Console.WriteLine("v: " +
                              String.Join(" ", (from i in Enumerable.Range(0, n) select v[i].Value()).ToArray()));
            for (int i = 0; i < n; i++)
            {
                Console.WriteLine("Lecture {0} at {1}h", i, v[i].Value());
            }
            Console.WriteLine("\n");
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
        Solve();
    }
}
