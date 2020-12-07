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
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.ConstraintSolver;

public class PMedian
{
    /**
     *
     * P-median problem.
     *
     * Model and data from the OPL Manual, which describes the problem:
     * """
     * The P-Median problem is a well known problem in Operations Research.
     * The problem can be stated very simply, like this: given a set of customers
     * with known amounts of demand, a set of candidate locations for warehouses,
     * and the distance between each pair of customer-warehouse, choose P
     * warehouses to open that minimize the demand-weighted distance of serving
     * all customers from those P warehouses.
     * """
     *
     * Also see http://www.hakank.org/or-tools/p_median.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("PMedian");

        //
        // Data
        //
        int p = 2;
        int num_customers = 4;
        IEnumerable<int> CUSTOMERS = Enumerable.Range(0, num_customers);

        int num_warehouses = 3;
        IEnumerable<int> WAREHOUSES = Enumerable.Range(0, num_warehouses);

        int[] demand = { 100, 80, 80, 70 };
        int[,] distance = { { 2, 10, 50 }, { 2, 10, 52 }, { 50, 60, 3 }, { 40, 60, 1 } };

        //
        // Decision variables
        //

        IntVar[] open = solver.MakeIntVarArray(num_warehouses, 0, num_warehouses, "open");
        IntVar[,] ship = solver.MakeIntVarMatrix(num_customers, num_warehouses, 0, 1, "ship");
        IntVar z = solver.MakeIntVar(0, 1000, "z");

        //
        // Constraints
        //

        solver.Add((from c in CUSTOMERS from w in WAREHOUSES select(demand[c] * distance[c, w] * ship[c, w]))
                       .ToArray()
                       .Sum() == z);

        solver.Add(open.Sum() == p);

        foreach (int c in CUSTOMERS)
        {
            foreach (int w in WAREHOUSES)
            {
                solver.Add(ship[c, w] <= open[w]);
            }

            solver.Add((from w in WAREHOUSES select ship[c, w]).ToArray().Sum() == 1);
        }

        //
        // Objective
        //
        OptimizeVar obj = z.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(open.Concat(ship.Flatten()).ToArray(), Solver.CHOOSE_FIRST_UNBOUND,
                                              Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("z: {0}", z.Value());
            Console.Write("open:");
            foreach (int w in WAREHOUSES)
            {
                Console.Write(open[w].Value() + " ");
            }
            Console.WriteLine();
            foreach (int c in CUSTOMERS)
            {
                foreach (int w in WAREHOUSES)
                {
                    Console.Write(ship[c, w].Value() + " ");
                }
                Console.WriteLine();
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
