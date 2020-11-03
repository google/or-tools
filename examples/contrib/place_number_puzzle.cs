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
using System.Collections;
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;

public class PlaceNumberPuzzle
{
    /**
     *
     * Place number puzzle.
     *
     * From
     * http://ai.uwaterloo.ca/~vanbeek/Courses/Slides/introduction.pdf
     * """
     * Place numbers 1 through 8 on nodes
     * - each number appears exactly once
     * - no connected nodes have consecutive numbers
     *       2 - 5
     *     / | X |                                \
     *   1 - 3 - 6 - 8
     *     \ | X | /
     *       4 - 7
     * """
     *
     * Also see http://www.hakank.org/or-tools/place_number_puzzle.py
     *

     */
    private static void Solve()
    {
        Solver solver = new Solver("PlaceNumberPuzzle");

        //
        // Data
        //
        int m = 32;
        int n = 8;

        // Note: this is 1-based for compatibility (and lazyness)
        int[,] graph = { { 1, 2 }, { 1, 3 }, { 1, 4 }, { 2, 1 }, { 2, 3 }, { 2, 5 }, { 2, 6 }, { 3, 2 },
                         { 3, 4 }, { 3, 6 }, { 3, 7 }, { 4, 1 }, { 4, 3 }, { 4, 6 }, { 4, 7 }, { 5, 2 },
                         { 5, 3 }, { 5, 6 }, { 5, 8 }, { 6, 2 }, { 6, 3 }, { 6, 4 }, { 6, 5 }, { 6, 7 },
                         { 6, 8 }, { 7, 3 }, { 7, 4 }, { 7, 6 }, { 7, 8 }, { 8, 5 }, { 8, 6 }, { 8, 7 } };

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 1, n, "x");

        //
        // Constraints
        //
        solver.Add(x.AllDifferent());
        for (int i = 0; i < m; i++)
        {
            // (also base 0-base)
            solver.Add((x[graph[i, 0] - 1] - x[graph[i, 1] - 1]).Abs() > 1);
        }

        // symmetry breaking
        solver.Add(x[0] < x[n - 1]);

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
