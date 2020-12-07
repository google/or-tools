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
using Google.OrTools.LinearSolver;

public class Volsay3
{
    /**
     * Volsay problem.
     *
     * From the OPL model volsay.mod.
     * This version use arrays and matrices
     *
     * Also see
     *  http://www.hakank.org/or-tools/volsay2.cs
     *  http://www.hakank.org/or-tools/volsay3.py
     */
    private static void Solve()
    {
        Solver solver = new Solver("Volsay3", Solver.OptimizationProblemType.CLP_LINEAR_PROGRAMMING);

        int num_products = 2;
        IEnumerable<int> PRODUCTS = Enumerable.Range(0, num_products);
        String[] products = { "Gas", "Chloride" };
        String[] components = { "nitrogen", "hydrogen", "chlorine" };

        int[,] demand = { { 1, 3, 0 }, { 1, 4, 1 } };
        int[] profit = { 30, 40 };
        int[] stock = { 50, 180, 40 };

        //
        // Variables
        //
        Variable[] production = new Variable[num_products];
        foreach (int p in PRODUCTS)
        {
            production[p] = solver.MakeNumVar(0, 100000, products[p]);
        }

        //
        // Constraints
        //
        int c_len = components.Length;
        Constraint[] cons = new Constraint[c_len];
        for (int c = 0; c < c_len; c++)
        {
            cons[c] = solver.Add((from p in PRODUCTS select(demand[p, c] * production[p])).ToArray().Sum() <= stock[c]);
        }

        //
        // Objective
        //
        solver.Maximize((from p in PRODUCTS select(profit[p] * production[p])).ToArray().Sum());

        if (solver.Solve() != Solver.ResultStatus.OPTIMAL)
        {
            Console.WriteLine("The problem don't have an optimal solution.");
            return;
        }

        Console.WriteLine("Objective: {0}", solver.Objective().Value());
        foreach (int p in PRODUCTS)
        {
            Console.WriteLine("{0,-10}: {1} ReducedCost: {2}", products[p], production[p].SolutionValue(),
                              production[p].ReducedCost());
        }

        double[] activities = solver.ComputeConstraintActivities();
        for (int c = 0; c < c_len; c++)
        {
            Console.WriteLine("Constraint {0} DualValue {1} Activity: {2} lb: {3} ub: {4}", c, cons[c].DualValue(),
                              activities[cons[c].Index()], cons[c].Lb(), cons[c].Ub());
        }

        Console.WriteLine("\nWallTime: " + solver.WallTime());
        Console.WriteLine("Iterations: " + solver.Iterations());
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
