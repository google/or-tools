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

public class SetCoveringOPL
{
    /**
     *
     * Solves a set covering problem.
     * See  See http://www.hakank.org/or-tools/set_covering_opl.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("SetCoveringOPL");

        //
        // data
        //
        int num_workers = 32;
        int num_tasks = 15;

        // Which worker is qualified for each task.
        // Note: This is 1-based and will be made 0-base below.
        int[][] qualified = {
            new int[] { 1, 9, 19, 22, 25, 28, 31 },       new int[] { 2, 12, 15, 19, 21, 23, 27, 29, 30, 31, 32 },
            new int[] { 3, 10, 19, 24, 26, 30, 32 },      new int[] { 4, 21, 25, 28, 32 },
            new int[] { 5, 11, 16, 22, 23, 27, 31 },      new int[] { 6, 20, 24, 26, 30, 32 },
            new int[] { 7, 12, 17, 25, 30, 31 },          new int[] { 8, 17, 20, 22, 23 },
            new int[] { 9, 13, 14, 26, 29, 30, 31 },      new int[] { 10, 21, 25, 31, 32 },
            new int[] { 14, 15, 18, 23, 24, 27, 30, 32 }, new int[] { 18, 19, 22, 24, 26, 29, 31 },
            new int[] { 11, 20, 25, 28, 30, 32 },         new int[] { 16, 19, 23, 31 },
            new int[] { 9, 18, 26, 28, 31, 32 }
        };

        int[] cost = { 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 8, 9 };

        //
        // Decision variables
        //
        IntVar[] hire = solver.MakeIntVarArray(num_workers, 0, 1, "workers");
        IntVar total_cost = hire.ScalProd(cost).Var();

        //
        // Constraints
        //

        for (int j = 0; j < num_tasks; j++)
        {
            // Sum the cost for hiring the qualified workers
            // (also, make 0-base).
            int len = qualified[j].Length;
            IntVar[] tmp = new IntVar[len];
            for (int c = 0; c < len; c++)
            {
                tmp[c] = hire[qualified[j][c] - 1];
            }
            solver.Add(tmp.Sum() >= 1);
        }

        //
        // objective
        //
        OptimizeVar objective = total_cost.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(hire, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db, objective);

        while (solver.NextSolution())
        {
            Console.WriteLine("Cost: " + total_cost.Value());
            Console.Write("Hire: ");
            for (int i = 0; i < num_workers; i++)
            {
                if (hire[i].Value() == 1)
                {
                    Console.Write(i + " ");
                }
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
