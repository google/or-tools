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

public class AllInterval
{
    /**
     *
     * Implements the all interval problem.
     * See http://www.hakank.org/google_or_tools/all_interval.py
     *
     */
    private static void Solve(int n = 12)
    {
        Solver solver = new Solver("AllInterval");

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, n - 1, "x");
        IntVar[] diffs = solver.MakeIntVarArray(n - 1, 1, n - 1, "diffs");

        //
        // Constraints
        //
        solver.Add(x.AllDifferent());
        solver.Add(diffs.AllDifferent());

        for (int k = 0; k < n - 1; k++)
        {
            // solver.Add(diffs[k] == (x[k + 1] - x[k]).Abs());
            solver.Add(diffs[k] == (x[k + 1] - x[k].Abs()));
        }

        // symmetry breaking
        solver.Add(x[0] < x[n - 1]);
        solver.Add(diffs[0] < diffs[1]);

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
                Console.Write("{0} ", x[i].Value());
            }
            Console.Write("  diffs: ");
            for (int i = 0; i < n - 1; i++)
            {
                Console.Write("{0} ", diffs[i].Value());
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
        int n = 12;
        if (args.Length > 0)
        {
            n = Convert.ToInt32(args[0]);
        }

        Solve(n);
    }
}
