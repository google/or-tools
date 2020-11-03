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

public class MagicSquareAndCards
{
    /**
     *
     * Magic squares and cards problem.
     *
     * Martin Gardner (July 1971)
     * """
     * Allowing duplicates values, what is the largest constant sum for an order-3
     * magic square that can be formed with nine cards from the deck.
     * """
     *
     *
     * Also see http://www.hakank.org/or-tools/magic_square_and_cards.py
     *
     */
    private static void Solve(int n = 3)
    {
        Solver solver = new Solver("MagicSquareAndCards");

        IEnumerable<int> RANGE = Enumerable.Range(0, n);

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(n, n, 1, 13, "x");
        IntVar[] x_flat = x.Flatten();

        IntVar s = solver.MakeIntVar(1, 13 * 4, "s");
        IntVar[] counts = solver.MakeIntVarArray(14, 0, 4, "counts");

        //
        // Constraints
        //

        solver.Add(x_flat.Distribute(counts));

        // the standard magic square constraints (sans all_different)
        foreach (int i in RANGE)
        {
            // rows
            solver.Add((from j in RANGE select x[i, j]).ToArray().Sum() == s);

            // columns
            solver.Add((from j in RANGE select x[j, i]).ToArray().Sum() == s);
        }

        // diagonals
        solver.Add((from i in RANGE select x[i, i]).ToArray().Sum() == s);
        solver.Add((from i in RANGE select x[i, n - i - 1]).ToArray().Sum() == s);

        // redundant constraint
        solver.Add(counts.Sum() == n * n);

        //
        // Objective
        //
        OptimizeVar obj = s.Maximize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MAX_VALUE);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("s: {0}", s.Value());
            Console.Write("counts:");
            for (int i = 0; i < 14; i++)
            {
                Console.Write(counts[i].Value() + " ");
            }
            Console.WriteLine();
            for (int i = 0; i < n; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    Console.Write(x[i, j].Value() + " ");
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
        int n = 3;

        if (args.Length > 0)
        {
            n = Convert.ToInt32(args[0]);
        }

        Solve(n);
    }
}
