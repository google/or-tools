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

public class SetCoveringSkiena
{
    /**
     *
     * Set covering.
     *
     * Example from Steven Skiena, The Stony Brook Algorithm Repository
     * http://www.cs.sunysb.edu/~algorith/files/set-cover.shtml
     * """
     * Input Description: A set of subsets S_1, ..., S_m of the
     * universal set U = {1,...,n}.
     *
     * Problem: What is the smallest subset of subsets T subset S such
     * that \cup_{t_i in T} t_i = U?
     * """
     * Data is from the pictures INPUT/OUTPUT.
     *
     *
     * Also see http://www.hakank.org/or-tools/set_covering_skiena.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("SetCoveringSkiena");

        int num_sets = 7;
        int num_elements = 12;
        IEnumerable<int> Sets = Enumerable.Range(0, num_sets);
        IEnumerable<int> Elements = Enumerable.Range(0, num_elements);

        // Which element belongs to which set
        int[,] belongs = {
            // 1 2 3 4 5 6 7 8 9 0 1 2  elements
            { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // Set 1
            { 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 }, //     2
            { 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0 }, //     3
            { 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0 }, //     4
            { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0 }, //     5
            { 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0 }, //     6
            { 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 }  //     7
        };

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(num_sets, 0, 1, "x");
        IntVar z = x.Sum().VarWithName("z");
        // total number of elements in the choosen sets
        IntVar tot_elements = solver.MakeIntVar(0, num_sets * num_elements, "tot_elements");

        //
        // Constraints
        //

        // all sets must be used
        foreach (int j in Elements)
        {
            solver.Add((from i in Sets select belongs[i, j] * x[i]).ToArray().Sum() >= 1);
        }

        // number of used elements
        solver.Add((from i in Sets from j in Elements select x[i] * belongs[i, j]).ToArray().Sum() == tot_elements);

        //
        // Objective
        //
        OptimizeVar obj = z.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("z: {0}", z.Value());
            Console.WriteLine("tot_elements: {0}", tot_elements.Value());
            Console.WriteLine(
                "x: {0}",
                String.Join(" ", (from i in Enumerable.Range(0, num_sets) select x[i].Value().ToString()).ToArray()));
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
