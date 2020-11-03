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

public class AllDifferentExcept0Test
{
    //
    // Decomposition of alldifferent_except_0
    //
    public static void AllDifferentExcept0(Solver solver, IntVar[] a)
    {
        int n = a.Length;
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < i; j++)
            {
                solver.Add((a[i] != 0) * (a[j] != 0) <= (a[i] != a[j]));
            }
        }
    }

    /**
     *
     * Decomposition of alldifferent_except_0
     *
     * See http://www.hakank.org/google_or_tools/map.py
     *
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("AllDifferentExcept0");

        //
        // data
        //
        int n = 6;

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, n - 1, "x");

        //
        // Constraints
        //
        AllDifferentExcept0(solver, x);

        // we also require at least 2 0's
        IntVar[] z_tmp = new IntVar[n];
        for (int i = 0; i < n; i++)
        {
            z_tmp[i] = x[i] == 0;
        }
        IntVar z = z_tmp.Sum().VarWithName("z");
        solver.Add(z == 2);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);
        while (solver.NextSolution())
        {
            Console.Write("z: {0}  x: ", z.Value());
            for (int i = 0; i < n; i++)
            {
                Console.Write("{0} ", x[i].Value());
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
