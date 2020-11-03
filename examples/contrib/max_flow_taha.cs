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

public class MaxFlowTaha
{
    /**
     *
     * Max flow problem.
     *
     * From Taha "Introduction to Operations Research", Example 6.4-2
     *
     * Translated from the AMPL code at
     * http://taha.ineg.uark.edu/maxflo.txt
     *
     * Also see http://www.hakank.org/or-tools/max_flow_taha.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("MaxFlowTaha");

        //
        // Data
        //
        int n = 5;
        int start = 0;
        int end = n - 1;

        IEnumerable<int> NODES = Enumerable.Range(0, n);

        // cost matrix
        int[,] c = {
            { 0, 20, 30, 10, 0 }, { 0, 0, 40, 0, 30 }, { 0, 0, 0, 10, 20 }, { 0, 0, 5, 0, 20 }, { 0, 0, 0, 0, 0 }
        };

        //
        // Decision variables
        //
        IntVar[,] x = new IntVar[n, n];
        foreach (int i in NODES)
        {
            foreach (int j in NODES)
            {
                x[i, j] = solver.MakeIntVar(0, c[i, j], "x");
            }
        }

        IntVar[] x_flat = x.Flatten();

        IntVar[] out_flow = solver.MakeIntVarArray(n, 0, 1000, "out_flow");
        IntVar[] in_flow = solver.MakeIntVarArray(n, 0, 1000, "in_flow");
        IntVar total = solver.MakeIntVar(0, 10000, "total");

        //
        // Constraints
        //
    solver.Add((from j in NODES where c[start, j] > 0 select x[start, j])
                   .ToArray()
                   .Sum() == total);

    foreach (int i in NODES)
    {
      var in_flow_sum = (from j in NODES where c[j, i] > 0 select x[j, i]);
      if (in_flow_sum.Count() > 0)
      {
          solver.Add(in_flow_sum.ToArray().Sum() == in_flow[i]);
      }

      var out_flow_sum = (from j in NODES where c[i, j] > 0 select x[i, j]);
      if (out_flow_sum.Count() > 0)
      {
          solver.Add(out_flow_sum.ToArray().Sum() == out_flow[i]);
      }
    }

    // in_flow == out_flow
    foreach (int i in NODES)
    {
        if (i != start && i != end)
        {
            solver.Add(out_flow[i] == in_flow[i]);
        }
    }

    var s1 = (from i in NODES where c[i, start] > 0 select x[i, start]);
    if (s1.Count() > 0)
    {
        solver.Add(s1.ToArray().Sum() == 0);
    }

    var s2 = (from j in NODES where c[end, j] > 0 select x[end, j]);
    if (s2.Count() > 0)
    {
        solver.Add(s2.ToArray().Sum() == 0);
    }

    //
    // Objective
    //
    OptimizeVar obj = total.Maximize(1);

    //
    // Search
    //
    DecisionBuilder db = solver.MakePhase(x_flat.Concat(in_flow).Concat(out_flow).ToArray(), Solver.INT_VAR_DEFAULT,
                                          Solver.ASSIGN_MAX_VALUE);

    solver.NewSearch(db, obj);
    while (solver.NextSolution())
    {
        Console.WriteLine("total: {0}", total.Value());
        Console.Write("in_flow : ");
        foreach (int i in NODES)
        {
            Console.Write(in_flow[i].Value() + " ");
        }
        Console.Write("\nout_flow: ");
        foreach (int i in NODES)
        {
            Console.Write(out_flow[i].Value() + " ");
        }
        Console.WriteLine();
        foreach (int i in NODES)
        {
            foreach (int j in NODES)
            {
                Console.Write("{0,2} ", x[i, j].Value());
            }
            Console.WriteLine();
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
