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

public class GolombRuler
{
    /**
     *
     * Golomb Ruler problem.
     *
     * This C# implementation is based on Charles Prud'homme's
     * or-tools/Java model:
     * http://code.google.com/p/or-tools/source/browse/trunk/com/google/ortools/constraintsolver/samples/GolombRuler.java
     *
     */
    private static void Solve(int m = 8)
    {
        Solver solver = new Solver("GolombRuler");

        //
        // Decision variables
        //
        IntVar[] ticks = solver.MakeIntVarArray(m, 0, ((m < 31) ? (1 << (m + 1)) - 1 : 9999), "ticks");

        IntVar[] diff = new IntVar[(m * m - m) / 2];

        //
        // Constraints
        //
        solver.Add(ticks[0] == 0);

        for (int i = 0; i < ticks.Length - 1; i++)
        {
            solver.Add(ticks[i] < ticks[i + 1]);
        }

        for (int k = 0, i = 0; i < m - 1; i++)
        {
            for (int j = i + 1; j < m; j++, k++)
            {
                diff[k] = (ticks[j] - ticks[i]).Var();
                solver.Add(diff[k] >= (j - i) * (j - i + 1) / 2);
            }
        }

        solver.Add(diff.AllDifferent());

        // break symetries
        if (m > 2)
        {
            solver.Add(diff[0] < diff[diff.Length - 1]);
        }

        //
        // Optimization
        //
        OptimizeVar opt = ticks[m - 1].Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(ticks, Solver.CHOOSE_MIN_SIZE_LOWEST_MIN, Solver.ASSIGN_MIN_VALUE);

        // We just want the debug info for larger instances.
        if (m >= 11)
        {
            SearchMonitor log = solver.MakeSearchLog(10000, opt);
            solver.NewSearch(db, opt, log);
        }
        else
        {
            solver.NewSearch(db, opt);
        }

        while (solver.NextSolution())
        {
            Console.Write("opt: {0}  [ ", ticks[m - 1].Value());
            for (int i = 0; i < m; i++)
            {
                Console.Write("{0} ", ticks[i].Value());
            }
            Console.WriteLine("]");
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        int n = 8;
        if (args.Length > 0)
        {
            n = Convert.ToInt32(args[0]);
        }

        Solve(n);
    }
}
