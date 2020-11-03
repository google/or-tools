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

public class Assignment
{
    /**
     *
     * Assignment problem
     *
     * From Wayne Winston "Operations Research",
     * Assignment Problems, page 393f
     * (generalized version with added test column)
     *
     * See  See http://www.hakank.org/or-tools/assignment.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Assignment");

        //
        // data
        //

        // Problem instance
        // hakank: I added the fifth column to make it more
        //         interesting
        int rows = 4;
        int cols = 5;
        int[,] cost = { { 14, 5, 8, 7, 15 }, { 2, 12, 6, 5, 3 }, { 7, 8, 3, 9, 7 }, { 2, 4, 6, 10, 1 } };

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeBoolVarMatrix(rows, cols, "x");
        IntVar[] x_flat = x.Flatten();

        //
        // Constraints
        //

        // Exacly one assignment per row (task),
        // i.e. all rows must be assigned with one worker
        for (int i = 0; i < rows; i++)
        {
            solver.Add((from j in Enumerable.Range(0, cols) select x[i, j]).ToArray().Sum() == 1);
        }

        // At most one assignments per column (worker)
        for (int j = 0; j < cols; j++)
        {
            solver.Add((from i in Enumerable.Range(0, rows) select x[i, j]).ToArray().Sum() <= 1);
        }

        // Total cost
        IntVar total_cost =
            (from i in Enumerable.Range(0, rows) from j in Enumerable.Range(0, cols) select(cost[i, j] * x[i, j]))
                .ToArray()
                .Sum()
                .Var();

        //
        // objective
        //
        OptimizeVar objective = total_cost.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db, objective);

        while (solver.NextSolution())
        {
            Console.WriteLine("total_cost: {0}", total_cost.Value());
            for (int i = 0; i < rows; i++)
            {
                for (int j = 0; j < cols; j++)
                {
                    Console.Write(x[i, j].Value() + " ");
                }
                Console.WriteLine();
            }
            Console.WriteLine();
            Console.WriteLine("Assignments:");
            for (int i = 0; i < rows; i++)
            {
                Console.Write("Task " + i);
                for (int j = 0; j < cols; j++)
                {
                    if (x[i, j].Value() == 1)
                    {
                        Console.WriteLine(" is done by " + j);
                    }
                }
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
        Solve();
    }
}
