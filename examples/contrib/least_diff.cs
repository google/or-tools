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

public class LeastDiff
{
    /**
     *
     * Solve the Least diff problem
     * For more info, see http://www.hakank.org/google_or_tools/least_diff.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("LeastDiff");

        //
        // Decision variables
        //
        IntVar A = solver.MakeIntVar(0, 9, "A");
        IntVar B = solver.MakeIntVar(0, 9, "B");
        IntVar C = solver.MakeIntVar(0, 9, "C");
        IntVar D = solver.MakeIntVar(0, 9, "D");
        IntVar E = solver.MakeIntVar(0, 9, "E");
        IntVar F = solver.MakeIntVar(0, 9, "F");
        IntVar G = solver.MakeIntVar(0, 9, "G");
        IntVar H = solver.MakeIntVar(0, 9, "H");
        IntVar I = solver.MakeIntVar(0, 9, "I");
        IntVar J = solver.MakeIntVar(0, 9, "J");

        IntVar[] all = new IntVar[] { A, B, C, D, E, F, G, H, I, J };
        int[] coeffs = { 10000, 1000, 100, 10, 1 };
        IntVar x = new IntVar[] { A, B, C, D, E }.ScalProd(coeffs).Var();
        IntVar y = new IntVar[] { F, G, H, I, J }.ScalProd(coeffs).Var();
        IntVar diff = (x - y).VarWithName("diff");

        //
        // Constraints
        //
        solver.Add(all.AllDifferent());
        solver.Add(A > 0);
        solver.Add(F > 0);
        solver.Add(diff > 0);

        //
        // Objective
        //
        OptimizeVar obj = diff.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(all, Solver.CHOOSE_PATH, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db, obj);
        while (solver.NextSolution())
        {
            Console.WriteLine("{0} - {1} = {2}  ({3}", x.Value(), y.Value(), diff.Value(), diff.ToString());
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
