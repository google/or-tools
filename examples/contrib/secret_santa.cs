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

public class SecretSanta
{
    /**
     *
     * Secret Santa problem in Google CP Solver.
     *
     * From Ruby Quiz Secret Santa
     * http://www.rubyquiz.com/quiz2.html
     * """
     * Honoring a long standing tradition started by my wife's dad, my friends
     * all play a Secret Santa game around Christmas time. We draw names and
     * spend a week sneaking that person gifts and clues to our identity. On the
     * last night of the game, we get together, have dinner, share stories, and,
     * most importantly, try to guess who our Secret Santa was. It's a crazily
     * fun way to enjoy each other's company during the holidays.
     *
     * To choose Santas, we use to draw names out of a hat. This system was
     * tedious, prone to many 'Wait, I got myself...' problems. This year, we
     * made a change to the rules that further complicated picking and we knew
     * the hat draw would not stand up to the challenge. Naturally, to solve
     * this problem, I scripted the process. Since that turned out to be more
     * interesting than I had expected, I decided to share.
     *
     * This weeks Ruby Quiz is to implement a Secret Santa selection script.
     * *  Your script will be fed a list of names on STDIN.
     * ...
     * Your script should then choose a Secret Santa for every name in the list.
     * Obviously, a person cannot be their own Secret Santa. In addition, my
     * friends no longer allow people in the same family to be Santas for each
     * other and your script should take this into account.
     * """
     *
     *  Comment: This model skips the file input and mail parts. We
     *        assume that the friends are identified with a number from 1..n,
     *        and the families is identified with a number 1..num_families.
     *
     * Also see http://www.hakank.org/or-tools/secret_santa.py
     * Also see http://www.hakank.org/or-tools/secret_santa2.cs
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("SecretSanta");

        int[] family = { 1, 1, 1, 1, 2, 3, 3, 3, 3, 3, 4, 4 };
        int n = family.Length;

        Console.WriteLine("n = {0}", n);

        IEnumerable<int> RANGE = Enumerable.Range(0, n);

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, n - 1, "x");

        //
        // Constraints
        //
        solver.Add(x.AllDifferent());

        // Can't be one own"s Secret Santa
        // (i.e. ensure that there are no fix-point in the array.)
        foreach (int i in RANGE)
        {
            solver.Add(x[i] != i);
        }

        // No Secret Santa to a person in the same family
        foreach (int i in RANGE)
        {
            solver.Add(solver.MakeIntConst(family[i]) != family.Element(x[i]));
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.INT_VAR_SIMPLE, Solver.INT_VALUE_SIMPLE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.Write("x:  ");
            foreach (int i in RANGE)
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
