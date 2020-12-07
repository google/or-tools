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

public class Grocery
{
    public static void Decreasing(Solver solver, IntVar[] x)
    {
        for (int i = 0; i < x.Length - 1; i++)
        {
            solver.Add(x[i] <= x[i + 1]);
        }
    }

    //
    // Simple decomposition of Prod() for an IntVar array
    //
    private static Constraint MyProd(IntVar[] x, int prod)
    {
        int len = x.Length;
        IntVar[] tmp = new IntVar[len];
        tmp[0] = x[0];
        for (int i = 1; i < len; i++)
        {
            tmp[i] = (tmp[i - 1] * x[i]).Var();
        }

        return tmp[len - 1] == prod;
    }

    /**
     *
     * Grocery problem.
     *
     * From  Christian Schulte, Gert Smolka, Finite Domain
     * http://www.mozart-oz.org/documentation/fdt/
     * Constraint Programming in Oz. A Tutorial. 2001.
     * """
     * A kid goes into a grocery store and buys four items. The cashier
     * charges $7.11, the kid pays and is about to leave when the cashier
     * calls the kid back, and says 'Hold on, I multiplied the four items
     * instead of adding them; I'll try again; Hah, with adding them the
     * price still comes to $7.11'. What were the prices of the four items?
     * """
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Grocery");

        int n = 4;
        int c = 711;

        //
        // Decision variables
        //

        IntVar[] item = solver.MakeIntVarArray(n, 0, c / 2, "item");

        //
        // Constraints
        //
        solver.Add(item.Sum() == c);
        // solver.Add(item[0] * item[1] * item[2] * item[3] == c * 100*100*100);
        // solver.Add(item.Prod() == c * 100*100*100);
        solver.Add(MyProd(item, c * 100 * 100 * 100));

        // Symmetry breaking
        Decreasing(solver, item);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(item, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);
        while (solver.NextSolution())
        {
            for (int i = 0; i < n; i++)
            {
                Console.Write(item[i].Value() + " ");
            }
            Console.WriteLine();
        }

        Console.WriteLine("\nWallTime: " + solver.WallTime() + "ms ");
        Console.WriteLine("Failures: " + solver.Failures());
        Console.WriteLine("Branches: " + solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
