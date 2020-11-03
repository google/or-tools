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
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class QuasigroupCompletion
{
    static int X = 0;

    /*
     * default problem
     *
     * Example from Ruben Martins and Inès Lynce
     * Breaking Local Symmetries in Quasigroup Completion Problems, page 3
     * The solution is unique:
     *
     *   1 3 2 5 4
     *   2 5 4 1 3
     *   4 1 3 2 5
     *   5 4 1 3 2
     *   3 2 5 4 1
     */
    static int default_n = 5;
    static int[,] default_problem = {
        { 1, X, X, X, 4 }, { X, 5, X, X, X }, { 4, X, X, 2, X }, { X, 4, X, X, X }, { X, X, 5, X, 1 }
    };

    // for the actual problem
    static int n;
    static int[,] problem;

    /**
     *
     * Solves the Quasigroup Completion problem.
     * See http://www.hakank.org/or-tools/quasigroup_completion.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("QuasigroupCompletion");

        //
        // data
        //
        Console.WriteLine("Problem:");
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                Console.Write(problem[i, j] + " ");
            }
            Console.WriteLine();
        }
        Console.WriteLine();

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(n, n, 1, n, "x");
        IntVar[] x_flat = x.Flatten();

        //
        // Constraints
        //
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (problem[i, j] > X)
                {
                    solver.Add(x[i, j] == problem[i, j]);
                }
            }
        }

        //
        // rows and columns must be different
        //

        // rows
        for (int i = 0; i < n; i++)
        {
            IntVar[] row = new IntVar[n];
            for (int j = 0; j < n; j++)
            {
                row[j] = x[i, j];
            }
            solver.Add(row.AllDifferent());
        }

        // columns
        for (int j = 0; j < n; j++)
        {
            IntVar[] col = new IntVar[n];
            for (int i = 0; i < n; i++)
            {
                col[i] = x[i, j];
            }
            solver.Add(col.AllDifferent());
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.INT_VAR_SIMPLE, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        int sol = 0;
        while (solver.NextSolution())
        {
            sol++;
            Console.WriteLine("Solution #{0} ", sol + " ");
            for (int i = 0; i < n; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    Console.Write("{0} ", x[i, j].Value());
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
     * Reads a Quasigroup completion file.
     * File format:
     *  # a comment which is ignored
     *  % a comment which also is ignored
     *  number of rows (n)
     *  <
     *    row number of space separated entries
     *  >
     *
     * "." or "0" means unknown, integer 1..n means known value
     *
     * Example
     *   5
     *    1 . . . 4
     *   . 5 . . .
     *   4 . . 2 .
     *   . 4 . . .
     *   . . 5 . 1
     *
     */
    private static void readFile(String file)
    {
        Console.WriteLine("readFile(" + file + ")");
        int lineCount = 0;

        TextReader inr = new StreamReader(file);
        String str;
        while ((str = inr.ReadLine()) != null && str.Length > 0)
        {
            str = str.Trim();

            // ignore comments
            if (str.StartsWith("#") || str.StartsWith("%"))
            {
                continue;
            }

            Console.WriteLine(str);
            if (lineCount == 0)
            {
                n = Convert.ToInt32(str); // number of rows
                problem = new int[n, n];
            }
            else
            {
                // the problem matrix
                String[] row = Regex.Split(str, " ");
                for (int i = 0; i < n; i++)
                {
                    String s = row[i];
                    if (s.Equals("."))
                    {
                        problem[lineCount - 1, i] = 0;
                    }
                    else
                    {
                        problem[lineCount - 1, i] = Convert.ToInt32(s);
                    }
                }
            }

            lineCount++;

        } // end while

        inr.Close();

    } // end readFile

    public static void Main(String[] args)
    {
        String file = "";
        if (args.Length > 0)
        {
            file = args[0];
            readFile(file);
        }
        else
        {
            problem = default_problem;
            n = default_n;
        }

        Solve();
    }
}
