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

public class Eq10
{
    /**
     *
     * Eq 10 in Google CP Solver.
     *
     * Standard benchmark problem.
     *
     * Also see http://hakank.org/or-tools/eq10.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Eq10");

        int n = 7;

        //
        // Decision variables
        //
        IntVar X1 = solver.MakeIntVar(0, 10, "X1");
        IntVar X2 = solver.MakeIntVar(0, 10, "X2");
        IntVar X3 = solver.MakeIntVar(0, 10, "X3");
        IntVar X4 = solver.MakeIntVar(0, 10, "X4");
        IntVar X5 = solver.MakeIntVar(0, 10, "X5");
        IntVar X6 = solver.MakeIntVar(0, 10, "X6");
        IntVar X7 = solver.MakeIntVar(0, 10, "X7");

        IntVar[] X = { X1, X2, X3, X4, X5, X6, X7 };

        //
        // Constraints
        //
        solver.Add(0 + 98527 * X1 + 34588 * X2 + 5872 * X3 + 59422 * X5 + 65159 * X7 ==
                   1547604 + 30704 * X4 + 29649 * X6);

        solver.Add(0 + 98957 * X2 + 83634 * X3 + 69966 * X4 + 62038 * X5 + 37164 * X6 + 85413 * X7 ==
                   1823553 + 93989 * X1);

        solver.Add(900032 + 10949 * X1 + 77761 * X2 + 67052 * X5 ==
                   0 + 80197 * X3 + 61944 * X4 + 92964 * X6 + 44550 * X7);

        solver.Add(0 + 73947 * X1 + 84391 * X3 + 81310 * X5 ==
                   1164380 + 96253 * X2 + 44247 * X4 + 70582 * X6 + 33054 * X7);

        solver.Add(0 + 13057 * X3 + 42253 * X4 + 77527 * X5 + 96552 * X7 ==
                   1185471 + 60152 * X1 + 21103 * X2 + 97932 * X6);

        solver.Add(1394152 + 66920 * X1 + 55679 * X4 ==
                   0 + 64234 * X2 + 65337 * X3 + 45581 * X5 + 67707 * X6 + 98038 * X7);

        solver.Add(0 + 68550 * X1 + 27886 * X2 + 31716 * X3 + 73597 * X4 + 38835 * X7 ==
                   279091 + 88963 * X5 + 76391 * X6);

        solver.Add(0 + 76132 * X2 + 71860 * X3 + 22770 * X4 + 68211 * X5 + 78587 * X6 ==
                   480923 + 48224 * X1 + 82817 * X7);

        solver.Add(519878 + 94198 * X2 + 87234 * X3 + 37498 * X4 ==
                   0 + 71583 * X1 + 25728 * X5 + 25495 * X6 + 70023 * X7);

        solver.Add(361921 + 78693 * X1 + 38592 * X5 + 38478 * X6 ==
                   0 + 94129 * X2 + 43188 * X3 + 82528 * X4 + 69025 * X7);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(X, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            for (int i = 0; i < n; i++)
            {
                Console.Write(X[i].ToString() + " ");
            }
            Console.WriteLine();
        }

        Console.WriteLine("\nSolutions: " + solver.Solutions());
        Console.WriteLine("WallTime: " + solver.WallTime() + "ms ");
        Console.WriteLine("Failures: " + solver.Failures());
        Console.WriteLine("Branches: " + solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
