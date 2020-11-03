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

public class JustForgotten
{
    /**
     *
     * Just forgotten puzzle (Enigma 1517) in Google CP Solver.
     *
     * From http://www.f1compiler.com/samples/Enigma 201517.f1.html
     * """
     * Enigma 1517 Bob Walker, New Scientist magazine, October 25, 2008.
     *
     * Joe was furious when he forgot one of his bank account numbers.
     * He remembered that it had all the digits 0 to 9 in some order,
     * so he tried the following four sets without success:
     *
     * 9 4 6 2 1 5 7 8 3 0
     * 8 6 0 4 3 9 1 2 5 7
     * 1 6 4 0 2 9 7 8 5 3
     * 6 8 2 4 3 1 9 0 7 5
     *
     * When Joe finally remembered his account number, he realised that
     * in each set just four of the digits were in their correct position
     * and that, if one knew that, it was possible to work out his
     * account number. What was it?
     * """
     *
     * Also see http://www.hakank.org/google_or_tools/just_forgotten.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("JustForgotten");

        int rows = 4;
        int cols = 10;

        // The four tries
        int[,] a = { { 9, 4, 6, 2, 1, 5, 7, 8, 3, 0 },
                     { 8, 6, 0, 4, 3, 9, 1, 2, 5, 7 },
                     { 1, 6, 4, 0, 2, 9, 7, 8, 5, 3 },
                     { 6, 8, 2, 4, 3, 1, 9, 0, 7, 5 } };

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(cols, 0, 9, "x");

        //
        // Constraints
        //
        solver.Add(x.AllDifferent());
        for (int r = 0; r < rows; r++)
        {
            solver.Add((from c in Enumerable.Range(0, cols) select x[c] == a[r, c]).ToArray().Sum() == 4);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.WriteLine("Account number:");
            for (int j = 0; j < cols; j++)
            {
                Console.Write(x[j].Value() + " ");
            }
            Console.WriteLine("\n");
            Console.WriteLine("The four tries, where '!' represents a correct digit:");
            for (int i = 0; i < rows; i++)
            {
                for (int j = 0; j < cols; j++)
                {
                    String c = " ";
                    if (a[i, j] == x[j].Value())
                    {
                        c = "!";
                    }
                    Console.Write("{0}{1} ", a[i, j], c);
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

    public static void Main(String[] args)
    {
        Solve();
    }
}
