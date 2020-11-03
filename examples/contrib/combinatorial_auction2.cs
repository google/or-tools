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
using System.Linq;
using System.Collections;
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;

public class CombinatorialAuction2
{
    /**
     *
     * Combinatorial auction.
     *
     * This is a more general model for the combinatorial example
     * in the Numberjack Tutorial, pages 9 and 24 (slides  19/175 and
     * 51/175).
     *
     * See http://www.hakank.org/or-tools/combinatorial_auction2.py
     *
     * The original and more talkative model is here:
     * http://www.hakank.org/numberjack/combinatorial_auction.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("CombinatorialAuction2");

        //
        // Data
        //
        int n = 5;

        // the items for each bid
        int[][] items = {
            new int[] { 0, 1 },    // A,B
            new int[] { 0, 2 },    // A, C
            new int[] { 1, 3 },    // B,D
            new int[] { 1, 2, 3 }, // B,C,D
            new int[] { 0 }        // A
        };

        int[] bid_ids = { 0, 1, 2, 3 };
        int[] bid_amount = { 10, 20, 30, 40, 14 };

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, 1, "x");
        IntVar z = x.ScalProd(bid_amount).VarWithName("z");

        //
        // Constraints
        //

        foreach (int bid_id in bid_ids)
        {
      var tmp2 = (from item in Enumerable.Range(0, n)
                      from i in Enumerable.Range(0, items[item].Length)
                          where items [item]
                          [i] == bid_id select x[item]);

      solver.Add(tmp2.ToArray().Sum() <= 1);
        }

        //
        // Objective
        //
        OptimizeVar obj = z.Maximize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.Write("z: {0,2} x: ", z.Value());
            for (int i = 0; i < n; i++)
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
