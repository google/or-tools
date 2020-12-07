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

public class ToNumTest
{
    /**
     *
     *  toNum(solver, a, num, base)
     *
     *  channelling between the array a and the number num.
     *
     */
    private static Constraint ToNum(IntVar[] a, IntVar num, int bbase)
    {
        int len = a.Length;

        IntVar[] tmp = new IntVar[len];
        for (int i = 0; i < len; i++)
        {
            tmp[i] = (a[i] * (int)Math.Pow(bbase, (len - i - 1))).Var();
        }
        return tmp.Sum() == num;
    }

    /**
     *
     * Implements toNum: channeling between a number and an array.
     * See http://www.hakank.org/or-tools/toNum.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("ToNum");

        int n = 5;
        int bbase = 10;

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, bbase - 1, "x");
        IntVar num = solver.MakeIntVar(0, (int)Math.Pow(bbase, n) - 1, "num");

        //
        // Constraints
        //

        solver.Add(x.AllDifferent());
        solver.Add(ToNum(x, num, bbase));

        // extra constraint (just for fun)
        // second digit should be 7
        // solver.Add(x[1] == 7);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.Write("\n" + num.Value() + ": ");
            for (int i = 0; i < n; i++)
            {
                Console.Write(x[i].Value() + " ");
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
