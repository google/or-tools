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
using System.Linq;
using Google.OrTools.ConstraintSolver;

public class NQueens
{
    /**
     *
     * Solves the N-Queens problem.
     *
     * Syntax: nqueens.exe n num print
     * where
     *    n    : size of board
     *    num  : number of solutions to calculate
     *    print: print the results (if > 0)
     *
     */
    private static void Solve(int n = 8, int num = 0, int print = 1)
    {
        Solver solver = new Solver("N-Queens");

        //
        // Decision variables
        //
        IntVar[] q = solver.MakeIntVarArray(n, 0, n - 1, "q");

        //
        // Constraints
        //
        solver.Add(q.AllDifferent());

        IntVar[] q1 = new IntVar[n];
        IntVar[] q2 = new IntVar[n];
        for (int i = 0; i < n; i++)
        {
            q1[i] = (q[i] + i).Var();
            q2[i] = (q[i] - i).Var();
        }
        solver.Add(q1.AllDifferent());
        solver.Add(q2.AllDifferent());

        // Alternative version: it works as well but are not that clear
        /*
        solver.Add((from i in Enumerable.Range(0, n)
                    select (q[i] + i).Var()).ToArray().AllDifferent());

        solver.Add((from i in Enumerable.Range(0, n)
                    select (q[i] - i).Var()).ToArray().AllDifferent());
        */

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(q, Solver.CHOOSE_MIN_SIZE_LOWEST_MAX, Solver.ASSIGN_CENTER_VALUE);

        solver.NewSearch(db);
        int c = 0;
        while (solver.NextSolution())
        {
            if (print > 0)
            {
                for (int i = 0; i < n; i++)
                {
                    Console.Write("{0} ", q[i].Value());
                }

                Console.WriteLine();
            }
            c++;
            if (num > 0 && c >= num)
            {
                break;
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
        int n = 8;
        int num = 0;
        int print = 1;

        if (args.Length > 0)
        {
            n = Convert.ToInt32(args[0]);
        }

        if (args.Length > 1)
        {
            num = Convert.ToInt32(args[1]);
        }
        if (args.Length > 2)
        {
            print = Convert.ToInt32(args[2]);
        }

        Console.WriteLine("n: {0} num: {1} print: {2}", n, num, print);

        Solve(n, num, print);
    }
}
