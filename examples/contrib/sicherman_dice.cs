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

public class SichermanDice
{
    /**
     *
     * Sicherman Dice.
     *
     * From http://en.wikipedia.org/wiki/Sicherman_dice
     * ""
     * Sicherman dice are the only pair of 6-sided dice which are not normal dice,
     * bear only positive integers, and have the same probability distribution for
     * the sum as normal dice.
     *
     * The faces on the dice are numbered 1, 2, 2, 3, 3, 4 and 1, 3, 4, 5, 6, 8.
     * ""
     *
     * I read about this problem in a book/column by Martin Gardner long
     * time ago, and got inspired to model it now by the WolframBlog post
     * "Sicherman Dice": http://blog.wolfram.com/2010/07/13/sicherman-dice/
     *
     * This model gets the two different ways, first the standard way and
     * then the Sicherman dice:
     *
     *  x1 = [1, 2, 3, 4, 5, 6]
     *  x2 = [1, 2, 3, 4, 5, 6]
     *  ----------
     *  x1 = [1, 2, 2, 3, 3, 4]
     *  x2 = [1, 3, 4, 5, 6, 8]
     *
     *
     * Extra: If we also allow 0 (zero) as a valid value then the
     * following two solutions are also valid:
     *
     * x1 = [0, 1, 1, 2, 2, 3]
     * x2 = [2, 4, 5, 6, 7, 9]
     * ----------
     * x1 = [0, 1, 2, 3, 4, 5]
     * x2 = [2, 3, 4, 5, 6, 7]
     *
     * These two extra cases are mentioned here:
     * http://mathworld.wolfram.com/SichermanDice.html
     *
     *
     * Also see http://www.hakank.org/or-tools/sicherman_dice.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("SichermanDice");

        //
        // Data
        //
        int n = 6;
        int m = 10;
        int lowest_value = 0;

        // standard distribution
        int[] standard_dist = { 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1 };

        IEnumerable<int> RANGE = Enumerable.Range(0, n);
        IEnumerable<int> RANGE1 = Enumerable.Range(0, n - 1);

        //
        // Decision variables
        //

        IntVar[] x1 = solver.MakeIntVarArray(n, lowest_value, m, "x1");
        IntVar[] x2 = solver.MakeIntVarArray(n, lowest_value, m, "x2");

        //
        // Constraints
        //
        for (int k = 0; k < standard_dist.Length; k++)
        {
            solver.Add((from i in RANGE from j in RANGE select x1[i] + x2[j] == k + 2).ToArray().Sum() ==
                       standard_dist[k]);
        }

        // symmetry breaking
        foreach (int i in RANGE1)
        {
            solver.Add(x1[i] <= x1[i + 1]);
            solver.Add(x2[i] <= x2[i + 1]);
            solver.Add(x1[i] <= x2[i]);
        }

        //
        // Search
        //
        DecisionBuilder db =
            solver.MakePhase(x1.Concat(x2).ToArray(), Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.Write("x1: ");
            foreach (int i in RANGE)
            {
                Console.Write(x1[i].Value() + " ");
            }
            Console.Write("\nx2: ");
            foreach (int i in RANGE)
            {
                Console.Write(x2[i].Value() + " ");
            }
            Console.WriteLine("\n");
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
