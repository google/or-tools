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
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class Strimko2
{
    /**
     *
     * Solves a Strimko problem.
     * See http://www.hakank.org/google_or_tools/strimko2.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("Strimko2");

        //
        // data
        //
        int[,] streams = { { 1, 1, 2, 2, 2, 2, 2 }, { 1, 1, 2, 3, 3, 3, 2 }, { 1, 4, 1, 3, 3, 5, 5 },
                           { 4, 4, 3, 1, 3, 5, 5 }, { 4, 6, 6, 6, 7, 7, 5 }, { 6, 4, 6, 4, 5, 5, 7 },
                           { 6, 6, 4, 7, 7, 7, 7 } };

        // Note: This is 1-based
        int[,] placed = { { 2, 1, 1 }, { 2, 3, 7 }, { 2, 5, 6 }, { 2, 7, 4 }, { 3, 2, 7 },
                          { 3, 6, 1 }, { 4, 1, 4 }, { 4, 7, 5 }, { 5, 2, 2 }, { 5, 6, 6 } };

        int n = streams.GetLength(0);
        int num_placed = placed.GetLength(0);

        //
        // Decision variables
        //
        IntVar[,] x = solver.MakeIntVarMatrix(n, n, 1, n, "x");
        IntVar[] x_flat = x.Flatten();

        //
        // Constraints
        //
        // all rows and columns must be unique, i.e. a Latin Square
        for (int i = 0; i < n; i++)
        {
            IntVar[] row = new IntVar[n];
            IntVar[] col = new IntVar[n];
            for (int j = 0; j < n; j++)
            {
                row[j] = x[i, j];
                col[j] = x[j, i];
            }

            solver.Add(row.AllDifferent());
            solver.Add(col.AllDifferent());
        }

        // streams
        for (int s = 1; s <= n; s++)
        {
      IntVar[] tmp =
          (from i in Enumerable.Range(0, n) from j in Enumerable.Range(0, n)
               where streams[i, j] == s select x[i, j])
              .ToArray();
      solver.Add(tmp.AllDifferent());
        }

        // placed
        for (int i = 0; i < num_placed; i++)
        {
            // note: also adjust to 0-based
            solver.Add(x[placed[i, 0] - 1, placed[i, 1] - 1] == placed[i, 2]);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            for (int i = 0; i < n; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    Console.Write(x[i, j].Value() + " ");
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
