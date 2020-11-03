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
using Google.OrTools.ConstraintSolver;

public class Olympic
{
    public static void minus(Solver solver, IntVar x, IntVar y, IntVar z)
    {
        solver.Add(z == (x - y).Abs());
    }

    /**
     *
     * Olympic puzzle.
     *
     * Benchmark for Prolog (BProlog)
     * """
     * File   : olympic.pl
     * Author : Neng-Fa ZHOU
     * Date   : 1993
     *
     * Purpose: solve a puzzle taken from Olympic Arithmetic Contest
     *
     * Given ten variables with the following configuration:
     *
     *                 X7   X8   X9   X10
     *
     *                    X4   X5   X6
     *
     *                       X2   X3
     *
     *                          X1
     *
     * We already know that X1 is equal to 3 and want to assign each variable
     * with a different integer from {1,2,...,10} such that for any three
     * variables
     *                        Xi   Xj
     *
     *                           Xk
     *
     * the following constraint is satisfied:
     *
     *                      |Xi-Xj| = Xk
     * """
     *
     * Also see http://www.hakank.org/or-tools/olympic.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Olympic");

        //
        // Data
        //
        int n = 10;

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 1, n, "x");
        IntVar X1 = x[0];
        IntVar X2 = x[1];
        IntVar X3 = x[2];
        IntVar X4 = x[3];
        IntVar X5 = x[4];
        IntVar X6 = x[5];
        IntVar X7 = x[6];
        IntVar X8 = x[7];
        IntVar X9 = x[8];
        IntVar X10 = x[9];

        //
        // Constraints
        //
        solver.Add(x.AllDifferent());

        solver.Add(X1 == 3);
        minus(solver, X2, X3, X1);
        minus(solver, X4, X5, X2);
        minus(solver, X5, X6, X3);
        minus(solver, X7, X8, X4);
        minus(solver, X8, X9, X5);
        minus(solver, X9, X10, X6);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.INT_VAR_SIMPLE, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            for (int i = 0; i < n; i++)
            {
                Console.Write("{0,2} ", x[i].Value());
            }
            Console.WriteLine();
        }

        Console.WriteLine("\nSolutions: " + solver.Solutions());
        Console.WriteLine("WallTime: " + solver.WallTime() + "ms ");
        Console.WriteLine("Failures: " + solver.Failures());
        Console.WriteLine("Branches: " + solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
