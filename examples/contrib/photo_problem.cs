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

public class PhotoProblem
{
    /**
     *
     * Photo problem.
     *
     * Problem statement from Mozart/Oz tutorial:
     * http://www.mozart-oz.org/home/doc/fdt/node37.html#section.reified.photo
     * """
     * Betty, Chris, Donald, Fred, Gary, Mary, and Paul want to align in one
     * row for taking a photo. Some of them have preferences next to whom
     * they want to stand:
     *
     *  1. Betty wants to stand next to Gary and Mary.
     *  2. Chris wants to stand next to Betty and Gary.
     * 3. Fred wants to stand next to Mary and Donald.
     * 4. Paul wants to stand next to Fred and Donald.
     *
     * Obviously, it is impossible to satisfy all preferences. Can you find
     * an alignment that maximizes the number of satisfied preferences?
     * """
     *
     *  Oz solution:
     *     6 # alignment(betty:5  chris:6  donald:1  fred:3  gary:7   mary:4
     * paul:2) [5, 6, 1, 3, 7, 4, 2]
     *
     *
     * Also see http://www.hakank.org/or-tools/photo_problem.py
     *
     */
    private static void Solve(int show_all_max = 0)
    {
        Solver solver = new Solver("PhotoProblem");

        //
        // Data
        //
        String[] persons = { "Betty", "Chris", "Donald", "Fred", "Gary", "Mary", "Paul" };
        int n = persons.Length;
        IEnumerable<int> RANGE = Enumerable.Range(0, n);

        int[,] preferences = {
            // 0 1 2 3 4 5 6
            // B C D F G M P
            { 0, 0, 0, 0, 1, 1, 0 }, // Betty  0
            { 1, 0, 0, 0, 1, 0, 0 }, // Chris  1
            { 0, 0, 0, 0, 0, 0, 0 }, // Donald 2
            { 0, 0, 1, 0, 0, 1, 0 }, // Fred   3
            { 0, 0, 0, 0, 0, 0, 0 }, // Gary   4
            { 0, 0, 0, 0, 0, 0, 0 }, // Mary   5
            { 0, 0, 1, 1, 0, 0, 0 }  // Paul   6
        };

        Console.WriteLine("Preferences:");
        Console.WriteLine("1. Betty wants to stand next to Gary and Mary.");
        Console.WriteLine("2. Chris wants to stand next to Betty and Gary.");
        Console.WriteLine("3. Fred wants to stand next to Mary and Donald.");
        Console.WriteLine("4. Paul wants to stand next to Fred and Donald.\n");

        //
        // Decision variables
        //
        IntVar[] positions = solver.MakeIntVarArray(n, 0, n - 1, "positions");
        // successful preferences (to Maximize)
        IntVar z = solver.MakeIntVar(0, n * n, "z");

        //
        // Constraints
        //
        solver.Add(positions.AllDifferent());

        // calculate all the successful preferences
    solver.Add((from i in RANGE from j in RANGE where preferences[i, j] ==
                1 select(positions[i] - positions[j]).Abs() == 1)
                   .ToArray()
                   .Sum() == z);

    //
    // Symmetry breaking (from the Oz page):
    //    Fred is somewhere left of Betty
    solver.Add(positions[3] < positions[0]);

    //
    // Objective
    //
    OptimizeVar obj = z.Maximize(1);

    if (show_all_max > 0)
    {
        Console.WriteLine("Showing all maximum solutions (z == 6).\n");
        solver.Add(z == 6);
    }

    //
    // Search
    //
    DecisionBuilder db = solver.MakePhase(positions, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MAX_VALUE);

    solver.NewSearch(db, obj);

    while (solver.NextSolution())
    {
        Console.WriteLine("z: {0}", z.Value());
        int[] p = new int[n];
        Console.Write("p: ");
        for (int i = 0; i < n; i++)
        {
            p[i] = (int)positions[i].Value();
            Console.Write(p[i] + " ");
        }
        Console.WriteLine();
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (p[j] == i)
                {
                    Console.Write(persons[j] + " ");
                }
            }
        }
        Console.WriteLine();
        Console.WriteLine("Successful preferences:");
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (preferences[i, j] == 1 && Math.Abs(p[i] - p[j]) == 1)
                {
                    Console.WriteLine("\t{0} {1}", persons[i], persons[j]);
                }
            }
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
        int show_all_max = 0;
        if (args.Length > 0)
        {
            show_all_max = Convert.ToInt32(args[0]);
        }

        Solve(show_all_max);
    }
}
