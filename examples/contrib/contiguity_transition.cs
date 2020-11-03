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
    static void MyContiguity(Solver solver, IntVar[] x)
    {
        // the DFA (for regular)
        int initial_state = 1;

        // all states are accepting states
        int[] accepting_states = { 1, 2, 3 };

        // The regular expression 0*1*0* {state, input, next state}
        long[][] transition_tuples = { new long[] { 1, 0, 1 }, new long[] { 1, 1, 2 }, new long[] { 2, 0, 3 },
                                       new long[] { 2, 1, 2 }, new long[] { 3, 0, 3 } };

        IntTupleSet result = new IntTupleSet(3);
        result.InsertAll(transition_tuples);

        solver.Add(x.Transition(result, initial_state, accepting_states));
    }

    /**
     *
     * Global constraint contiguity using Transition
     *
     * This version use the built-in TransitionConstraint.
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

        IntVar[] reg_input = solver.MakeIntVarArray(n, 0, 1, "reg_input");

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
                Console.Write((reg_input[i].Value()) + " ");
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
