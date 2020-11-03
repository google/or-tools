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

public class PerfectSquareSequence
{
    /**
     *
     * Perfect square sequence.
     *
     * From 'Fun with num3ers'
     * "Sequence"
     * http://benvitale-funwithnum3ers.blogspot.com/2010/11/sequence.html
     * """
     * If we take the numbers from 1 to 15
     *    (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)
     * and rearrange them in such an order that any two consecutive
     * numbers in the sequence add up to a perfect square, we get,
     *
     *  8     1     15     10     6     3     13     12      4      5     11 14 2
     * 7      9 9    16    25     16     9     16     25     16     9     16 25 16
     * 9     16
     *
     *
     * I ask the readers the following:
     *
     * Can you take the numbers from 1 to 25 to produce such an arrangement?
     * How about the numbers from 1 to 100?
     * """
     *
     * Via http://wildaboutmath.com/2010/11/26/wild-about-math-bloggers-111910
     *
     *
     * Also see http://www.hakank.org/or-tools/perfect_square_sequence.py
     *
     */
    private static int Solve(int n = 15, int print_solutions = 1, int show_num_sols = 0)
    {
        Solver solver = new Solver("PerfectSquareSequence");

        IEnumerable<int> RANGE = Enumerable.Range(0, n);

        // create the table of possible squares
        int[] squares = new int[n - 1];
        for (int i = 1; i < n; i++)
        {
            squares[i - 1] = i * i;
        }

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 1, n, "x");

        //
        // Constraints
        //

        solver.Add(x.AllDifferent());

        for (int i = 1; i < n; i++)
        {
            solver.Add((x[i - 1] + x[i]).Member(squares));
        }

        // symmetry breaking
        solver.Add(x[0] < x[n - 1]);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        int num_solutions = 0;
        while (solver.NextSolution())
        {
            num_solutions++;
            if (print_solutions > 0)
            {
                Console.Write("x:  ");
                foreach (int i in RANGE)
                {
                    Console.Write(x[i].Value() + " ");
                }
                Console.WriteLine();
            }

            if (show_num_sols > 0 && num_solutions >= show_num_sols)
            {
                break;
            }
        }

        if (print_solutions > 0)
        {
            Console.WriteLine("\nSolutions: {0}", solver.Solutions());
            Console.WriteLine("WallTime: {0}ms", solver.WallTime());
            Console.WriteLine("Failures: {0}", solver.Failures());
            Console.WriteLine("Branches: {0} ", solver.Branches());
        }

        solver.EndSearch();

        return num_solutions;
    }

    public static void Main(String[] args)
    {
        int n = 15;
        if (args.Length > 0)
        {
            n = Convert.ToInt32(args[0]);
        }

        if (n == 0)
        {
            for (int i = 2; i < 100; i++)
            {
                int num_solutions = Solve(i, 0, 0);
                Console.WriteLine("{0}: {1} solution(s)", i, num_solutions);
            }
        }
        else
        {
            int num_solutions = Solve(n);
            Console.WriteLine("{0}: {1} solution(s)", n, num_solutions);
        }
    }
}
