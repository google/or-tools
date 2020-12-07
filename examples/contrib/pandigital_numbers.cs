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
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.ConstraintSolver;

public class PandigitalNumbers
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
            tmp[i] = (a[i] * (int)Math.Pow(bbase, len - i - 1)).Var();
        }
        return tmp.Sum() == num;
    }

    /**
     *
     * Pandigital numbers in Google CP Solver.
     *
     * From Albert H. Beiler 'Recreations in the Theory of Numbers',
     * quoted from http://www.worldofnumbers.com/ninedig1.htm
     * """
     * Chapter VIII : Digits - and the magic of 9
     *
     * The following curious table shows how to arrange the 9 digits so that
     * the product of 2 groups is equal to a number represented by the
     * remaining digits.
     *
     *   12 x 483 = 5796
     *   42 x 138 = 5796
     *   18 x 297 = 5346
     *   27 x 198 = 5346
     *   39 x 186 = 7254
     *   48 x 159 = 7632
     *   28 x 157 = 4396
     *   4 x 1738 = 6952
     *   4 x 1963 = 7852
     * """
     *
     * Also see MathWorld http://mathworld.wolfram.com/PandigitalNumber.html
     * """
     * A number is said to be pandigital if it contains each of the digits
     * from 0 to 9 (and whose leading digit must be nonzero). However,
     * "zeroless" pandigital quantities contain the digits 1 through 9.
     * Sometimes exclusivity is also required so that each digit is
     * restricted to appear exactly once.
     * """
     *
     * Wikipedia: http://en.wikipedia.org/wiki/Pandigital_number
     *
     *
     * Also see http://www.hakank.org/or-tools/pandigital_numbers.py
     *
     */
    private static void Solve(int bbase = 10, int start = 1, int len1 = 1, int len2 = 4)
    {
        Solver solver = new Solver("PandigitalNumbers");

        //
        // Data
        //
        int max_d = bbase - 1;
        int x_len = max_d + 1 - start;
        int max_num = (int)Math.Pow(bbase, 4) - 1;

        //
        // Decision variables
        //
        IntVar num1 = solver.MakeIntVar(1, max_num, "num1");
        IntVar num2 = solver.MakeIntVar(1, max_num, "num2");
        IntVar res = solver.MakeIntVar(1, max_num, "res");

        IntVar[] x = solver.MakeIntVarArray(x_len, start, max_d, "x");

        // for labeling
        IntVar[] all = new IntVar[x_len + 3];
        for (int i = 0; i < x_len; i++)
        {
            all[i] = x[i];
        }
        all[x_len] = num1;
        all[x_len + 1] = num2;
        all[x_len + 2] = res;

        //
        // Constraints
        //
        solver.Add(x.AllDifferent());

        solver.Add(ToNum((from i in Enumerable.Range(0, len1) select x[i]).ToArray(), num1, bbase));

        solver.Add(ToNum((from i in Enumerable.Range(len1, len2) select x[i]).ToArray(), num2, bbase));

        solver.Add(
            ToNum((from i in Enumerable.Range(len1 + len2, x_len - (len1 + len2)) select x[i]).ToArray(), res, bbase));

        solver.Add(num1 * num2 == res);

        // no number must start with 0
        solver.Add(x[0] > 0);
        solver.Add(x[len1] > 0);
        solver.Add(x[len1 + len2] > 0);

        // symmetry breaking
        solver.Add(num1 < num2);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(all, Solver.INT_VAR_SIMPLE, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.WriteLine("{0} * {1} = {2}", num1.Value(), num2.Value(), res.Value());
        }

        /*
        Console.WriteLine("\nSolutions: " + solver.Solutions());
        Console.WriteLine("WallTime: " + solver.WallTime() + "ms ");
        Console.WriteLine("Failures: " + solver.Failures());
        Console.WriteLine("Branches: " + solver.Branches());
        */

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        int bbase = 10;
        int start = 1;

        if (args.Length > 0)
        {
            bbase = Convert.ToInt32(args[0]);
        }

        if (args.Length > 1)
        {
            start = Convert.ToInt32(args[1]);
        }

        int x_len = bbase - 1 + 1 - start;
        for (int len1 = 0; len1 <= x_len; len1++)
        {
            for (int len2 = 0; len2 <= x_len; len2++)
            {
                if (x_len > len1 + len2 && len1 > 0 && len2 > 0)
                {
                    Solve(bbase, start, len1, len2);
                }
            }
        }
    }
}
