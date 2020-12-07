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

public class Volsay
{
    /**
     * Volsay problem.
     *
     * From the OPL model volsay.mod.
     *
     * Also see http://www.hakank.org/or-tools/volsay.py
     */
    private static void Solve()
    {
        Solver solver = new Solver("Volsay", Solver.OptimizationProblemType.CLP_LINEAR_PROGRAMMING);

        //
        // Variables
        //
        Variable Gas = solver.MakeNumVar(0, 100000, "Gas");
        Variable Chloride = solver.MakeNumVar(0, 100000, "Cloride");

        Constraint c1 = solver.Add(Gas + Chloride <= 50);
        Constraint c2 = solver.Add(3 * Gas + 4 * Chloride <= 180);

        solver.Maximize(40 * Gas + 50 * Chloride);

        Solver.ResultStatus resultStatus = solver.Solve();

        if (resultStatus != Solver.ResultStatus.OPTIMAL)
        {
            Console.WriteLine("The problem don't have an optimal solution.");
            return;
        }

        Console.WriteLine("Objective: {0}", solver.Objective().Value());

        Console.WriteLine("Gas      : {0} ReducedCost: {1}", Gas.SolutionValue(), Gas.ReducedCost());

        Console.WriteLine("Chloride : {0} ReducedCost: {1}", Chloride.SolutionValue(), Chloride.ReducedCost());

        double[] activities = solver.ComputeConstraintActivities();
        Console.WriteLine("c1       : DualValue: {0} Activity: {1}", c1.DualValue(), activities[c1.Index()]);

        Console.WriteLine("c2       : DualValue: {0} Activity: {1}", c2.DualValue(), activities[c2.Index()]);

        Console.WriteLine("\nWallTime: " + solver.WallTime());
        Console.WriteLine("Iterations: " + solver.Iterations());
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
