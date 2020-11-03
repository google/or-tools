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

public class SendMostMoney
{
    /**
     *
     * Solve the SEND+MOST=MONEY problem
     * where the object is to maximize MONEY
     * See http://www.hakank.org/google_or_tools/send_most_money.py
     *
     */
    private static long Solve(long MONEY)
    {
        Solver solver = new Solver("SendMostMoney");

        //
        // Decision variables
        //
        IntVar S = solver.MakeIntVar(0, 9, "S");
        IntVar E = solver.MakeIntVar(0, 9, "E");
        IntVar N = solver.MakeIntVar(0, 9, "N");
        IntVar D = solver.MakeIntVar(0, 9, "D");
        IntVar M = solver.MakeIntVar(0, 9, "M");
        IntVar O = solver.MakeIntVar(0, 9, "O");
        IntVar T = solver.MakeIntVar(0, 9, "T");
        IntVar Y = solver.MakeIntVar(0, 9, "Y");

        // for AllDifferent()
        IntVar[] x = new IntVar[] { S, E, N, D, M, O, T, Y };

        IntVar[] eq = { S, E, N, D, M, O, S, T, M, O, N, E, Y };
        int[] coeffs = {
            1000,   100,   10,   1,      //    S E N D +
            1000,   100,   10,   1,      //    M O S T
            -10000, -1000, -100, -10, -1 // == M O N E Y
        };
        solver.Add(eq.ScalProd(coeffs) == 0);

        // IntVar money = solver.MakeScalProd(new IntVar[] {M, O, N, E, Y},
        //                                    new int[] {10000, 1000, 100, 10,
        //                                    1}).Var();
        IntVar money = (new IntVar[] { M, O, N, E, Y }).ScalProd(new int[] { 10000, 1000, 100, 10, 1 }).Var();

        //
        // Constraints
        //
        solver.Add(x.AllDifferent());
        solver.Add(S > 0);
        solver.Add(M > 0);

        if (MONEY > 0)
        {
            solver.Add(money == MONEY);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        if (MONEY == 0)
        {
            OptimizeVar obj = money.Maximize(1);
            solver.NewSearch(db, obj);
        }
        else
        {
            solver.NewSearch(db);
        }

        long money_ret = 0;
        while (solver.NextSolution())
        {
            money_ret = money.Value();
            Console.WriteLine("money: {0}", money.Value());
            for (int i = 0; i < x.Length; i++)
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

        return money_ret;
    }

    public static void Main(String[] args)
    {
        Console.WriteLine("First get the max value of money:");
        long this_money = Solve(0);
        Console.WriteLine("\nThen we find all solutions for MONEY = {0}:", this_money);
        long tmp = Solve(this_money);
    }
}
