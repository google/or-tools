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

public class Minesweeper
{
    static int X = -1;

    //
    // Default problem.
    // It has 4 solutions.
    //
    static int default_r = 8;
    static int default_c = 8;
    static int[,] default_game = { { 2, 3, X, 2, 2, X, 2, 1 }, { X, X, 4, X, X, 4, X, 2 }, { X, X, X, X, X, X, 4, X },
                                   { X, 5, X, 6, X, X, X, 2 }, { 2, X, X, X, 5, 5, X, 2 }, { 1, 3, 4, X, X, X, 4, X },
                                   { 0, 1, X, 4, X, X, X, 3 }, { 0, 1, 2, X, 2, 3, X, 2 } };

    // for the actual problem
    static int r;
    static int c;
    static int[,] game;

    /**
     *
     * Solves the Minesweeper problems.
     *
     * See http://www.hakank.org/google_or_tools/minesweeper.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Minesweeper");

        //
        // data
        //
        int[] S = { -1, 0, 1 };

        Console.WriteLine("Problem:");
        for (int i = 0; i < r; i++)
        {
            for (int j = 0; j < c; j++)
            {
                if (game[i, j] > X)
                {
                    Console.Write(game[i, j] + " ");
                }
                else
                {
                    Console.Write("X ");
                }
            }
            Console.WriteLine();
        }
        Console.WriteLine();

        //
        // Decision variables
        //
        IntVar[,] mines = solver.MakeIntVarMatrix(r, c, 0, 1, "mines");
        // for branching
        IntVar[] mines_flat = mines.Flatten();

        //
        // Constraints
        //
        for (int i = 0; i < r; i++)
        {
            for (int j = 0; j < c; j++)
            {
                if (game[i, j] >= 0)
                {
                    solver.Add(mines[i, j] == 0);

                    // this cell is the sum of all its neighbours
                    var tmp = from a in S from b in S
                                      where i +
  a >= 0 && j + b >= 0 && i + a<r && j + b<c select(mines[i + a, j + b]);

                    solver.Add(tmp.ToArray().Sum() == game[i, j]);
                }

                if (game[i, j] > X)
                {
                    // This cell cannot be a mine since it
                    // has some value assigned to it
                    solver.Add(mines[i, j] == 0);
                }
            }
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(mines_flat, Solver.CHOOSE_PATH, Solver.ASSIGN_MIN_VALUE);

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
                    Console.Write("{0} ", mines[i, j].Value());
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
     * Reads a minesweeper file.
     * File format:
     *  # a comment which is ignored
     *  % a comment which also is ignored
     *  number of rows
     *  number of columns
     *  <
     *    row number of neighbours lines...
     *  >
     *
     * 0..8 means number of neighbours, "." mean unknown (may be a mine)
     *
     * Example (from minesweeper0.txt)
     * # Problem from Gecode/examples/minesweeper.cc  problem 0
     * 6
     * 6
     * ..2.3.
     * 2.....
     * ..24.3
     * 1.34..
     * .....3
     * .3.3..
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
                r = Convert.ToInt32(str); // number of rows
            }
            else if (lineCount == 1)
            {
                c = Convert.ToInt32(str); // number of columns
                game = new int[r, c];
            }
            else
            {
                // the problem matrix
                String[] row = Regex.Split(str, "");
                for (int j = 1; j <= c; j++)
                {
                    String s = row[j];
                    if (s.Equals("."))
                    {
                        game[lineCount - 2, j - 1] = -1;
                    }
                    else
                    {
                        game[lineCount - 2, j - 1] = Convert.ToInt32(s);
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
            game = default_game;
            r = default_r;
            c = default_c;
        }

        Solve();
    }
}
