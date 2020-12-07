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

public class SchedulingSpeakers
{
    /**
     *
     * Scheduling speakers problem
     *
     *  From Rina Dechter, Constraint Processing, page 72
     *  Scheduling of 6 speakers in 6 slots.
     *
     * See http://www.hakank.org/google_or_tools/scheduling_speakers.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("SchedulingSpeakers");

        // number of speakers
        int n = 6;

        // slots available to speak
        int[][] available = {
            // Reasoning:
            new int[] { 3, 4, 5, 6 },      // 2) the only one with 6 after speaker F -> 1
            new int[] { 3, 4 },            // 5) 3 or 4
            new int[] { 2, 3, 4, 5 },      // 3) only with 5 after F -> 1 and A -> 6
            new int[] { 2, 3, 4 },         // 4) only with 2 after C -> 5 and F -> 1
            new int[] { 3, 4 },            // 5) 3 or 4
            new int[] { 1, 2, 3, 4, 5, 6 } // 1) the only with 1
        };

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 1, n, "x");

        //
        // Constraints
        //
        solver.Add(x.AllDifferent());

        for (int i = 0; i < n; i++)
        {
            solver.Add(x[i].Member(available[i]));
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            Console.WriteLine(string.Join(",", (from i in x select i.Value())));
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
