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
using System.Linq;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class PostOfficeProblem2
{
    /**
     *
     * Post office problem.
     *
     * Problem statement:
     * http://www-128.ibm.com/developerworks/linux/library/l-glpk2/
     *
     * From Winston 'Operations Research: Applications and Algorithms':
     * """
     * A post office requires a different number of full-time employees working
     * on different days of the week [summarized below]. Union rules state that
     * each full-time employee must work for 5 consecutive days and then receive
     * two days off. For example, an employee who works on Monday to Friday
     * must be off on Saturday and Sunday. The post office wants to meet its
     * daily requirements using only full-time employees. Minimize the number
     * of employees that must be hired.
     *
     * To summarize the important information about the problem:
     *
     * Every full-time worker works for 5 consecutive days and takes 2 days off
     * - Day 1 (Monday): 17 workers needed
     * - Day 2 : 13 workers needed
     * - Day 3 : 15 workers needed
     * - Day 4 : 19 workers needed
     * - Day 5 : 14 workers needed
     * - Day 6 : 16 workers needed
     * - Day 7 (Sunday) : 11 workers needed
     *
     * The post office needs to minimize the number of employees it needs
     * to hire to meet its demand.
     * """
     *
     * Also see http://www.hakank.org/or-tools/post_office_problem2.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("PostOfficeProblem2");

        //
        // Data
        //

        // days 0..6, monday 0
        int n = 7;
        int[] need = { 17, 13, 15, 19, 14, 16, 11 };

        // Total cost for the 5 day schedule.
        // Base cost per day is 100.
        // Working saturday is 100 extra
        // Working sunday is 200 extra.
        int[] cost = { 500, 600, 800, 800, 800, 800, 700 };

        //
        // Decision variables
        //

        // No. of workers starting at day i
        IntVar[] x = solver.MakeIntVarArray(n, 0, 100, "x");

        IntVar total_cost = x.ScalProd(cost).Var();
        IntVar num_workers = x.Sum().Var();

        //
        // Constraints
        //
        for (int i = 0; i < n; i++)
        {
      IntVar s = (from j in Enumerable.Range(0, n) where j != (i + 5) % n &&
                  j != (i + 6) % n select x[j])
                     .ToArray()
                     .Sum()
                     .Var();
      solver.Add(s >= need[i]);
        }

        // Add a limit for the cost
        solver.Add(total_cost <= 20000);

        //

        // objective
        //
        //
        OptimizeVar obj = total_cost.Minimize(100);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_MIN_SIZE_LOWEST_MIN, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("num_workers: {0}", num_workers.Value());
            Console.WriteLine("total_cost: {0}", total_cost.Value());
            Console.Write("x: ");
            for (int i = 0; i < n; i++)
            {
                Console.Write(x[i].Value() + " ");
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
