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

public class SubsetSum
{
    public static IntVar[] subset_sum(Solver solver, int[] values, int total)
    {
        int n = values.Length;
        IntVar[] x = solver.MakeIntVarArray(n, 0, n, "x");
        solver.Add(x.ScalProd(values) == total);

        return x;
    }

    /**
     *
     * Subset sum problem.
     *
     * From Katta G. Murty: 'Optimization Models for Decision Making', page 340
     * http://ioe.engin.umich.edu/people/fac/books/murty/opti_model/junior-7.pdf
     * """
     * Example 7.8.1
     *
     * A bank van had several bags of coins, each containing either
     * 16, 17, 23, 24, 39, or 40 coins. While the van was parked on the
     * street, thieves stole some bags. A total of 100 coins were lost.
     * It is required to find how many bags were stolen.
     * """
     *
     * Also see http://www.hakank.org/or-tools/subset_sum.py
     *
     */
    private static void Solve(int[] coins, int total)
    {
        Solver solver = new Solver("SubsetSum");

        int n = coins.Length;
        Console.Write("Coins: ");
        for (int i = 0; i < n; i++)
        {
            Console.Write(coins[i] + " ");
        }
        Console.WriteLine("\nTotal: {0}", total);

        //
        // Variables
        //
        // number of coins
        IntVar s = solver.MakeIntVar(0, coins.Sum(), "s");

        //
        // Constraints
        //
        IntVar[] x = subset_sum(solver, coins, total);
        solver.Add(x.Sum() == s);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.Write("x: ");
            for (int i = 0; i < n; i++)
            {
                Console.Write(x[i].Value() + " ");
            }
            Console.WriteLine("  s: {0}", s.Value());
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        int[] coins = { 16, 17, 23, 24, 39, 40 };
        int total = 100;

        if (args.Length > 0)
        {
            total = Convert.ToInt32(args[0]);
        }

        Solve(coins, total);
    }
}
