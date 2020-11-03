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
using System.IO;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class SetCovering2
{
    /**
     *
     * Solves a set covering problem.
     * See  See http://www.hakank.org/or-tools/set_covering2.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("SetCovering2");

        //
        // data
        //

        // Example 9.1-2 from
        // Taha "Operations Research - An Introduction",
        // page 354ff.
        // Minimize the number of security telephones in street
        // corners on a campus.

        int n = 8;            // maximum number of corners
        int num_streets = 11; // number of connected streets

        // corners of each street
        // Note: 1-based (handled below)
        int[,] corner = { { 1, 2 }, { 2, 3 }, { 4, 5 }, { 7, 8 }, { 6, 7 }, { 2, 6 },
                          { 1, 6 }, { 4, 7 }, { 2, 4 }, { 5, 8 }, { 3, 5 } };

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, 1, "x");
        // number of telephones, to be minimized
        IntVar z = x.Sum().Var();

        //
        // Constraints
        //

        // ensure that all streets are covered
        for (int i = 0; i < num_streets; i++)
        {
            solver.Add(x[corner[i, 0] - 1] + x[corner[i, 1] - 1] >= 1);
        }

        //
        // objective
        //
        OptimizeVar objective = z.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db, objective);

        while (solver.NextSolution())
        {
            Console.WriteLine("z: {0}", z.Value());
            Console.Write("x: ");
            for (int i = 0; i < n; i++)
            {
                Console.Write(x[i].Value() + " ");
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
