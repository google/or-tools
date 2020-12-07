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

public class Volsay2
{
    /**
     * Volsay problem.
     *
     * From the OPL model volsay.mod.
     *
     * Also see
     *  http://www.hakank.org/or-tools/volsay.cs
     *  http://www.hakank.org/or-tools/volsay2.py
     */
    private static void Solve()
    {
        Solver solver = new Solver("Volsay2", Solver.OptimizationProblemType.CLP_LINEAR_PROGRAMMING);

        int num_products = 2;
        IEnumerable<int> PRODUCTS = Enumerable.Range(0, num_products);
        int Gas = 0;
        int Chloride = 1;
        String[] products = { "Gas", "Chloride" };

        //
        // Variables
        //
        Variable[] production = new Variable[num_products];
        foreach (int p in PRODUCTS)
        {
            production[p] = solver.MakeNumVar(0, 100000, products[p]);
        }

        int num_constraints = 2;
        IEnumerable<int> CONSTRAINTS = Enumerable.Range(0, num_constraints);
        Constraint[] cons = new Constraint[num_constraints];
        cons[0] = solver.Add(production[Gas] + production[Chloride] <= 50);
        cons[1] = solver.Add(3 * production[Gas] + 4 * production[Chloride] <= 180);

        solver.Maximize(40 * production[Gas] + 50 * production[Chloride]);

        Console.WriteLine("NumConstraints: {0}", solver.NumConstraints());

        Solver.ResultStatus resultStatus = solver.Solve();

        if (resultStatus != Solver.ResultStatus.OPTIMAL)
        {
            Console.WriteLine("The problem don't have an optimal solution.");
            return;
        }

        foreach (int p in PRODUCTS)
        {
            Console.WriteLine("{0,-10}: {1} ReducedCost: {2}", products[p], production[p].SolutionValue(),
                              production[p].ReducedCost());
        }

        double[] activities = solver.ComputeConstraintActivities();
        foreach (int c in CONSTRAINTS)
        {
            Console.WriteLine("Constraint {0} DualValue {1} Activity: {2} lb: {3} ub: {4}", c.ToString(),
                              cons[c].DualValue(), activities[cons[c].Index()], cons[c].Lb(), cons[c].Ub());
        }

        Console.WriteLine("\nWallTime: " + solver.WallTime());
        Console.WriteLine("Iterations: " + solver.Iterations());
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
