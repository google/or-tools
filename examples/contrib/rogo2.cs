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
using System.IO;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class Rogo2
{
    static int W = 0;
    static int B = -1;

    // Default problem
    // Data from
    // Mike Trick: "Operations Research, Sudoko, Rogo, and Puzzles"
    // http://mat.tepper.cmu.edu/blog/?p=1302
    //
    // This has 48 solutions with symmetries;
    // 4 when the path symmetry is removed.
    //
    static int default_rows = 5;
    static int default_cols = 9;
    static int default_max_steps = 12;
    static int default_best = 8;
    static int[,] default_problem = { { 2, W, W, W, W, W, W, W, W },
                                      { W, 3, W, W, 1, W, W, 2, W },
                                      { W, W, W, W, W, W, B, W, 2 },
                                      { W, W, 2, B, W, W, W, W, W },
                                      { W, W, W, W, 2, W, W, 1, W } };
    static String default_problem_name = "Problem Mike Trick";

    // The actual problem
    static int rows;
    static int cols;
    static int max_steps;
    static int best;
    static int[,] problem;
    static string problem_name;

    /**
     *
     * Build the table of valid connections of the grid.
     *
     */
    public static IntTupleSet ValidConnections(int rows, int cols)
    {
        IEnumerable<int> ROWS = Enumerable.Range(0, rows);
        IEnumerable<int> COLS = Enumerable.Range(0, cols);
        var result_tmp =
            (from i1 in ROWS from j1 in COLS from i2 in ROWS from j2 in COLS where(Math.Abs(j1 - j2) == 1 &&
                                                                                   i1 == i2) ||
             (Math.Abs(i1 - i2) == 1 && j1 % cols == j2 % cols) select new int[] { i1 * cols + j1, i2 * cols + j2 })
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
     * Rogo puzzle solver.
     *
     * From http://www.rogopuzzle.co.nz/
     * """
     * The object is to collect the biggest score possible using a given
     * number of steps in a loop around a grid. The best possible score
     * for a puzzle is given with it, so you can easily check that you have
     * solved the puzzle. Rogo puzzles can also include forbidden squares,
     * which must be avoided in your loop.
     * """
     *
     * Also see Mike Trick:
     * "Operations Research, Sudoko, Rogo, and Puzzles"
     * http://mat.tepper.cmu.edu/blog/?p=1302
     *
     *
     * Also see, http://www.hakank.org/or-tools/rogo2.py
     * though this model differs in a couple of central points
     * which makes it much faster:
     *
     * - it use a table (
  AllowedAssignments) with the valid connections
     * - instead of two coordinates arrays, it use a single path array
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Rogo2");

        Console.WriteLine("\n");
        Console.WriteLine("**********************************************");
        Console.WriteLine("    {0}", problem_name);
        Console.WriteLine("**********************************************\n");

        //
        // Data
        //
        int B = -1;

        Console.WriteLine("Rows: {0} Cols: {1} Max Steps: {2}", rows, cols, max_steps);

        int[] problem_flatten = problem.Cast<int>().ToArray();
        int max_point = problem_flatten.Max();
        int max_sum = problem_flatten.Sum();
        Console.WriteLine("max_point: {0} max_sum: {1} best: {2}", max_point, max_sum, best);

        IEnumerable<int> STEPS = Enumerable.Range(0, max_steps);
        IEnumerable<int> STEPS1 = Enumerable.Range(0, max_steps - 1);

        // the valid connections, to be used with AllowedAssignments
        IntTupleSet valid_connections = ValidConnections(rows, cols);

        //
        // Decision variables
        //
        IntVar[] path = solver.MakeIntVarArray(max_steps, 0, rows * cols - 1, "path");
        IntVar[] points = solver.MakeIntVarArray(max_steps, 0, best, "points");
        IntVar sum_points = points.Sum().VarWithName("sum_points");

        //
        // Constraints
        //

        foreach (int s in STEPS)
        {
            // calculate the points (to maximize)
            solver.Add(points[s] == problem_flatten.Element(path[s]));

            // ensure that there are no black cells in
            // the path
            solver.Add(problem_flatten.Element(path[s]) != B);
        }

        solver.Add(path.AllDifferent());

        // valid connections
        foreach (int s in STEPS1)
        {
            solver.Add(new IntVar[] { path[s], path[s + 1] }.AllowedAssignments(valid_connections));
        }
        // around the corner
        solver.Add(new IntVar[] { path[max_steps - 1], path[0] }.AllowedAssignments(valid_connections));

        // Symmetry breaking
        for (int s = 1; s < max_steps; s++)
        {
            solver.Add(path[0] < path[s]);
        }

        //
        // Objective
        //
        OptimizeVar obj = sum_points.Maximize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(path, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("sum_points: {0}", sum_points.Value());
            Console.Write("path: ");
            foreach (int s in STEPS)
            {
                Console.Write("{0} ", path[s].Value());
            }
            Console.WriteLine();
            Console.WriteLine("(Adding 1 to coords...)");
            int[,] sol = new int[rows, cols];
            foreach (int s in STEPS)
            {
                int p = (int)path[s].Value();
                int x = (int)(p / cols);
                int y = (int)(p % cols);
                Console.WriteLine("{0,2},{1,2} ({2} points)", x + 1, y + 1, points[s].Value());
                sol[x, y] = 1;
            }
            Console.WriteLine("\nThe path is marked by 'X's:");
            for (int i = 0; i < rows; i++)
            {
                for (int j = 0; j < cols; j++)
                {
                    String p = sol[i, j] == 1 ? "X" : " ";
                    String q = problem[i, j] == B ? "B" : problem[i, j] == 0 ? "." : problem[i, j].ToString();
                    Console.Write("{0,2}{1} ", q, p);
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

    /**
     *
     * Reads a Rogo problem instance file.
     *
     * File format:
     *  # a comment which is ignored
     *  % a comment which also is ignored
     *  rows
     *  cols
     *  max_step
     *  best
     *  <data>
     *
     * Where <data> is a rows x cols matrix of
     *   digits (points), W (white), B (black)
     *
     * """
     * # comment
     * % another comment
     * 5
     * 9
     * 12
     * 8
     * 2 W W W W W W W W
     * W 3 W W 1 W W 2 W
     * W W W W W W B W 2
     * W W 2 B W W W W W
     * W W W W 2 W W 1 W
     * """
     *
     */

    private static void ReadFile(String file)
    {
        Console.WriteLine("readFile(" + file + ")");

        TextReader inr = new StreamReader(file);
        String str;
        int lineCount = 0;
        while ((str = inr.ReadLine()) != null && str.Length > 0)
        {
            str = str.Trim();

            Console.WriteLine(str);

            // ignore comments
            if (str.StartsWith("#") || str.StartsWith("%"))
            {
                continue;
            }

            if (lineCount == 0)
            {
                rows = Convert.ToInt32(str);
            }
            else if (lineCount == 1)
            {
                cols = Convert.ToInt32(str);
                problem = new int[rows, cols];
            }
            else if (lineCount == 2)
            {
                max_steps = Convert.ToInt32(str);
            }
            else if (lineCount == 3)
            {
                best = Convert.ToInt32(str);
            }
            else
            {
                String[] tmp = Regex.Split(str, "[,\\s]+");
                for (int j = 0; j < cols; j++)
                {
                    int val = 0;
                    if (tmp[j] == "B")
                    {
                        val = B;
                    }
                    else if (tmp[j] == "W")
                    {
                        val = W;
                    }
                    else
                    {
                        val = Convert.ToInt32(tmp[j]);
                    }
                    problem[lineCount - 4, j] = val;
                }
            }

            lineCount++;

        } // end while

        inr.Close();

    } // end readFile

    public static void Main(String[] args)
    {
        rows = default_rows;
        cols = default_cols;
        max_steps = default_max_steps;
        best = default_best;
        problem = default_problem;
        problem_name = default_problem_name;

        String file = "";

        if (args.Length > 0)
        {
            file = args[0];
            problem_name = "Problem " + file;
            ReadFile(file);
        }

        Solve();
    }
}
