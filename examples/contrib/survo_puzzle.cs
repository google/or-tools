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

public class SurvoPuzzle
{
    //
    // default problem
    //
    static int default_r = 3;
    static int default_c = 4;
    static int[] default_rowsums = { 30, 18, 30 };
    static int[] default_colsums = { 27, 16, 10, 25 };
    static int[,] default_game = { { 0, 6, 0, 0 }, { 8, 0, 0, 0 }, { 0, 0, 3, 0 } };

    // for the actual problem
    static int r;
    static int c;
    static int[] rowsums;
    static int[] colsums;
    static int[,] game;

    /**
     *
     * Solves the Survo puzzle.
     * See http://www.hakank.org/or-tools/survo_puzzle.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("SurvoPuzzle");

        //
        // data
        //
        Console.WriteLine("Problem:");
        for (int i = 0; i < r; i++)
        {
            for (int j = 0; j < c; j++)
            {
                Console.Write(game[i, j] + " ");
            }
            Console.WriteLine();
        }
        Console.WriteLine();

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(r, c, 1, r * c, "x");
        IntVar[] x_flat = x.Flatten();

        //
        // Constraints
        //
        for (int i = 0; i < r; i++)
        {
            for (int j = 0; j < c; j++)
            {
                if (game[i, j] > 0)
                {
                    solver.Add(x[i, j] == game[i, j]);
                }
            }
        }

        solver.Add(x_flat.AllDifferent());

        //
        // calculate rowsums and colsums
        //
        for (int i = 0; i < r; i++)
        {
            IntVar[] row = new IntVar[c];
            for (int j = 0; j < c; j++)
            {
                row[j] = x[i, j];
            }
            solver.Add(row.Sum() == rowsums[i]);
        }

        for (int j = 0; j < c; j++)
        {
            IntVar[] col = new IntVar[r];
            for (int i = 0; i < r; i++)
            {
                col[i] = x[i, j];
            }
            solver.Add(col.Sum() == colsums[j]);
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
            for (int i = 0; i < r; i++)
            {
                for (int j = 0; j < c; j++)
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
     * readFile()
     *
     * Reads a Survo puzzle in the following format
     * r
     * c
     * rowsums
     * olsums
     * data
     * ...
     *
     * Example:
     * 3
     * 4
     * 30,18,30
     * 27,16,10,25
     * 0,6,0,0
     * 8,0,0,0
     * 0,0,3,0
     *
     */
    private static void readFile(String file)
    {
        Console.WriteLine("readFile(" + file + ")");

        TextReader inr = new StreamReader(file);

        r = Convert.ToInt32(inr.ReadLine());
        c = Convert.ToInt32(inr.ReadLine());
        rowsums = new int[r];
        colsums = new int[c];
        Console.WriteLine("r: " + r + " c: " + c);

        String[] rowsums_str = Regex.Split(inr.ReadLine(), ",\\s*");
        String[] colsums_str = Regex.Split(inr.ReadLine(), ",\\s*");
        Console.WriteLine("rowsums:");
        for (int i = 0; i < r; i++)
        {
            Console.Write(rowsums_str[i] + " ");
            rowsums[i] = Convert.ToInt32(rowsums_str[i]);
        }
        Console.WriteLine("\ncolsums:");
        for (int j = 0; j < c; j++)
        {
            Console.Write(colsums_str[j] + " ");
            colsums[j] = Convert.ToInt32(colsums_str[j]);
        }
        Console.WriteLine();

        // init the game matrix and read data from file
        game = new int[r, c];
        String str;
        int line_count = 0;
        while ((str = inr.ReadLine()) != null && str.Length > 0)
        {
            str = str.Trim();

            // ignore comments
            // starting with either # or %
            if (str.StartsWith("#") || str.StartsWith("%"))
            {
                continue;
            }

            String[] this_row = Regex.Split(str, ",\\s*");
            for (int j = 0; j < this_row.Length; j++)
            {
                game[line_count, j] = Convert.ToInt32(this_row[j]);
            }

            line_count++;

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
            r = default_r;
            c = default_c;
            game = default_game;
            rowsums = default_rowsums;
            colsums = default_colsums;
        }

        Solve();
    }
}
