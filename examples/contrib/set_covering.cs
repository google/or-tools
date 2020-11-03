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
using System.Linq;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class SetCovering
{
    /**
     *
     * Solves a set covering problem.
     * See  See http://www.hakank.org/or-tools/set_covering.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("SetCovering");

        //
        // data
        //

        // Placing of firestations, from Winston 'Operations Research',
        // page 486.
        int min_distance = 15;
        int num_cities = 6;

        int[,] distance = { { 0, 10, 20, 30, 30, 20 }, { 10, 0, 25, 35, 20, 10 }, { 20, 25, 0, 15, 30, 20 },
                            { 30, 35, 15, 0, 15, 25 }, { 30, 20, 30, 15, 0, 14 }, { 20, 10, 20, 25, 14, 0 } };

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(num_cities, 0, 1, "x");
        IntVar z = x.Sum().Var();

        //
        // Constraints
        //

        // ensure that all cities are covered
        for (int i = 0; i < num_cities; i++)
        {
      IntVar[] b = (from j in Enumerable.Range(0, num_cities)
                        where distance[i, j] <= min_distance select x[j])
                       .ToArray();
      solver.Add(b.Sum() >= 1);
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
            for (int i = 0; i < num_cities; i++)
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
