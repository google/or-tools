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

public class Diet
{
    /**
     *
     * Solves the Diet problem
     *
     * See http://www.hakank.org/google_or_tools/diet1.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Diet");

        int n = 4;
        int[] price = { 50, 20, 30, 80 }; // in cents

        // requirements for each nutrition type
        int[] limits = { 500, 6, 10, 8 };
        string[] products = { "A", "B", "C", "D" };

        // nutritions for each product
        int[] calories = { 400, 200, 150, 500 };
        int[] chocolate = { 3, 2, 0, 0 };
        int[] sugar = { 2, 2, 4, 4 };
        int[] fat = { 2, 4, 1, 5 };

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, 100, "x");
        IntVar cost = x.ScalProd(price).Var();

        //
        // Constraints
        //

        // solver.Add(solver.MakeScalProdGreaterOrEqual(x, calories,  limits[0]));
        solver.Add(x.ScalProd(calories) >= limits[0]);
        solver.Add(x.ScalProd(chocolate) >= limits[1]);
        solver.Add(x.ScalProd(sugar) >= limits[2]);
        solver.Add(x.ScalProd(fat) >= limits[3]);

        //
        // Objective
        //
        OptimizeVar obj = cost.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_PATH, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db, obj);
        while (solver.NextSolution())
        {
            Console.WriteLine("cost: {0}", cost.Value());
            Console.WriteLine("Products: ");
            for (int i = 0; i < n; i++)
            {
                Console.WriteLine("{0}: {1}", products[i], x[i].Value());
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
