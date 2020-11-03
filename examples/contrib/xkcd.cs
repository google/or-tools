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

public class Xkcd
{
    /**
     *
     * Solve the xkcd problem
     * See http://www.hakank.org/google_or_tools/xkcd.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Xkcd");

        //
        // Constants, inits
        //
        int n = 6;
        // for price and total: multiplied by 100 to be able to use integers
        int[] price = { 215, 275, 335, 355, 420, 580 };
        int total = 1505;

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, 10, "x");

        //
        // Constraints
        //
        solver.Add(x.ScalProd(price) == total);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);
        while (solver.NextSolution())
        {
            for (int i = 0; i < n; i++)
            {
                Console.Write(x[i].Value() + " ");
            }
            Console.WriteLine();
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0} ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0}", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
