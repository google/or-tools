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
using System.Linq;
using System.Diagnostics;
using Google.OrTools.ConstraintSolver;

public class ContiguityRegular
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
            solver.Add(a[i + 1] == d2_flatten.Element(((a[i] * S) + (x[i] - 1))));
        }
    }

    static void MyContiguity(Solver solver, IntVar[] x)
    {
        // the DFA (for regular)
        int n_states = 3;
        int input_max = 2;
        int initial_state = 1; // note: state 0 is used for the failing state
                               // in MyRegular

        // all states are accepting states
        int[] accepting_states = { 1, 2, 3 };

        // The regular expression 0*1*0*
        int[,] transition_fn = {
            { 1, 2 }, // state 1 (start): input 0 -> state 1, input 1 -> state 2 i.e. 0*
            { 3, 2 }, // state 2: 1*
            { 3, 0 }, // state 3: 0*
        };

        MyRegular(solver, x, n_states, input_max, transition_fn, initial_state, accepting_states);
    }

    /**
     *
     * Global constraint contiguity using regular
     *
     * This is a decomposition of the global constraint global contiguity.
     *
     * From Global Constraint Catalogue
     * http://www.emn.fr/x-info/sdemasse/gccat/Cglobal_contiguity.html
     * """
     * Enforce all variables of the VARIABLES collection to be assigned to 0 or 1.
     * In addition, all variables assigned to value 1 appear contiguously.
     *
     * Example:
     * (<0, 1, 1, 0>)
     *
     * The global_contiguity constraint holds since the sequence 0 1 1 0 contains
     * no more than one group of contiguous 1.
     * """
     *
     * Also see http://www.hakank.org/or-tools/contiguity_regular.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("ContiguityRegular");

        //
        // Data
        //
        int n = 7; // length of the array

        //
        // Decision variables
        //

        // Note: We use 1..2 (instead of 0..1) and subtract 1 in the solution
        IntVar[] reg_input = solver.MakeIntVarArray(n, 1, 2, "reg_input");

        //
        // Constraints
        //
        MyContiguity(solver, reg_input);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(reg_input, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            for (int i = 0; i < n; i++)
            {
                // Note: here we subtract 1 to get 0..1
                Console.Write((reg_input[i].Value() - 1) + " ");
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
