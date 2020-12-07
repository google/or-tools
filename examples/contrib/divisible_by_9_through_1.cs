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

public class DivisibleBy9Through1
{
    /**
     *
     * A simple propagator for modulo constraint.
     *
     * This implementation is based on the ECLiPSe version
     * mentioned in "A Modulo propagator for ECLiPSE"
     * http://www.hakank.org/constraint_programming_blog/2010/05/a_modulo_propagator_for_eclips.html
     * The ECLiPSe Prolog source code:
     * http://www.hakank.org/eclipse/modulo_propagator.ecl
     *
     */
    public static void MyMod(Solver solver, IntVar x, IntVar y, IntVar r)
    {
        long lbx = x.Min();
        long ubx = x.Max();
        long ubx_neg = -ubx;
        long lbx_neg = -lbx;
        int min_x = (int)Math.Min(lbx, ubx_neg);
        int max_x = (int)Math.Max(ubx, lbx_neg);

        IntVar d = solver.MakeIntVar(min_x, max_x, "d");

        // r >= 0
        solver.Add(r >= 0);

        // x*r >= 0
        solver.Add(x * r >= 0);

        // -abs(y) < r
        solver.Add(-y.Abs() < r);

        // r < abs(y)
        solver.Add(r < y.Abs());

        // min_x <= d, i.e. d > min_x
        solver.Add(d > min_x);

        // d <= max_x
        solver.Add(d <= max_x);

        // x == y*d+r
        solver.Add(x - (y * d + r) == 0);
    }

    /**
     *
     *  ToNum(solver, a, num, base)
     *
     *  channelling between the array a and the number num
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
     * Solves the divisible by 9 through 1 problem.
     * See http://www.hakank.org/google_or_tools/divisible_by_9_through_1.py
     *
     */
    private static void Solve(int bbase)
    {
        Solver solver = new Solver("DivisibleBy9Through1");

        int m = (int)Math.Pow(bbase, (bbase - 1)) - 1;
        int n = bbase - 1;

        String[] digits_str = { "_", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

        Console.WriteLine("base: " + bbase);

        //
        // Decision variables
        //
        // digits
        IntVar[] x = solver.MakeIntVarArray(n, 1, bbase - 1, "x");

        // the numbers. t[0] contains the answe
        IntVar[] t = solver.MakeIntVarArray(n, 0, m, "t");

        //
        // Constraints
        //

        solver.Add(x.AllDifferent());

        // Ensure the divisibility of base .. 1
        IntVar zero = solver.MakeIntConst(0);
        for (int i = 0; i < n; i++)
        {
            int mm = bbase - i - 1;
            IntVar[] tt = new IntVar[mm];
            for (int j = 0; j < mm; j++)
            {
                tt[j] = x[j];
            }
            solver.Add(ToNum(tt, t[i], bbase));
            MyMod(solver, t[i], solver.MakeIntConst(mm), zero);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.Write("x: ");
            for (int i = 0; i < n; i++)
            {
                Console.Write(x[i].Value() + " ");
            }
            Console.WriteLine("\nt: ");
            for (int i = 0; i < n; i++)
            {
                Console.Write(t[i].Value() + " ");
            }
            Console.WriteLine("\n");

            if (bbase != 10)
            {
                Console.Write("Number base 10: " + t[0].Value());
                Console.Write(" Base " + bbase + ": ");
                for (int i = 0; i < n; i++)
                {
                    Console.Write(digits_str[(int)x[i].Value() + 1]);
                }
                Console.WriteLine("\n");
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
        int bbase = 10;
        if (args.Length > 0)
        {
            bbase = Convert.ToInt32(args[0]);
            if (bbase > 12)
            {
                // Though base = 12 has no solution...
                Console.WriteLine("Sorry, max relevant base is 12. Setting base to 12.");
                bbase = 10;
            }
        }

        Solve(bbase);
    }
}
