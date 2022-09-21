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

public class CircuitTest2
{
    /**
     * circuit(solver, x, z)
     *
     * A decomposition of the global constraint circuit, based
     * on some observation of the orbits in an array.
     *
     * This version also exposes z (the path) to the public.
     *
     * Note: The domain of x must be 0..n-1 (not 1..n)
     * since C# is 0-based.
     */
    public static void circuit(Solver solver, IntVar[] x, IntVar[] z)
    {
        int n = x.Length;

        solver.Add(x.AllDifferent());
        solver.Add(z.AllDifferent());

        // put the orbit of x[0] in z[0..n-1]
        solver.Add(z[0] == x[0]);
        for (int i = 1; i < n - 1; i++)
        {
            solver.Add(z[i] == x.Element(z[i - 1]));
        }

        // z may not be 0 for i < n-1
        for (int i = 1; i < n - 1; i++)
        {
            solver.Add(z[i] != 0);
        }

        // when i = n-1 it must be 0
        solver.Add(z[n - 1] == 0);
    }

    /**
     *
     * Implements a (decomposition) of the global constraint circuit
     * and extracting the path.
     *
     * One circuit for n = 5 is    3 0 4 2 1
     * Thus the extracted path is  0 -> 3 -> 2 -> 4 -> 1 -> 0
     *
     */
    private static void Solve(int n = 5)
    {
        Solver solver = new Solver("CircuitTest2");

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, n - 1, "x");
        IntVar[] path = solver.MakeIntVarArray(n, 0, n - 1, "path");

        //
        // Constraints
        //
        circuit(solver, x, path);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.Write("x   : ");
            for (int i = 0; i < n; i++)
            {
                Console.Write("{0} ", x[i].Value());
            }
            Console.Write("\npath: ");
            for (int i = 0; i < n; i++)
            {
                Console.Write("{0} ", path[i].Value());
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
        int n = 5;
        if (args.Length > 1)
        {
            n = Convert.ToInt32(args[1]);
        }

        Solve(n);
    }
}
