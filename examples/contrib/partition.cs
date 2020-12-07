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

public class Partition
{
    /**
     *
     * This is a port of Charles Prud'homme's Java model
     * Partition.java
     * """
     * Partition n numbers into two groups, so that
     * - the sum of the first group equals the sum of the second,
     * - and the sum of the squares of the first group equals the sum of
     *   the squares of the second
     * """
     *
     */
    private static void Solve(int m)
    {
        Solver solver = new Solver("Partition");

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(m, 1, 2 * m, "x");
        IntVar[] y = solver.MakeIntVarArray(m, 1, 2 * m, "y");

        //
        // Constraints
        //
        // break symmetries
        for (int i = 0; i < m - 1; i++)
        {
            solver.Add(x[i] < x[i + 1]);
            solver.Add(y[i] < y[i + 1]);
        }
        solver.Add(x[0] < y[0]);

        IntVar[] xy = new IntVar[2 * m];
        for (int i = m - 1; i >= 0; i--)
        {
            xy[i] = x[i];
            xy[m + i] = y[i];
        }

        solver.Add(xy.AllDifferent());

        int[] coeffs = new int[2 * m];
        for (int i = m - 1; i >= 0; i--)
        {
            coeffs[i] = 1;
            coeffs[m + i] = -1;
        }
        solver.Add(xy.ScalProd(coeffs) == 0);

        IntVar[] sxy, sx, sy;
        sxy = new IntVar[2 * m];
        sx = new IntVar[m];
        sy = new IntVar[m];
        for (int i = m - 1; i >= 0; i--)
        {
            sx[i] = x[i].Square().Var();
            sxy[i] = sx[i];
            sy[i] = y[i].Square().Var();
            sxy[m + i] = sy[i];
        }
        solver.Add(sxy.ScalProd(coeffs) == 0);

        solver.Add(x.Sum() == 2 * m * (2 * m + 1) / 4);
        solver.Add(y.Sum() == 2 * m * (2 * m + 1) / 4);
        solver.Add(sx.Sum() == 2 * m * (2 * m + 1) * (4 * m + 1) / 12);
        solver.Add(sy.Sum() == 2 * m * (2 * m + 1) * (4 * m + 1) / 12);

        //
        // Search
        //
        DecisionBuilder db = solver.MakeDefaultPhase(xy);

        SearchMonitor log = solver.MakeSearchLog(10000);
        solver.NewSearch(db, log);

        while (solver.NextSolution())
        {
            for (int i = 0; i < m; i++)
            {
                Console.Write("[" + xy[i].Value() + "] ");
            }
            Console.WriteLine();
            for (int i = 0; i < m; i++)
            {
                Console.Write("[" + xy[m + i].Value() + "] ");
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
        int m = 32;
        if (args.Length > 0)
        {
            m = Convert.ToInt32(args[0]);
        }

        Solve(m);
    }
}
