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

public class SecretSanta2
{
    /**
     *
     * Secret Santa problem II in Google CP Solver.
     *
     * From Maple Primes: 'Secret Santa Graph Theory'
     * http://www.mapleprimes.com/blog/jpmay/secretsantagraphtheory
     * """
     * Every year my extended family does a 'secret santa' gift exchange.
     * Each person draws another person at random and then gets a gift for
     * them. At first, none of my siblings were married, and so the draw was
     * completely random. Then, as people got married, we added the restriction
     * that spouses should not draw each others names. This restriction meant
     * that we moved from using slips of paper on a hat to using a simple
     * computer program to choose names. Then people began to complain when
     * they would get the same person two years in a row, so the program was
     * modified to keep some history and avoid giving anyone a name in their
     * recent history. This year, not everyone was participating, and so after
     * removing names, and limiting the number of exclusions to four per person,
     * I had data something like this:
     *
     * Name: Spouse, Recent Picks
     *
     * Noah: Ava. Ella, Evan, Ryan, John
     * Ava: Noah, Evan, Mia, John, Ryan
     * Ryan: Mia, Ella, Ava, Lily, Evan
     * Mia: Ryan, Ava, Ella, Lily, Evan
     * Ella: John, Lily, Evan, Mia, Ava
     * John: Ella, Noah, Lily, Ryan, Ava
     * Lily: Evan, John, Mia, Ava, Ella
     * Evan: Lily, Mia, John, Ryan, Noah
     * """
     *
     *  Note: I interpret this as the following three constraints:
     * 1) One cannot be a Secret Santa of one's spouse
     * 2) One cannot be a Secret Santa for somebody two years in a row
     * 3) Optimization: maximize the time since the last time
     *
     *  This model also handle single persons, something the original
     *  problem don't mention.
     *
     *
     * Also see http://www.hakank.org/or-tools/secret_santa2.py
     *
     */
    private static void Solve(int single = 0)
    {
        Solver solver = new Solver("SecretSanta2");

        Console.WriteLine("\nSingle: {0}", single);

        //
        // The matrix version of earlier rounds.
        // M means that no earlier Santa has been assigned.
        // Note: Ryan and Mia has the same recipient for years 3 and 4,
        //       and Ella and John has for year 4.
        //       This seems to be caused by modification of
        //       original data.
        //
        int n_no_single = 8;
        int M = n_no_single + 1;
        int[][] rounds_no_single = {
            // N  A  R  M  El J  L  Ev
            new int[] { 0, M, 3, M, 1, 4, M, 2 }, // Noah
            new int[] { M, 0, 4, 2, M, 3, M, 1 }, // Ava
            new int[] { M, 2, 0, M, 1, M, 3, 4 }, // Ryan
            new int[] { M, 1, M, 0, 2, M, 3, 4 }, // Mia
            new int[] { M, 4, M, 3, 0, M, 1, 2 }, // Ella
            new int[] { 1, 4, 3, M, M, 0, 2, M }, // John
            new int[] { M, 3, M, 2, 4, 1, 0, M }, // Lily
            new int[] { 4, M, 3, 1, M, 2, M, 0 }  // Evan
        };

        //
        // Rounds with a single person (fake data)
        //
        int n_with_single = 9;
        M = n_with_single + 1;
        int[][] rounds_single = {
            // N  A  R  M  El J  L  Ev S
            new int[] { 0, M, 3, M, 1, 4, M, 2, 2 }, // Noah
            new int[] { M, 0, 4, 2, M, 3, M, 1, 1 }, // Ava
            new int[] { M, 2, 0, M, 1, M, 3, 4, 4 }, // Ryan
            new int[] { M, 1, M, 0, 2, M, 3, 4, 3 }, // Mia
            new int[] { M, 4, M, 3, 0, M, 1, 2, M }, // Ella
            new int[] { 1, 4, 3, M, M, 0, 2, M, M }, // John
            new int[] { M, 3, M, 2, 4, 1, 0, M, M }, // Lily
            new int[] { 4, M, 3, 1, M, 2, M, 0, M }, // Evan
            new int[] { 1, 2, 3, 4, M, 2, M, M, 0 }  // Single
        };

        int Noah = 0;
        int Ava = 1;
        int Ryan = 2;
        int Mia = 3;
        int Ella = 4;
        int John = 5;
        int Lily = 6;
        int Evan = 7;

        int n = n_no_single;

        int[][] rounds = rounds_no_single;
        if (single == 1)
        {
            n = n_with_single;
            rounds = rounds_single;
        }
        M = n + 1;

        IEnumerable<int> RANGE = Enumerable.Range(0, n);

        String[] persons = { "Noah", "Ava", "Ryan", "Mia", "Ella", "John", "Lily", "Evan", "Single" };

        int[] spouses = {
            Ava,  // Noah
            Noah, // Ava
            Mia,  // Rya
            Ryan, // Mia
            John, // Ella
            Ella, // John
            Evan, // Lily
            Lily, // Evan
            -1    // Single has no spouse
        };

        //
        // Decision variables
        //
        IntVar[] santas = solver.MakeIntVarArray(n, 0, n - 1, "santas");
        IntVar[] santa_distance = solver.MakeIntVarArray(n, 0, M, "santa_distance");

        // total of "distance", to maximize
        IntVar z = santa_distance.Sum().VarWithName("z");

        //
        // Constraints
        //
        solver.Add(santas.AllDifferent());

        // Can't be one own"s Secret Santa
        // (i.e. ensure that there are no fix-point in the array.)
        foreach (int i in RANGE)
        {
            solver.Add(santas[i] != i);
        }

        // no Santa for a spouses
        foreach (int i in RANGE)
        {
            if (spouses[i] > -1)
            {
                solver.Add(santas[i] != spouses[i]);
            }
        }

        // optimize "distance" to earlier rounds:
        foreach (int i in RANGE)
        {
            solver.Add(santa_distance[i] == rounds[i].Element(santas[i]));
        }

        // cannot be a Secret Santa for the same person
        // two years in a row.
        foreach (int i in RANGE)
        {
            foreach (int j in RANGE)
            {
                if (rounds[i][j] == 1)
                {
                    solver.Add(santas[i] != j);
                }
            }
        }

        //
        // Objective (minimize the distances)
        //
        OptimizeVar obj = z.Maximize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(santas, Solver.CHOOSE_MIN_SIZE_LOWEST_MIN, Solver.ASSIGN_CENTER_VALUE);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("\ntotal distances: {0}", z.Value());
            Console.Write("santas:  ");
            for (int i = 0; i < n; i++)
            {
                Console.Write(santas[i].Value() + " ");
            }
            Console.WriteLine();
            foreach (int i in RANGE)
            {
                Console.WriteLine("{0}\tis a Santa to {1} (distance {2})", persons[i], persons[santas[i].Value()],
                                  santa_distance[i].Value());
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
        int single = 0;
        Solve(single);
        single = 1;
        Solve(single);
    }
}
