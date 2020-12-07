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
using System.Diagnostics;
using Google.OrTools.ConstraintSolver;

public class NurseRostering
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
        Debug.Assert(Q > 0, "regular: 'Q' must be greater than zero");
        Debug.Assert(S > 0, "regular: 'S' must be greater than zero");

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
            solver.Add(a[i + 1] == d2_flatten.Element(((a[i]) * S) + (x[i] - 1)));
        }
    }

    /**
     *
     * Nurse rostering
     *
     * This is a simple nurse rostering model using a DFA and
     * my decomposition of regular constraint.
     *
     * The DFA is from MiniZinc Tutorial, Nurse Rostering example:
     * - one day off every 4 days
     * - no 3 nights in a row.
     *
     * Also see http://www.hakank.org/or-tools/nurse_rostering.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("NurseRostering");

        //
        // Data
        //

        // Note: If you change num_nurses or num_days,
        //       please also change the constraints
        //       on nurse_stat and/or day_stat.
        int num_nurses = 7;
        int num_days = 14;

        // Note: I had to add a dummy shift.
        int dummy_shift = 0;
        int day_shift = 1;
        int night_shift = 2;
        int off_shift = 3;
        int[] shifts = { dummy_shift, day_shift, night_shift, off_shift };
        int[] valid_shifts = { day_shift, night_shift, off_shift };

        // the DFA (for regular)
        int n_states = 6;
        int input_max = 3;
        int initial_state = 1; // 0 is for the failing state
        int[] accepting_states = { 1, 2, 3, 4, 5, 6 };

        int[,] transition_fn = {
            // d,n,o
            { 2, 3, 1 }, // state 1
            { 4, 4, 1 }, // state 2
            { 4, 5, 1 }, // state 3
            { 6, 6, 1 }, // state 4
            { 6, 0, 1 }, // state 5
            { 0, 0, 1 }  // state 6
        };

        string[] days = { "d", "n", "o" }; // for presentation

        //
        // Decision variables
        //

        // For regular
        IntVar[,] x = solver.MakeIntVarMatrix(num_nurses, num_days, valid_shifts, "x");
        IntVar[] x_flat = x.Flatten();

        // summary of the nurses
        IntVar[] nurse_stat = solver.MakeIntVarArray(num_nurses, 0, num_days, "nurse_stat");

        // summary of the shifts per day
        int num_shifts = shifts.Length;
        IntVar[,] day_stat = new IntVar[num_days, num_shifts];
        for (int i = 0; i < num_days; i++)
        {
            for (int j = 0; j < num_shifts; j++)
            {
                day_stat[i, j] = solver.MakeIntVar(0, num_nurses, "day_stat");
            }
        }

        //
        // Constraints
        //
        for (int i = 0; i < num_nurses; i++)
        {
            IntVar[] reg_input = new IntVar[num_days];
            for (int j = 0; j < num_days; j++)
            {
                reg_input[j] = x[i, j];
            }
            MyRegular(solver, reg_input, n_states, input_max, transition_fn, initial_state, accepting_states);
        }

        //
        // Statistics and constraints for each nurse
        //
        for (int i = 0; i < num_nurses; i++)
        {
            // Number of worked days (either day or night shift)
            IntVar[] b = new IntVar[num_days];
            for (int j = 0; j < num_days; j++)
            {
                b[j] = ((x[i, j] == day_shift) + (x[i, j] == night_shift)).Var();
            }
            solver.Add(b.Sum() == nurse_stat[i]);

            // Each nurse must work between 7 and 10
            // days/nights during this period
            solver.Add(nurse_stat[i] >= 7);
            solver.Add(nurse_stat[i] <= 10);
        }

        //
        // Statistics and constraints for each day
        //
        for (int j = 0; j < num_days; j++)
        {
            for (int t = 0; t < num_shifts; t++)
            {
                IntVar[] b = new IntVar[num_nurses];
                for (int i = 0; i < num_nurses; i++)
                {
                    b[i] = x[i, j] == t;
                }
                solver.Add(b.Sum() == day_stat[j, t]);
            }

            //
            // Some constraints for each day:
            //
            // Note: We have a strict requirements of
            //       the number of shifts.
            //       Using atleast constraints is harder
            //       in this model.
            //
            if (j % 7 == 5 || j % 7 == 6)
            {
                // special constraints for the weekends
                solver.Add(day_stat[j, day_shift] == 2);
                solver.Add(day_stat[j, night_shift] == 1);
                solver.Add(day_stat[j, off_shift] == 4);
            }
            else
            {
                // for workdays:

                // - exactly 3 on day shift
                solver.Add(day_stat[j, day_shift] == 3);
                // - exactly 2 on night
                solver.Add(day_stat[j, night_shift] == 2);
                // - exactly 2 off duty
                solver.Add(day_stat[j, off_shift] == 2);
            }
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        int num_solutions = 0;
        while (solver.NextSolution())
        {
            num_solutions++;
            for (int i = 0; i < num_nurses; i++)
            {
                Console.Write("Nurse #{0,-2}: ", i);
                var occ = new Dictionary<int, int>();
                for (int j = 0; j < num_days; j++)
                {
                    int v = (int)x[i, j].Value() - 1;
                    if (!occ.ContainsKey(v))
                    {
                        occ[v] = 0;
                    }
                    occ[v]++;
                    Console.Write(days[v] + " ");
                }

                Console.Write(" #workdays: {0,2}", nurse_stat[i].Value());
                foreach (int s in valid_shifts)
                {
                    int v = 0;
                    if (occ.ContainsKey(s - 1))
                    {
                        v = occ[s - 1];
                    }
                    Console.Write("  {0}:{1}", days[s - 1], v);
                }
                Console.WriteLine();
            }
            Console.WriteLine();

            Console.WriteLine("Statistics per day:\nDay      d n o");
            for (int j = 0; j < num_days; j++)
            {
                Console.Write("Day #{0,2}: ", j);
                foreach (int t in valid_shifts)
                {
                    Console.Write(day_stat[j, t].Value() + " ");
                }
                Console.WriteLine();
            }
            Console.WriteLine();

            // We just show 2 solutions
            if (num_solutions > 1)
            {
                break;
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
        Solve();
    }
}
