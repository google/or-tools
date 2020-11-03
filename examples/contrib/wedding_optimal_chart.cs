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

public class WeddingOptimalChart
{
    /**
     *
     * Finding an optimal wedding seating chart.
     *
     * From
     * Meghan L. Bellows and J. D. Luc Peterson
     * "Finding an optimal seating chart for a wedding"
     * http://www.improbable.com/news/2012/Optimal-seating-chart.pdf
     * http://www.improbable.com/2012/02/12/finding-an-optimal-seating-chart-for-a-wedding
     *
     * """
     * Every year, millions of brides (not to mention their mothers, future
     * mothers-in-law, and occasionally grooms) struggle with one of the
     * most daunting tasks during the wedding-planning process: the
     * seating chart. The guest responses are in, banquet hall is booked,
     * menu choices have been made. You think the hard parts are over,
     * but you have yet to embark upon the biggest headache of them all.
     * In order to make this process easier, we present a mathematical
     * formulation that models the seating chart problem. This model can
     * be solved to find the optimal arrangement of guests at tables.
     * At the very least, it can provide a starting point and hopefully
     * minimize stress and arguments…
     * """
     *
     *
     * Also see http://www.hakank.org/minizinc/wedding_optimal_chart.mzn
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("WeddingOptimalChart");

        //
        // Data
        //

        // Easy problem (from the paper)
        int n = 2;  // number of tables
        int a = 10; // maximum number of guests a table can seat
        int b = 1;  // minimum number of people each guest knows at their table

        /*
        // Sligthly harder problem (also from the paper)
        int n = 5; // number of tables
        int a = 4; // maximum number of guests a table can seat
        int b = 1; // minimum number of people each guest knows at their table
        */

        //  j   Guest         Relation
        //  -------------------------------------
        //  1   Deb           mother of the bride
        //  2   John          father of the bride
        //  3   Martha        sister of the bride
        //  4   Travis        boyfriend of Martha
        //  5   Allan         grandfather of the bride
        //  6   Lois          wife of Allan
        //  7   Jayne         aunt of the bride
        //  8   Brad          uncle of the bride
        //  9   Abby          cousin of the bride
        // 10   Mary Helen    mother of the groom
        // 11   Lee           father of the groom
        // 12   Annika        sister of the groom
        // 13   Carl          brother of the groom
        // 14   Colin         brother of the groom
        // 15   Shirley       grandmother of the groom
        // 16   DeAnn         aunt of the groom
        // 17   Lori          aunt of the groom

        // Connection matrix: who knows who, and how strong
        // is the relation
        int[,] C = { { 1, 50, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
                     { 50, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
                     { 1, 1, 1, 50, 1, 1, 1, 1, 10, 0, 0, 0, 0, 0, 0, 0, 0 },
                     { 1, 1, 50, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
                     { 1, 1, 1, 1, 1, 50, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
                     { 1, 1, 1, 1, 50, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
                     { 1, 1, 1, 1, 1, 1, 1, 50, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
                     { 1, 1, 1, 1, 1, 1, 50, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
                     { 1, 1, 10, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
                     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 50, 1, 1, 1, 1, 1, 1 },
                     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 50, 1, 1, 1, 1, 1, 1, 1 },
                     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
                     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
                     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
                     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
                     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
                     { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 } };

        // Names of the guests. B: Bride side, G: Groom side
        String[] names = { "Deb (B)",   "John (B)",  "Martha (B)",  "Travis (B)",     "Allan (B)", "Lois (B)",
                           "Jayne (B)", "Brad (B)",  "Abby (B)",    "Mary Helen (G)", "Lee (G)",   "Annika (G)",
                           "Carl (G)",  "Colin (G)", "Shirley (G)", "DeAnn (G)",      "Lori (G)" };

        int m = C.GetLength(0); // number of quests

        IEnumerable<int> NRANGE = Enumerable.Range(0, n);
        IEnumerable<int> MRANGE = Enumerable.Range(0, m);

        //
        // Decision variables
        //
        IntVar[] tables = solver.MakeIntVarArray(m, 0, n - 1, "tables");
    IntVar z = (from j in MRANGE from k in MRANGE where j <
                    k select C[j, k] * tables[j] ==
                tables[k])
                   .ToArray()
                   .Sum()
                   .VarWithName("z");

    //
    // Constraints
    //
    foreach (int i in NRANGE)
    {
      solver.Add((from j in MRANGE from k in MRANGE where j < k &&
                  C[j, k] > 0 select(tables[j] == i) * (tables[k] == i))
                     .ToArray()
                     .Sum() >= b);

      solver.Add((from j in MRANGE select tables[j] == i).ToArray().Sum() <= a);
    }

    // Symmetry breaking
    solver.Add(tables[0] == 0);

    //
    // Objective
    //
    OptimizeVar obj = z.Maximize(1);

    //
    // Search
    //
    DecisionBuilder db = solver.MakePhase(tables, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

    solver.NewSearch(db, obj);
    while (solver.NextSolution())
    {
        Console.WriteLine("z: {0}", z.Value());
        Console.Write("Table: ");
        foreach (int j in MRANGE)
        {
            Console.Write(tables[j].Value() + " ");
        }
        Console.WriteLine();

        foreach (int i in NRANGE)
        {
            Console.Write("Table {0}: ", i);
            foreach (int j in MRANGE)
            {
                if (tables[j].Value() == i)
                {
                    Console.Write(names[j] + " ");
                }
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
