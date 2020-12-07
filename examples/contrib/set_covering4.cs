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

public class SetCovering4
{
    /**
     *
     * Solves a set covering problem.
     * See  See http://www.hakank.org/or-tools/set_covering4.py
     *
     */
    private static void Solve(int set_partition)
    {
        Solver solver = new Solver("SetCovering4");

        //
        // data
        //

        // Set partition and set covering problem from
        // Example from the Swedish book
        // Lundgren, Roennqvist, Vaebrand
        // 'Optimeringslaera' (translation: 'Optimization theory'),
        // page 408.
        int num_alternatives = 10;
        int num_objects = 8;

        // costs for the alternatives
        int[] costs = { 19, 16, 18, 13, 15, 19, 15, 17, 16, 15 };

        // the alternatives, and their objects
        int[,] a = {                            // 1 2 3 4 5 6 7 8    the objects
                    { 1, 0, 0, 0, 0, 1, 0, 0 }, // alternative 1
                    { 0, 1, 0, 0, 0, 1, 0, 1 }, // alternative 2
                    { 1, 0, 0, 1, 0, 0, 1, 0 }, // alternative 3
                    { 0, 1, 1, 0, 1, 0, 0, 0 }, // alternative 4
                    { 0, 1, 0, 0, 1, 0, 0, 0 }, // alternative 5
                    { 0, 1, 1, 0, 0, 0, 0, 0 }, // alternative 6
                    { 0, 1, 1, 1, 0, 0, 0, 0 }, // alternative 7
                    { 0, 0, 0, 1, 1, 0, 0, 1 }, // alternative 8
                    { 0, 0, 1, 0, 0, 1, 0, 1 }, // alternative 9
                    { 1, 0, 0, 0, 0, 1, 1, 0 }
        }; // alternative 10

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(num_alternatives, 0, 1, "x");
        // number of assigned senators, to be minimized
        IntVar z = x.ScalProd(costs).VarWithName("z");

        //
        // Constraints
        //

        for (int j = 0; j < num_objects; j++)
        {
            IntVar[] b = new IntVar[num_alternatives];
            for (int i = 0; i < num_alternatives; i++)
            {
                b[i] = (x[i] * a[i, j]).Var();
            }

            if (set_partition == 1)
            {
                solver.Add(b.Sum() >= 1);
            }
            else
            {
                solver.Add(b.Sum() == 1);
            }
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
            Console.Write("Selected alternatives: ");
            for (int i = 0; i < num_alternatives; i++)
            {
                if (x[i].Value() == 1)
                {
                    Console.Write((i + 1) + " ");
                }
            }
            Console.WriteLine("\n");
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        Console.WriteLine("Set partition:");
        Solve(1);
        Console.WriteLine("\nSet covering:");
        Solve(0);
    }
}
