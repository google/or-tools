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

public class Futoshiki
{
    /**
     *
     * Futoshiki problem.
     *
     * From http://en.wikipedia.org/wiki/Futoshiki
     * """
     * The puzzle is played on a square grid, such as 5 x 5. The objective
     * is to place the numbers 1 to 5 (or whatever the dimensions are)
     * such that each row, and column contains each of the digits 1 to 5.
     * Some digits may be given at the start. In addition, inequality
     * constraints are also initially specifed between some of the squares,
     * such that one must be higher or lower than its neighbour. These
     * constraints must be honoured as the grid is filled out.
     * """
     *
     * Also see http://www.hakank.org/or-tools/futoshiki.py
     *
     */
    private static void Solve(int[,] values, int[,] lt)
    {
        Solver solver = new Solver("Futoshiki");

        int size = values.GetLength(0);
        IEnumerable<int> RANGE = Enumerable.Range(0, size);
        IEnumerable<int> NUMQD = Enumerable.Range(0, lt.GetLength(0));

        //
        // Decision variables
        //
        IntVar[,] field = solver.MakeIntVarMatrix(size, size, 1, size, "field");
        IntVar[] field_flat = field.Flatten();

        //
        // Constraints
        //

        // set initial values
        foreach (int row in RANGE)
        {
            foreach (int col in RANGE)
            {
                if (values[row, col] > 0)
                {
                    solver.Add(field[row, col] == values[row, col]);
                }
            }
        }

        // all rows have to be different
        foreach (int row in RANGE)
        {
            solver.Add((from col in RANGE select field[row, col]).ToArray().AllDifferent());
        }

        // all columns have to be different
        foreach (int col in RANGE)
        {
            solver.Add((from row in RANGE select field[row, col]).ToArray().AllDifferent());
        }

        // all < constraints are satisfied
        // Also: make 0-based
        foreach (int i in NUMQD)
        {
            solver.Add(field[lt[i, 0] - 1, lt[i, 1] - 1] < field[lt[i, 2] - 1, lt[i, 3] - 1]);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(field_flat, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            foreach (int i in RANGE)
            {
                foreach (int j in RANGE)
                {
                    Console.Write("{0} ", field[i, j].Value());
                }
                Console.WriteLine();
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
        //
        // Example from Tailor model futoshiki.param/futoshiki.param
        // Solution:
        // 5 1 3 2 4
        // 1 4 2 5 3
        // 2 3 1 4 5
        // 3 5 4 1 2
        // 4 2 5 3 1
        //
        // Futoshiki instance, by Andras Salamon
        // specify the numbers in the grid
        //
        int[,] values1 = {
            { 0, 0, 3, 2, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0 }
        };

        // [i1,j1, i2,j2] requires that values[i1,j1] < values[i2,j2]
        // Note: 1-based
        int[,] lt1 = { { 1, 2, 1, 1 }, { 1, 4, 1, 5 }, { 2, 3, 1, 3 }, { 3, 3, 2, 3 }, { 3, 4, 2, 4 }, { 2, 5, 3, 5 },
                       { 3, 2, 4, 2 }, { 4, 4, 4, 3 }, { 5, 2, 5, 1 }, { 5, 4, 5, 3 }, { 5, 5, 4, 5 } };

        //
        // Example from http://en.wikipedia.org/wiki/Futoshiki
        // Solution:
        // 5 4 3 2 1
        // 4 3 1 5 2
        // 2 1 4 3 5
        // 3 5 2 1 4
        // 1 2 5 4 3
        //
        int[,] values2 = {
            { 0, 0, 0, 0, 0 }, { 4, 0, 0, 0, 2 }, { 0, 0, 4, 0, 0 }, { 0, 0, 0, 0, 4 }, { 0, 0, 0, 0, 0 }
        };

        // Note: 1-based
        int[,] lt2 = { { 1, 2, 1, 1 }, { 1, 4, 1, 3 }, { 1, 5, 1, 4 }, { 4, 4, 4, 5 }, { 5, 1, 5, 2 }, { 5, 2, 5, 3 } };

        Console.WriteLine("Problem 1");
        Solve(values1, lt1);

        Console.WriteLine("\nProblem 2");
        Solve(values2, lt2);
    }
}
