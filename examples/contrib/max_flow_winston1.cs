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

public class MaxFlowWinston1
{
    /**
     *
     * Max flow problem.
     *
     * From Winston 'Operations Research', page 420f, 423f
     * Sunco Oil example.
     *
     *
     * Also see http://www.hakank.org/or-tools/max_flow_winston1.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("MaxFlowWinston1");

        //
        // Data
        //
        int n = 5;
        IEnumerable<int> NODES = Enumerable.Range(0, n);

        // The arcs
        // Note:
        // This is 1-based to be compatible with other implementations.
        //
        int[,] arcs1 = { { 1, 2 }, { 1, 3 }, { 2, 3 }, { 2, 4 }, { 3, 5 }, { 4, 5 }, { 5, 1 } };

        // Capacities
        int[] cap = { 2, 3, 3, 4, 2, 1, 100 };

        // Convert arcs to 0-based
        int num_arcs = arcs1.GetLength(0);
        IEnumerable<int> ARCS = Enumerable.Range(0, num_arcs);
        int[,] arcs = new int[num_arcs, 2];
        foreach (int i in ARCS)
        {
            for (int j = 0; j < 2; j++)
            {
                arcs[i, j] = arcs1[i, j] - 1;
            }
        }

        // Convert arcs to matrix (for sanity checking below)
        int[,] mat = new int[num_arcs, num_arcs];
        foreach (int i in NODES)
        {
            foreach (int j in NODES)
            {
                int c = 0;
                foreach (int k in ARCS)
                {
                    if (arcs[k, 0] == i && arcs[k, 1] == j)
                    {
                        c = 1;
                    }
                }
                mat[i, j] = c;
            }
        }

        //
        // Decision variables
        //
        IntVar[,] flow = solver.MakeIntVarMatrix(n, n, 0, 200, "flow");
        IntVar z = flow[n - 1, 0].VarWithName("z");

        //
        // Constraints
        //

        // capacity of arcs
        foreach (int i in ARCS)
        {
            solver.Add(flow[arcs[i, 0], arcs[i, 1]] <= cap[i]);
        }

        // inflows == outflows
        foreach (int i in NODES)
        {
      var s1 = (from k in ARCS where arcs[k, 1] ==
                i select flow[arcs[k, 0], arcs[k, 1]])
                   .ToArray()
                   .Sum();

      var s2 = (from k in ARCS where arcs[k, 0] ==
                i select flow[arcs[k, 0], arcs[k, 1]])
                   .ToArray()
                   .Sum();

      solver.Add(s1 == s2);
        }

        // Sanity check: just arcs with connections can have a flow.
        foreach (int i in NODES)
        {
            foreach (int j in NODES)
            {
                if (mat[i, j] == 0)
                {
                    solver.Add(flow[i, j] == 0);
                }
            }
        }

        //
        // Objective
        //
        OptimizeVar obj = z.Maximize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(flow.Flatten(), Solver.INT_VAR_DEFAULT, Solver.ASSIGN_MAX_VALUE);
        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.WriteLine("z: {0}", z.Value());
            foreach (int i in NODES)
            {
                foreach (int j in NODES)
                {
                    Console.Write(flow[i, j].Value() + " ");
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
