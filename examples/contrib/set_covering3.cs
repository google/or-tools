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

public class SetCovering3
{
    /**
     *
     * Solves a set covering problem.
     * See  See http://www.hakank.org/or-tools/set_covering3.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("SetCovering3");

        //
        // data
        //

        // Set covering problem from
        // Katta G. Murty: 'Optimization Models for Decision Making',
        // page 302f
        // http://ioe.engin.umich.edu/people/fac/books/murty/opti_model/junior-7.pdf
        int num_groups = 6;
        int num_senators = 10;

        // which group does a senator belong to?
        int[,] belongs = { { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 },   // 1 southern
                           { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1 },   // 2 northern
                           { 0, 1, 1, 0, 0, 0, 0, 1, 1, 1 },   // 3 liberals
                           { 1, 0, 0, 0, 1, 1, 1, 0, 0, 0 },   // 4 conservative
                           { 0, 0, 1, 1, 1, 1, 1, 0, 1, 0 },   // 5 democrats
                           { 1, 1, 0, 0, 0, 0, 0, 1, 0, 1 } }; // 6 republicans

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(num_senators, 0, 1, "x");
        // number of assigned senators, to be minimized
        IntVar z = x.Sum().Var();

        //
        // Constraints
        //

        // ensure that each group is covered by at least
        // one senator
        for (int i = 0; i < num_groups; i++)
        {
            IntVar[] b = new IntVar[num_senators];
            for (int j = 0; j < num_senators; j++)
            {
                b[j] = (x[j] * belongs[i, j]).Var();
            }
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
            Console.WriteLine("z: " + z.Value());
            Console.Write("x: ");
            for (int j = 0; j < num_senators; j++)
            {
                Console.Write(x[j].Value() + " ");
            }
            Console.WriteLine();

            // More details
            for (int j = 0; j < num_senators; j++)
            {
                if (x[j].Value() == 1)
                {
                    Console.Write("Senator " + (1 + j) + " belongs to these groups: ");
                    for (int i = 0; i < num_groups; i++)
                    {
                        if (belongs[i, j] == 1)
                        {
                            Console.Write((1 + i) + " ");
                        }
                    }
                    Console.WriteLine();
                }
            }
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
