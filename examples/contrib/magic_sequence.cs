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
using System.IO;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class MagicSequence
{
    /**
     *
     * Magic sequence problem.
     *
     * This is a port of the Python model
     * https://code.google.com/p/or-tools/source/browse/trunk/python/magic_sequence_distribute.py
     * """
     * This models aims at building a sequence of numbers such that the number of
     * occurrences of i in this sequence is equal to the value of the ith number.
     * It uses an aggregated formulation of the count expression called
     * distribute().
     * """
     *
     */
    private static void Solve(int size)
    {
        Solver solver = new Solver("MagicSequence");

        Console.WriteLine("\nSize: {0}", size);

        //
        // data
        //
        int[] all_values = new int[size];
        for (int i = 0; i < size; i++)
        {
            all_values[i] = i;
        }

        //
        // Decision variables
        //
        IntVar[] all_vars = solver.MakeIntVarArray(size, 0, size - 1, "vars");

        //
        // Constraints
        //
        solver.Add(all_vars.Distribute(all_values, all_vars));
        solver.Add(all_vars.Sum() == size);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(all_vars, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            for (int i = 0; i < size; i++)
            {
                Console.Write(all_vars[i].Value() + " ");
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
        if (args.Length > 0)
        {
            int size = Convert.ToInt32(args[0]);
            Solve(size);
        }
        else
        {
            // Let's test some diferent sizes
            foreach (int i in new int[] { 2, 10, 100, 200, 500 })
            {
                Solve(i);
            }
        }
    }
}
