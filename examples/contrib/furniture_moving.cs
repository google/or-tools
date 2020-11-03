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
using Google.OrTools.ConstraintSolver;

public class FurnitureMoving
{
    /*
     * Decompositon of cumulative.
     *
     * Inspired by the MiniZinc implementation:
     * http://www.g12.csse.unimelb.edu.au/wiki/doku.php?id=g12:zinc:lib:minizinc:std:cumulative.mzn&s[]=cumulative
     * The MiniZinc decomposition is discussed in the paper:
     * A. Schutt, T. Feydy, P.J. Stuckey, and M. G. Wallace.
     * "Why cumulative decomposition is not as bad as it sounds."
     * Download:
     * http://www.cs.mu.oz.au/%7Epjs/rcpsp/papers/cp09-cu.pdf
     * http://www.cs.mu.oz.au/%7Epjs/rcpsp/cumu_lazyfd.pdf
     *
     *
     * Parameters:
     *
     * s: start_times    assumption: IntVar[]
     * d: durations      assumption: int[]
     * r: resources      assumption: int[]
     * b: resource limit assumption: IntVar or int
     *
     *
     */
    static void MyCumulative(Solver solver, IntVar[] s, int[] d, int[] r, IntVar b)
    {
    int[] tasks = (from i in Enumerable.Range(0, s.Length) where r[i] > 0 &&
                   d[i] > 0 select i)
                      .ToArray();
    int times_min = tasks.Min(i => (int)s[i].Min());
    int d_max = d.Max();
    int times_max = tasks.Max(i => (int)s[i].Max() + d_max);
    for (int t = times_min; t <= times_max; t++)
    {
        ArrayList bb = new ArrayList();
        foreach (int i in tasks)
        {
            bb.Add(((s[i] <= t) * (s[i] + d[i] > t) * r[i]).Var());
        }
        solver.Add((bb.ToArray(typeof(IntVar)) as IntVar[]).Sum() <= b);
    }

    // Somewhat experimental:
    // This constraint is needed to constrain the upper limit of b.
    if (b is IntVar)
    {
        solver.Add(b <= r.Sum());
    }
    }

    /**
     *
     * Moving furnitures (scheduling) problem in Google CP Solver.
     *
     * Marriott & Stukey: 'Programming with constraints', page  112f
     *
     * The model implements an decomposition of the global constraint
     * cumulative (see above).
     *
     * Also see http://www.hakank.org/or-tools/furniture_moving.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("FurnitureMoving");

        int n = 4;
        int[] duration = { 30, 10, 15, 15 };
        int[] demand = { 3, 1, 3, 2 };
        int upper_limit = 160;

        //
        // Decision variables
        //
        IntVar[] start_times = solver.MakeIntVarArray(n, 0, upper_limit, "start_times");
        IntVar[] end_times = solver.MakeIntVarArray(n, 0, upper_limit * 2, "end_times");
        IntVar end_time = solver.MakeIntVar(0, upper_limit * 2, "end_time");

        // number of needed resources, to be minimized or constrained
        IntVar num_resources = solver.MakeIntVar(0, 10, "num_resources");

        //
        // Constraints
        //
        for (int i = 0; i < n; i++)
        {
            solver.Add(end_times[i] == start_times[i] + duration[i]);
        }

        solver.Add(end_time == end_times.Max());
        MyCumulative(solver, start_times, duration, demand, num_resources);

        //
        // Some extra constraints to play with
        //

        // all tasks must end within an hour
        // solver.Add(end_time <= 60);

        // All tasks should start at time 0
        // for(int i = 0; i < n; i++) {
        //   solver.Add(start_times[i] == 0);
        // }

        // limitation of the number of people
        // solver.Add(num_resources <= 3);
        solver.Add(num_resources <= 4);

        //
        // Objective
        //

        // OptimizeVar obj = num_resources.Minimize(1);
        OptimizeVar obj = end_time.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(start_times, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("num_resources: {0} end_time: {1}", num_resources.Value(), end_time.Value());
            for (int i = 0; i < n; i++)
            {
                Console.WriteLine("Task {0,1}: {1,2} -> {2,2} -> {3,2} (demand: {4})", i, start_times[i].Value(),
                                  duration[i], end_times[i].Value(), demand[i]);
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
