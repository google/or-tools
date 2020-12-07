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
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.ConstraintSolver;

public class RegexGeneration
{
    /*
     * Global constraint regular
     *
     * This is a translation of MiniZinc's regular constraint (defined in
     * lib/zinc/globals.mzn), via the Comet code refered above.
     * All comments are from the MiniZinc code.
     * """
     * The sequence of values in array 'x' (which must all be in the range 1..S)
     * is accepted by the DFA of 'Q' states with input 1..S and transition
     * function 'd' (which maps (1..Q, 1..S) -> 0..Q)) and initial state 'q0'
     * (which must be in 1..Q) and accepting states 'F' (which all must be in
     * 1..Q).  We reserve state 0 to be an always failing state.
     * """
     *
     * x : IntVar array
     * Q : number of states
     * S : input_max
     * d : transition matrix
     * q0: initial state
     * F : accepting states
     *
     */
    static void MyRegular(Solver solver, IntVar[] x, int Q, int S, int[,] d, int q0, int[] F)
    {
        // d2 is the same as d, except we add one extra transition for
        // each possible input;  each extra transition is from state zero
        // to state zero.  This allows us to continue even if we hit a
        // non-accepted input.
        int[][] d2 = new int [Q + 1][];
        for (int i = 0; i <= Q; i++)
        {
            int[] row = new int[S];
            for (int j = 0; j < S; j++)
            {
                if (i == 0)
                {
                    row[j] = 0;
                }
                else
                {
                    row[j] = d[i - 1, j];
                }
            }
            d2[i] = row;
        }

        int[] d2_flatten =
            (from i in Enumerable.Range(0, Q + 1) from j in Enumerable.Range(0, S) select d2[i][j]).ToArray();

        // If x has index set m..n, then a[m-1] holds the initial state
        // (q0), and a[i+1] holds the state we're in after processing
        // x[i].  If a[n] is in F, then we succeed (ie. accept the
        // string).
        int m = 0;
        int n = x.Length;

        IntVar[] a = solver.MakeIntVarArray(n + 1 - m, 0, Q + 1, "a");
        // Check that the final state is in F
        solver.Add(a[a.Length - 1].Member(F));
        // First state is q0
        solver.Add(a[m] == q0);

        for (int i = 0; i < n; i++)
        {
            solver.Add(x[i] >= 1);
            solver.Add(x[i] <= S);
            // Determine a[i+1]: a[i+1] == d2[a[i], x[i]]
            solver.Add(a[i + 1] == d2_flatten.Element(((a[i] * S) + (x[i] - 1))));
        }
    }

    /**
     *
     * Simple regular expression.
     *
     * My last name (Kjellerstrand) is quite often misspelled
     * in ways that this regular expression shows:
     *   k(je|ä)ll(er|ar)?(st|b)r?an?d
     *
     * This model generates all the words that can be construed
     * by this regular expression.
     *
     *
     * Also see http://www.hakank.org/or-tools/regex.py
     *
     */
    private static void Solve(int n, List<String> res)
    {
        Solver solver = new Solver("RegexGeneration");

        Console.WriteLine("\nn: {0}", n);

        // The DFS (for regular)
        int n_states = 11;
        int input_max = 12;
        int initial_state = 1; // 0 is for the failing state
        int[] accepting_states = { 12 };

        // The DFA
        int[,] transition_fn = {
            // 1 2 3 4 5 6 7 8 9 0 1 2   //
            { 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0 },   //  1 k
            { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0 },   //  2 je
            { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0 },   //  3 ä
            { 0, 0, 0, 0, 5, 6, 7, 8, 0, 0, 0, 0 },   //  4 ll
            { 0, 0, 0, 0, 0, 0, 7, 8, 0, 0, 0, 0 },   //  5 er
            { 0, 0, 0, 0, 0, 0, 7, 8, 0, 0, 0, 0 },   //  6 ar
            { 0, 0, 0, 0, 0, 0, 0, 0, 9, 10, 0, 0 },  //  7 st
            { 0, 0, 0, 0, 0, 0, 0, 0, 9, 10, 0, 0 },  //  8 b
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0 },  //  9 r
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 12 }, // 10 a
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12 },  // 11 n
                                                      // 12 d
        };

        // Name of the states
        String[] s = { "k", "je", "ä", "ll", "er", "ar", "st", "b", "r", "a", "n", "d" };

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 1, input_max, "x");

        //
        // Constraints
        //
        MyRegular(solver, x, n_states, input_max, transition_fn, initial_state, accepting_states);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            List<String> res2 = new List<String>();
            // State 1 (the start state) is not included in the
            // state array (x) so we add it first.
            res2.Add(s[0]);
            for (int i = 0; i < n; i++)
            {
                res2.Add(s[x[i].Value() - 1]);
            }
            res.Add(String.Join("", res2.ToArray()));
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        List<String> res = new List<String>();
        for (int n = 4; n <= 9; n++)
        {
            Solve(n, res);
        }
        Console.WriteLine("\nThe following {0} words where generated", res.Count);
        foreach (string r in res)
        {
            Console.WriteLine(r);
        }
    }
}
