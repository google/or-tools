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

public class CuriousSetOfIntegers
{
    public static void Decreasing(Solver solver, IntVar[] x)
    {
        for (int i = 0; i < x.Length - 1; i++)
        {
            solver.Add(x[i] <= x[i + 1]);
        }
    }

    /**
     *
     * Crypto problem in Google CP Solver.
     *
     * Martin Gardner (February 1967):
     * """
     * The integers 1,3,8, and 120 form a set with a remarkable property: the
     * product of any two integers is one less than a perfect square. Find
     * a fifth number that can be added to the set without destroying
     * this property.
     * """
     *
     * Also see, http://www.hakank.org/or-tools/curious_set_of_integers.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("CuriousSetOfIntegers");

        //
        // data
        //
        int n = 5;
        int max_val = 10000;

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, max_val, "x");

        //
        // Constraints
        //
        solver.Add(x.AllDifferent());

        for (int i = 0; i < n - 1; i++)
        {
            for (int j = i + 1; j < n; j++)
            {
                IntVar p = solver.MakeIntVar(0, max_val);
                solver.Add((p.Square() - 1) - (x[i] * x[j]) == 0);
            }
        }

        // Symmetry breaking
        Decreasing(solver, x);

        // This is the original problem
        // Which is the fifth number?
        int[] v = { 1, 3, 8, 120 };
        IntVar[] b = (from i in Enumerable.Range(0, n) select x[i].IsMember(v)).ToArray();
        solver.Add(b.Sum() == 4);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_MIN_SIZE_LOWEST_MIN, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
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
