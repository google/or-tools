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

public class APuzzle
{
    /**
     *
     * From "God plays dice"
     * "A puzzle"
     * http://gottwurfelt.wordpress.com/2012/02/22/a-puzzle/
     * And the sequel "Answer to a puzzle"
     * http://gottwurfelt.wordpress.com/2012/02/24/an-answer-to-a-puzzle/
     *
     * This problem instance was taken from the latter blog post.
     * (Problem 1)
     *
     * """
     * 8809 = 6
     * 7111 = 0
     * 2172 = 0
     * 6666 = 4
     * 1111 = 0
     * 3213 = 0
     * 7662 = 2
     * 9312 = 1
     * 0000 = 4
     * 2222 = 0
     * 3333 = 0
     * 5555 = 0
     * 8193 = 3
     * 8096 = 5
     * 7777 = 0
     * 9999 = 4
     * 7756 = 1
     * 6855 = 3
     * 9881 = 5
     * 5531 = 0
     *
     * 2581 = ?
     * """
     *
     * Note:
     * This model yields 10 solutions, since x4 is not
     * restricted in the constraints.
     * All solutions has x assigned to the correct result.
     *
     *
     * (Problem 2)
     * The problem stated in "A puzzle"
     * http://gottwurfelt.wordpress.com/2012/02/22/a-puzzle/
     * is
     * """
     * 8809 = 6
     * 7662 = 2
     * 9312 = 1
     * 8193 = 3
     * 8096 = 5
     * 7756 = 1
     * 6855 = 3
     * 9881 = 5
     *
     * 2581 = ?
     * """
     * This problem instance yields two different solutions of x,
     * one is the same (correct) as for the above problem instance,
     * and one is not.
     * This is because here x0,x1,x4 and x9 are underdefined.
     *
     *
     */
    private static void Solve(int p = 1)
    {
        Solver solver = new Solver("APuzzle");

        Console.WriteLine("\nSolving p{0}", p);

        //
        // Data
        //
        int n = 10;

        //
        // Decision variables
        //
        IntVar x0 = solver.MakeIntVar(0, n - 1, "x0");
        IntVar x1 = solver.MakeIntVar(0, n - 1, "x1");
        IntVar x2 = solver.MakeIntVar(0, n - 1, "x2");
        IntVar x3 = solver.MakeIntVar(0, n - 1, "x3");
        IntVar x4 = solver.MakeIntVar(0, n - 1, "x4");
        IntVar x5 = solver.MakeIntVar(0, n - 1, "x5");
        IntVar x6 = solver.MakeIntVar(0, n - 1, "x6");
        IntVar x7 = solver.MakeIntVar(0, n - 1, "x7");
        IntVar x8 = solver.MakeIntVar(0, n - 1, "x8");
        IntVar x9 = solver.MakeIntVar(0, n - 1, "x9");

        IntVar[] all = { x0, x1, x2, x3, x4, x5, x6, x7, x8, x9 };

        // The unknown, i.e. 2581 = x
        IntVar x = solver.MakeIntVar(0, n - 1, "x");

        //
        // Constraints
        //

        // Both problem are here shown in two
        // approaches:
        //   - using equations
        //   - using a a matrix and Sum of each row

        if (p == 1)
        {
            // Problem 1
            solver.Add(x8 + x8 + x0 + x9 == 6);
            solver.Add(x7 + x1 + x1 + x1 == 0);
            solver.Add(x2 + x1 + x7 + x2 == 0);
            solver.Add(x6 + x6 + x6 + x6 == 4);
            solver.Add(x1 + x1 + x1 + x1 == 0);
            solver.Add(x3 + x2 + x1 + x3 == 0);
            solver.Add(x7 + x6 + x6 + x2 == 2);
            solver.Add(x9 + x3 + x1 + x2 == 1);
            solver.Add(x0 + x0 + x0 + x0 == 4);
            solver.Add(x2 + x2 + x2 + x2 == 0);
            solver.Add(x3 + x3 + x3 + x3 == 0);
            solver.Add(x5 + x5 + x5 + x5 == 0);
            solver.Add(x8 + x1 + x9 + x3 == 3);
            solver.Add(x8 + x0 + x9 + x6 == 5);
            solver.Add(x7 + x7 + x7 + x7 == 0);
            solver.Add(x9 + x9 + x9 + x9 == 4);
            solver.Add(x7 + x7 + x5 + x6 == 1);
            solver.Add(x6 + x8 + x5 + x5 == 3);
            solver.Add(x9 + x8 + x8 + x1 == 5);
            solver.Add(x5 + x5 + x3 + x1 == 0);

            // The unknown
            solver.Add(x2 + x5 + x8 + x1 == x);
        }
        else if (p == 2)
        {
            // Another representation of Problem 1
            int[,] problem1 = { { 8, 8, 0, 9, 6 }, { 7, 1, 1, 1, 0 }, { 2, 1, 7, 2, 0 }, { 6, 6, 6, 6, 4 },
                                { 1, 1, 1, 1, 0 }, { 3, 2, 1, 3, 0 }, { 7, 6, 6, 2, 2 }, { 9, 3, 1, 2, 1 },
                                { 0, 0, 0, 0, 4 }, { 2, 2, 2, 2, 0 }, { 3, 3, 3, 3, 0 }, { 5, 5, 5, 5, 0 },
                                { 8, 1, 9, 3, 3 }, { 8, 0, 9, 6, 5 }, { 7, 7, 7, 7, 0 }, { 9, 9, 9, 9, 4 },
                                { 7, 7, 5, 6, 1 }, { 6, 8, 5, 5, 3 }, { 9, 8, 8, 1, 5 }, { 5, 5, 3, 1, 0 } };

            for (int i = 0; i < problem1.GetLength(0); i++)
            {
                solver.Add((from j in Enumerable.Range(0, 4) select all[problem1[i, j]]).ToArray().Sum() ==
                           problem1[i, 4]);
            }

            solver.Add(all[2] + all[5] + all[8] + all[1] == x);
        }
        else if (p == 3)
        {
            // Problem 2
            solver.Add(x8 + x8 + x0 + x9 == 6);
            solver.Add(x7 + x6 + x6 + x2 == 2);
            solver.Add(x9 + x3 + x1 + x2 == 1);
            solver.Add(x8 + x1 + x9 + x3 == 3);
            solver.Add(x8 + x0 + x9 + x6 == 5);
            solver.Add(x7 + x7 + x5 + x6 == 1);
            solver.Add(x6 + x8 + x5 + x5 == 3);
            solver.Add(x9 + x8 + x8 + x1 == 5);

            // The unknown
            solver.Add(x2 + x5 + x8 + x1 == x);
        }
        else
        {
            // Another representation of Problem 2
            int[,] problem2 = { { 8, 8, 0, 9, 6 }, { 7, 6, 6, 2, 2 }, { 9, 3, 1, 2, 1 }, { 8, 1, 9, 3, 3 },
                                { 8, 0, 9, 6, 5 }, { 7, 7, 5, 6, 1 }, { 6, 8, 5, 5, 3 }, { 9, 8, 8, 1, 5 } };

            for (int i = 0; i < problem2.GetLength(0); i++)
            {
                solver.Add((from j in Enumerable.Range(0, 4) select all[problem2[i, j]]).ToArray().Sum() ==
                           problem2[i, 4]);
            }

            solver.Add(all[2] + all[5] + all[8] + all[1] == x);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(all, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);
        while (solver.NextSolution())
        {
            Console.Write("x: {0}  x0..x9: ", x.Value());
            for (int i = 0; i < n; i++)
            {
                Console.Write(all[i].Value() + " ");
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
        for (int p = 1; p <= 4; p++)
        {
            Solve(p);
        }
    }
}
