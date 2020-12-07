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

public class SendMoreMoney
{
    /**
     *
     * Solve the SEND+MORE=MONEY problem
     * using scalar product.
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("SendMoreMoney");

        //
        // Decision variables
        //
        IntVar S = solver.MakeIntVar(0, 9, "S");
        IntVar E = solver.MakeIntVar(0, 9, "E");
        IntVar N = solver.MakeIntVar(0, 9, "N");
        IntVar D = solver.MakeIntVar(0, 9, "D");
        IntVar M = solver.MakeIntVar(0, 9, "M");
        IntVar O = solver.MakeIntVar(0, 9, "O");
        IntVar R = solver.MakeIntVar(0, 9, "R");
        IntVar Y = solver.MakeIntVar(0, 9, "Y");

        // for AllDifferent()
        IntVar[] x = new IntVar[] { S, E, N, D, M, O, R, Y };

        //
        // Constraints
        //
        solver.Add(x.AllDifferent());

        /*
        solver.Add(S*1000 + E*100 + N*10 + D + M*1000 + O*100 + R*10 + E ==
                   M*10000 + O*1000 + N*100 + E*10 + Y);
        */

        // Here we use scalar product instead.
        int[] s1 = new int[] { 1000, 100, 10, 1 };
        int[] s2 = new int[] { 10000, 1000, 100, 10, 1 };
        solver.Add(new IntVar[] { S, E, N, D }.ScalProd(s1) + new IntVar[] { M, O, R, E }.ScalProd(s1) ==
                   new IntVar[] { M, O, N, E, Y }.ScalProd(s2));

        solver.Add(S > 0);
        solver.Add(M > 0);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);
        while (solver.NextSolution())
        {
            for (int i = 0; i < 8; i++)
            {
                Console.Write(x[i].Value() + " ");
            }
            Console.WriteLine();
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0}", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
