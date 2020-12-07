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

public class FillAPix
{
    static int X = -1;

    //
    // Default problem.
    // Puzzle 1 from
    // http://www.conceptispuzzles.com/index.aspx?uri=puzzle/fill-a-pix/rules

    //
    static int default_n = 10;
    static int[,] default_puzzle = { { X, X, X, X, X, X, X, X, 0, X }, { X, 8, 8, X, 2, X, 0, X, X, X },
                                     { 5, X, 8, X, X, X, X, X, X, X }, { X, X, X, X, X, 2, X, X, X, 2 },
                                     { 1, X, X, X, 4, 5, 6, X, X, X }, { X, 0, X, X, X, 7, 9, X, X, 6 },
                                     { X, X, X, 6, X, X, 9, X, X, 6 }, { X, X, 6, 6, 8, 7, 8, 7, X, 5 },
                                     { X, 4, X, 6, 6, 6, X, 6, X, 4 }, { X, X, X, X, X, X, 3, X, X, X } };

    // for the actual problem
    static int n;
    static int[,] puzzle;

    /**
     *
     * Fill-a-Pix problem
     *
     * From
     * http://www.conceptispuzzles.com/index.aspx?uri=puzzle/fill-a-pix/basiclogic
     * """
     * Each puzzle consists of a grid containing clues in various places. The
     * object is to reveal a hidden picture by painting the squares around each
     * clue so that the number of painted squares, including the square with
     * the clue, matches the value of the clue.
     * """
     *
     * http://www.conceptispuzzles.com/index.aspx?uri=puzzle/fill-a-pix/rules
     * """
     * Fill-a-Pix is a Minesweeper-like puzzle based on a grid with a pixilated
     * picture hidden inside. Using logic alone, the solver determines which
     * squares are painted and which should remain empty until the hidden picture
     * is completely exposed.
     * """
     *
     * Fill-a-pix History:
     * http://www.conceptispuzzles.com/index.aspx?uri=puzzle/fill-a-pix/history
     *
     * Also see http://www.hakank.org/google_or_tools/fill_a_pix.py
     *
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("FillAPix");

        //
        // data
        //
        int[] S = { -1, 0, 1 };

        Console.WriteLine("Problem:");
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (puzzle[i, j] > X)
                {
                    Console.Write(puzzle[i, j] + " ");
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
        IntVar[,] pict = solver.MakeIntVarMatrix(n, n, 0, 1, "pict");
        IntVar[] pict_flat = pict.Flatten(); // for branching

        //
        // Constraints
        //
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (puzzle[i, j] > X)
                {
                    // this cell is the sum of all surrounding cells
                    var tmp = from a in S from b in S
                                      where i +
  a >= 0 && j + b >= 0 && i + a<n && j + b<n select(pict[i + a, j + b]);

                    solver.Add(tmp.ToArray().Sum() == puzzle[i, j]);
                }
            }
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(pict_flat, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

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
                    Console.Write(pict[i, j].Value() == 1 ? "#" : " ");
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
     * Reads a Fill-a-Pix file.
     * File format:
     *  # a comment which is ignored
     *  % a comment which also is ignored
     *  number of rows and columns (n x n)
     *  <
     *    row number of neighbours lines...
     *  >
     *
     * 0..8 means number of neighbours, "." mean unknown (may be a mine)
     *
     * Example (from fill_a_pix1.txt):
     *
     * 10
     * ........0.
     * .88.2.0...
     * 5.8.......
     * .....2...2
     * 1...456...
     * .0...79..6
     * ...6..9..6
     * ..668787.5
     * .4.666.6.4
     * ......3...
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
                puzzle = new int[n, n];
            }
            else
            {
                // the problem matrix
                String[] row = Regex.Split(str, "");
                for (int j = 1; j <= n; j++)
                {
                    String s = row[j];
                    if (s.Equals("."))
                    {
                        puzzle[lineCount - 1, j - 1] = -1;
                    }
                    else
                    {
                        puzzle[lineCount - 1, j - 1] = Convert.ToInt32(s);
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
            puzzle = default_puzzle;
            n = default_n;
        }

        Solve();
    }
}
