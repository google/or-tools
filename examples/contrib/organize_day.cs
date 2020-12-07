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

public class OrganizeDay
{
    //
    // No overlapping of tasks s1 and s2
    //
    public static void NoOverlap(Solver solver, IntVar s1, int d1, IntVar s2, int d2)
    {
        solver.Add((s1 + d1 <= s2) + (s2 + d2 <= s1) == 1);
    }

    /**
     *
     *
     * Organizing a day.
     *
     * Simple scheduling problem.
     *
     * Problem formulation from ECLiPSe:
     * Slides on (Finite Domain) Constraint Logic Programming, page 38f
     * http://eclipseclp.org/reports/eclipse.ppt
     *
     *
     * Also see http://www.hakank.org/google_or_tools/organize_day.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("OrganizeDay");

        int n = 4;

        int work = 0;
        int mail = 1;
        int shop = 2;
        int bank = 3;
        int[] tasks = { work, mail, shop, bank };
        int[] durations = { 4, 1, 2, 1 };

        // task [i,0] must be finished before task [i,1]
        int[,] before_tasks = { { bank, shop }, { mail, work } };

        // the valid times of the day
        int begin = 9;
        int end = 17;

        //
        // Decision variables
        //
        IntVar[] begins = solver.MakeIntVarArray(n, begin, end, "begins");
        IntVar[] ends = solver.MakeIntVarArray(n, begin, end, "ends");

        //
        // Constraints
        //
        foreach (int t in tasks)
        {
            solver.Add(ends[t] == begins[t] + durations[t]);
        }

        foreach (int i in tasks)
        {
            foreach (int j in tasks)
            {
                if (i < j)
                {
                    NoOverlap(solver, begins[i], durations[i], begins[j], durations[j]);
                }
            }
        }

        // specific constraints
        for (int t = 0; t < before_tasks.GetLength(0); t++)
        {
            solver.Add(ends[before_tasks[t, 0]] <= begins[before_tasks[t, 1]]);
        }

        solver.Add(begins[work] >= 11);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(begins, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            foreach (int t in tasks)
            {
                Console.WriteLine("Task {0}: {1,2} .. ({2}) .. {3,2}", t, begins[t].Value(), durations[t],
                                  ends[t].Value());
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
