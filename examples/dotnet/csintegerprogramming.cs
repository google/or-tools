// Copyright 2010-2021 Google LLC
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
using Google.OrTools.Init;
using Google.OrTools.LinearSolver;

public class CsIntegerProgramming
{
    private static void RunIntegerProgrammingExample(String solverType)
    {
        Solver solver = Solver.CreateSolver(solverType);
        if (solver == null)
        {
            Console.WriteLine("Could not create solver " + solverType);
            return;
        }
        // x1 and x2 are integer non-negative variables.
        Variable x1 = solver.MakeIntVar(0.0, double.PositiveInfinity, "x1");
        Variable x2 = solver.MakeIntVar(0.0, double.PositiveInfinity, "x2");

        // Minimize x1 + 2 * x2.
        Objective objective = solver.Objective();
        objective.SetMinimization();
        objective.SetCoefficient(x1, 1);
        objective.SetCoefficient(x2, 2);

        // 2 * x2 + 3 * x1 >= 17.
        Constraint ct = solver.MakeConstraint(17, double.PositiveInfinity);
        ct.SetCoefficient(x1, 3);
        ct.SetCoefficient(x2, 2);

        Solver.ResultStatus resultStatus = solver.Solve();

        // Check that the problem has an optimal solution.
        if (resultStatus != Solver.ResultStatus.OPTIMAL)
        {
            Console.WriteLine("The problem does not have an optimal solution!");
            return;
        }

        Console.WriteLine("Problem solved in " + solver.WallTime() + " milliseconds");

        // The objective value of the solution.
        Console.WriteLine("Optimal objective value = " + objective.Value());

        // The value of each variable in the solution.
        Console.WriteLine("x1 = " + x1.SolutionValue());
        Console.WriteLine("x2 = " + x2.SolutionValue());

        Console.WriteLine("Advanced usage:");
        Console.WriteLine("Problem solved in " + solver.Nodes() + " branch-and-bound nodes");
    }

    private static void RunIntegerProgrammingExampleNaturalApi(String solverType)
    {
        Solver solver = Solver.CreateSolver(solverType);
        if (solver == null)
        {
            Console.WriteLine("Could not create solver " + solverType);
            return;
        }
        // x1 and x2 are integer non-negative variables.
        Variable x1 = solver.MakeIntVar(0.0, double.PositiveInfinity, "x1");
        Variable x2 = solver.MakeIntVar(0.0, double.PositiveInfinity, "x2");

        solver.Minimize(x1 + 2 * x2);
        solver.Add(2 * x2 + 3 * x1 >= 17);

        Solver.ResultStatus resultStatus = solver.Solve();

        // Check that the problem has an optimal solution.
        if (resultStatus != Solver.ResultStatus.OPTIMAL)
        {
            Console.WriteLine("The problem does not have an optimal solution!");
            return;
        }

        Console.WriteLine("Problem solved in " + solver.WallTime() + " milliseconds");

        // The objective value of the solution.
        Console.WriteLine("Optimal objective value = " + solver.Objective().Value());

        // The value of each variable in the solution.
        Console.WriteLine("x1 = " + x1.SolutionValue());
        Console.WriteLine("x2 = " + x2.SolutionValue());

        Console.WriteLine("Advanced usage:");
        Console.WriteLine("Problem solved in " + solver.Nodes() + " branch-and-bound nodes");
    }

    static void Main()
    {
        CppBridge.InitLogging("csintegerprogramming.cs");
        CppFlags flags = new CppFlags();
        flags.logtostderr = true;
        flags.log_prefix = false;
        CppBridge.SetFlags(flags);

        Console.WriteLine("---- Integer programming example with GLPK ----");
        RunIntegerProgrammingExample("GLPK");
        Console.WriteLine("---- Linear programming example with CBC ----");
        RunIntegerProgrammingExample("CBC");
        Console.WriteLine("---- Linear programming example with SCIP ----");
        RunIntegerProgrammingExample("SCIP");
        Console.WriteLine("---- Linear programming example with SAT ----");
        RunIntegerProgrammingExample("SAT");
        Console.WriteLine("---- Linear programming example with GUROBI ----");
        RunIntegerProgrammingExample("GUROBI");
        Console.WriteLine("---- Integer programming example (Natural API) with GLPK ----");
        RunIntegerProgrammingExampleNaturalApi("GLPK");
        Console.WriteLine("---- Linear programming example (Natural API) with CBC ----");
        RunIntegerProgrammingExampleNaturalApi("CBC");
        Console.WriteLine("---- Linear programming example (Natural API) with SCIP ----");
        RunIntegerProgrammingExampleNaturalApi("SCIP");
        Console.WriteLine("---- Linear programming example (Natural API) with SAT ----");
        RunIntegerProgrammingExampleNaturalApi("SAT");
        Console.WriteLine("---- Linear programming example (Natural API) with GUROBI ----");
        RunIntegerProgrammingExampleNaturalApi("GUROBI");
    }
}
