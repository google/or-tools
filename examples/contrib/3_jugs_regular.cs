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

public class ThreeJugsRegular
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

    /**
     *
     * 3 jugs problem using regular constraint in Google CP Solver.
     *
     * A.k.a. water jugs problem.
     *
     * Problem from Taha 'Introduction to Operations Research',
     * page 245f .
     *
     * For more info about the problem, see:
     * http://mathworld.wolfram.com/ThreeJugProblem.html
     *
     * This model use a regular constraint for handling the
     * transitions between the states. Instead of minimizing
     * the cost in a cost matrix (as shortest path problem),
     * we here call the model with increasing length of the
     * sequence array (x).
     *
     *
     * Also see http://www.hakank.org/or-tools/3_jugs_regular.py
     *
     */
    private static bool Solve(int n)
    {
        Solver solver = new Solver("ThreeJugProblem");

        //
        // Data
        //

        // the DFA (for regular)
        int n_states = 14;
        int input_max = 15;
        int initial_state = 1; // state 0 is for the failing state
        int[] accepting_states = { 15 };

        //
        // Manually crafted DFA
        // (from the adjacency matrix used in the other models)
        //
        /*
        int[,] transition_fn =  {
           // 1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
          {0, 2, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0},  // 1
          {0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // 2
          {0, 0, 0, 4, 0, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0},  // 3
          {0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  // 4
          {0, 0, 0, 0, 0, 6, 0, 0, 9, 0, 0, 0, 0, 0, 0},  // 5
          {0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0},  // 6
          {0, 0, 0, 0, 0, 0, 0, 8, 9, 0, 0, 0, 0, 0, 0},  // 7
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15}, // 8
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0}, // 9
          {0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 0}, // 10
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0}, // 11
          {0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13, 0, 0}, // 12
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 0}, // 13
          {0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15}, // 14
                                                            // 15
        };
        */

        //
        // However, the DFA is easy to create from adjacency lists.
        //
        int[][] states = {
            new int[] { 2, 9 }, // state 1
            new int[] { 3 },    // state 2
            new int[] { 4, 9 }, // state 3
            new int[] { 5 },    // state 4
            new int[] { 6, 9 }, // state 5
            new int[] { 7 },    // state 6
            new int[] { 8, 9 }, // state 7
            new int[] { 15 },   // state 8
            new int[] { 10 },   // state 9
            new int[] { 11 },   // state 10
            new int[] { 12 },   // state 11
            new int[] { 13 },   // state 12
            new int[] { 14 },   // state 13
            new int[] { 15 }    // state 14
        };

        int[,] transition_fn = new int[n_states, input_max];
        for (int i = 0; i < n_states; i++)
        {
            for (int j = 1; j <= input_max; j++)
            {
                bool in_states = false;
                for (int s = 0; s < states[i].Length; s++)
                {
                    if (j == states[i][s])
                    {
                        in_states = true;
                        break;
                    }
                }
                if (in_states)
                {
                    transition_fn[i, j - 1] = j;
                }
                else
                {
                    transition_fn[i, j - 1] = 0;
                }
            }
        }

        //
        // The name of the nodes, for printing
        // the solution.
        //
        string[] nodes = {
            "8,0,0", // 1 start
            "5,0,3", // 2
            "5,3,0", // 3
            "2,3,3", // 4
            "2,5,1", // 5
            "7,0,1", // 6
            "7,1,0", // 7
            "4,1,3", // 8
            "3,5,0", // 9
            "3,2,3", // 10
            "6,2,0", // 11
            "6,0,2", // 12
            "1,5,2", // 13
            "1,4,3", // 14
            "4,4,0"  // 15 goal
        };

        //
        // Decision variables
        //

        // Note: We use 1..2 (instead of 0..1) and subtract 1 in the solution
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

        bool found = false;
        while (solver.NextSolution())
        {
            Console.WriteLine("\nFound a solution of length {0}", n + 1);
            int[] x_val = new int[n];
            x_val[0] = 1;
            Console.WriteLine("{0} -> {1}", nodes[0], nodes[x_val[0]]);
            for (int i = 1; i < n; i++)
            {
                // Note: here we subtract 1 to get 0..1
                int val = (int)x[i].Value() - 1;
                x_val[i] = val;
                Console.WriteLine("{0} -> {1}", nodes[x_val[i - 1]], nodes[x_val[i]]);
            }
            Console.WriteLine();

            Console.WriteLine("\nSolutions: {0}", solver.Solutions());
            Console.WriteLine("WallTime: {0}ms", solver.WallTime());
            Console.WriteLine("Failures: {0}", solver.Failures());
            Console.WriteLine("Branches: {0} ", solver.Branches());

            found = true;
        }

        solver.EndSearch();

        return found;
    }

    public static void Main(String[] args)
    {
        for (int n = 1; n < 15; n++)
        {
            bool found = Solve(n);
            if (found)
            {
                break;
            }
        }
    }
}
