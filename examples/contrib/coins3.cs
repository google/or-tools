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

public class Coins3
{
    /**
     *
     * Coin application.
     *
     * From "Constraint Logic Programming using ECLiPSe"
     *  pages 99f and 234 ff.
     * The solution in ECLiPSe is at page 236.
     *
     * """
     * What is the minimum number of coins that allows one to pay _exactly_
     * any amount smaller than one Euro? Recall that there are six different
     * euro cents, of denomination 1, 2, 5, 10, 20, 50
     * """

     * Also see http://www.hakank.org/or-tools/coins3.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Coins3");

        //
        // Data
        //
        int n = 6; // number of different coins
        int[] variables = { 1, 2, 5, 10, 25, 50 };

        IEnumerable<int> RANGE = Enumerable.Range(0, n);

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, 99, "x");
        IntVar num_coins = x.Sum().VarWithName("num_coins");

        //
        // Constraints
        //

        // Check that all changes from 1 to 99 can be made.
        for (int j = 1; j < 100; j++)
        {
            IntVar[] tmp = solver.MakeIntVarArray(n, 0, 99, "tmp");
            solver.Add(tmp.ScalProd(variables) == j);

            foreach (int i in RANGE)
            {
                solver.Add(tmp[i] <= x[i]);
            }
        }

        //
        // Objective
        //
        OptimizeVar obj = num_coins.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_MIN_SIZE_LOWEST_MAX, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("num_coins: {0}", num_coins.Value());
            Console.Write("x:  ");
            foreach (int i in RANGE)
            {
                Console.Write(x[i].Value() + " ");
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
