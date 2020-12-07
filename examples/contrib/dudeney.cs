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

public class DudeneyNumbers
{
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
     * Dudeney numbers
     * From Pierre Schaus blog post
     * Dudeney number
     * http://cp-is-fun.blogspot.com/2010/09/test-python.html
     * """
     * I discovered yesterday Dudeney Numbers
     * A Dudeney Numbers is a positive integer that is a perfect cube such that
     * the sum of its decimal digits is equal to the cube root of the number.
     * There are only six Dudeney Numbers and those are very easy to find with CP.
     * I made my first experience with google cp solver so find these numbers
     * (model below) and must say that I found it very convenient to build CP
     * models in python! When you take a close look at the line:
     *     solver.Add(sum([10**(n-i-1)*x[i] for i in range(n)]) == nb)
     * It is difficult to argue that it is very far from dedicated
     * optimization languages!
     * """
     *
     * Also see: http://en.wikipedia.org/wiki/Dudeney_number
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("DudeneyNumbers");

        //
        // data
        //
        int n = 6;

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, 9, "x");
        IntVar nb = solver.MakeIntVar(3, (int)Math.Pow(10, n), "nb");
        IntVar s = solver.MakeIntVar(1, 9 * n + 1, "s");

        //
        // Constraints
        //
        solver.Add(nb == s * s * s);
        solver.Add(x.Sum() == s);

        // solver.Add(ToNum(x, nb, 10));

        // alternative
        solver.Add(
            (from i in Enumerable.Range(0, n) select(x[i] * (int)Math.Pow(10, n - i - 1)).Var()).ToArray().Sum() == nb);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.WriteLine(nb.Value());
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
