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

public class DiscreteTomography
{
    // default problem
    static int[] default_rowsums = { 0, 0, 8, 2, 6, 4, 5, 3, 7, 0, 0 };
    static int[] default_colsums = { 0, 0, 7, 1, 6, 3, 4, 5, 2, 7, 0, 0 };

    static int[] rowsums2;
    static int[] colsums2;

    /**
     *
     * Discrete tomography
     *
     * Problem from http://eclipse.crosscoreop.com/examples/tomo.ecl.txt
     * """
     * This is a little 'tomography' problem, taken from an old issue
     * of Scientific American.
     *
     * A matrix which contains zeroes and ones gets "x-rayed" vertically and
     * horizontally, giving the total number of ones in each row and column.
     * The problem is to reconstruct the contents of the matrix from this
     * information. Sample run:
     *
     * ?- go.
     *    0 0 7 1 6 3 4 5 2 7 0 0
     * 0
     * 0
     * 8      * * * * * * * *
     * 2      *             *
     * 6      *   * * * *   *
     * 4      *   *     *   *
     * 5      *   *   * *   *
     * 3      *   *         *
     * 7      *   * * * * * *
     * 0
     * 0
     *
     * Eclipse solution by Joachim Schimpf, IC-Parc
     * """
     *
     * See  http://www.hakank.org/or-tools/discrete_tomography.py
     *
     */
    private static void Solve(int[] rowsums, int[] colsums)
    {
        Solver solver = new Solver("DiscreteTomography");

        //
        // Data
        //
        int r = rowsums.Length;
        int c = colsums.Length;

        Console.Write("rowsums: ");
        for (int i = 0; i < r; i++)
        {
            Console.Write(rowsums[i] + " ");
        }
        Console.Write("\ncolsums: ");
        for (int j = 0; j < c; j++)
        {
            Console.Write(colsums[j] + " ");
        }
        Console.WriteLine("\n");

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(r, c, 0, 1, "x");
        IntVar[] x_flat = x.Flatten();

        //
        // Constraints
        //

        // row sums
        for (int i = 0; i < r; i++)
        {
            var tmp = from j in Enumerable.Range(0, c) select x[i, j];
            solver.Add(tmp.ToArray().Sum() == rowsums[i]);
        }

        // cols sums
        for (int j = 0; j < c; j++)
        {
            var tmp = from i in Enumerable.Range(0, r) select x[i, j];
            solver.Add(tmp.ToArray().Sum() == colsums[j]);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            for (int i = 0; i < r; i++)
            {
                for (int j = 0; j < c; j++)
                {
                    Console.Write("{0} ", x[i, j].Value() == 1 ? "#" : ".");
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
     * Reads a discrete tomography file.
     * File format:
     *  # a comment which is ignored
     *  % a comment which also is ignored
     *  rowsums separated by [,\s]
     *  colsums separated by [,\s]
     *
     * e.g.
     * """
     * 0,0,8,2,6,4,5,3,7,0,0
     * 0,0,7,1,6,3,4,5,2,7,0,0
     * # comment
     * % another comment
     * """
     *
     */
    private static void readFile(String file)
    {
        Console.WriteLine("readFile(" + file + ")");

        TextReader inr = new StreamReader(file);
        String str;
        int lineCount = 0;
        while ((str = inr.ReadLine()) != null && str.Length > 0)
        {
            str = str.Trim();

            // ignore comments
            if (str.StartsWith("#") || str.StartsWith("%"))
            {
                continue;
            }

            if (lineCount == 0)
            {
                rowsums2 = ConvLine(str);
            }
            else if (lineCount == 1)
            {
                colsums2 = ConvLine(str);
                break;
            }

            lineCount++;

        } // end while

        inr.Close();

    } // end readFile

    private static int[] ConvLine(String str)
    {
        String[] tmp = Regex.Split(str, "[,\\s]+");
        int len = tmp.Length;
        int[] sums = new int[len];
        for (int i = 0; i < len; i++)
        {
            sums[i] = Convert.ToInt32(tmp[i]);
        }

        return sums;
    }

    public static void Main(String[] args)
    {
        if (args.Length > 0)
        {
            readFile(args[0]);
            Solve(rowsums2, colsums2);
        }
        else
        {
            Solve(default_rowsums, default_colsums);
        }
    }
}
