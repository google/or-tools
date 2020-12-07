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
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class NonTransitiveDice
{
    /**
     *
     * Nontransitive dice.
     *
     * From
     * http://en.wikipedia.org/wiki/Nontransitive_dice
     * """
     * A set of nontransitive dice is a set of dice for which the relation
     * 'is more likely to roll a higher number' is not transitive. See also
     * intransitivity.
     *
     * This situation is similar to that in the game Rock, Paper, Scissors,
     * in which each element has an advantage over one choice and a
     * disadvantage to the other.
     * """
     *
     * Also see http://www.hakank.org/or-tools/nontransitive_dice.py
     *
     *
     */
    private static void Solve(int m = 3, int n = 6, int minimize_val = 0)
    {
        Solver solver = new Solver("Nontransitive_dice");

        Console.WriteLine("Number of dice: {0}", m);
        Console.WriteLine("Number of sides: {0}", n);
        Console.WriteLine("minimize_val: {0}\n", minimize_val);

        //
        // Decision variables
        //

        // The dice
        IntVar[,] dice = solver.MakeIntVarMatrix(m, n, 1, n * 2, "dice");
        IntVar[] dice_flat = dice.Flatten();

        // For comparison (probability)
        IntVar[,] comp = solver.MakeIntVarMatrix(m, 2, 0, n * n, "dice");
        IntVar[] comp_flat = comp.Flatten();

        // For branching
        IntVar[] all = dice_flat.Concat(comp_flat).ToArray();

        // The following variables are for summaries or objectives
        IntVar[] gap = solver.MakeIntVarArray(m, 0, n * n, "gap");
        IntVar gap_sum = gap.Sum().Var();

        IntVar max_val = dice_flat.Max().Var();
        IntVar max_win = comp_flat.Max().Var();

        // number of occurrences of each value of the dice
        IntVar[] counts = solver.MakeIntVarArray(n * 2 + 1, 0, n * m, "counts");

        //
        // Constraints
        //

        // Number of occurrences for each number
        solver.Add(dice_flat.Distribute(counts));

        // Order of the number of each die, lowest first
        for (int i = 0; i < m; i++)
        {
            for (int j = 0; j < n - 1; j++)
            {
                solver.Add(dice[i, j] <= dice[i, j + 1]);
            }
        }

        // Nontransitivity
        for (int i = 0; i < m; i++)
        {
            solver.Add(comp[i, 0] > comp[i, 1]);
        }

        // Probability gap
        for (int i = 0; i < m; i++)
        {
            solver.Add(gap[i] == comp[i, 0] - comp[i, 1]);
            solver.Add(gap[i] > 0);
        }

        // And now we roll...
        // comp[] is the number of wins for [A vs B, B vs A]
        for (int d = 0; d < m; d++)
        {
            IntVar sum1 = (from r1 in Enumerable.Range(0, n) from r2 in Enumerable.Range(0, n)
                               select(dice[d % m, r1] > dice[(d + 1) % m, r2]))
                              .ToArray()
                              .Sum()
                              .Var();

            solver.Add(comp[d % m, 0] == sum1);

            IntVar sum2 = (from r1 in Enumerable.Range(0, n) from r2 in Enumerable.Range(0, n)
                               select(dice[(d + 1) % m, r1] > dice[d % m, r2]))
                              .ToArray()
                              .Sum()
                              .Var();

            solver.Add(comp[d % m, 1] == sum2);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(all, Solver.INT_VAR_DEFAULT, Solver.ASSIGN_MIN_VALUE);

        if (minimize_val > 0)
        {
            Console.WriteLine("Minimizing max_val");

            OptimizeVar obj = max_val.Minimize(1);

            // Other experiments:
            // OptimizeVar obj = max_win.Maximize(1);
            // OptimizeVar obj = gap_sum.Maximize(1);

            solver.NewSearch(db, obj);
        }
        else
        {
            solver.NewSearch(db);
        }

        while (solver.NextSolution())
        {
            Console.WriteLine("gap_sum: {0}", gap_sum.Value());
            Console.WriteLine("gap: {0}",
                              (from i in Enumerable.Range(0, m) select gap[i].Value().ToString()).ToArray());
            Console.WriteLine("max_val: {0}", max_val.Value());
            Console.WriteLine("max_win: {0}", max_win.Value());
            Console.WriteLine("dice:");
            for (int i = 0; i < m; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    Console.Write(dice[i, j].Value() + " ");
                }
                Console.WriteLine();
            }
            Console.WriteLine("comp:");
            for (int i = 0; i < m; i++)
            {
                for (int j = 0; j < 2; j++)
                {
                    Console.Write(comp[i, j].Value() + " ");
                }
                Console.WriteLine();
            }
            Console.WriteLine("counts:");
            for (int i = 1; i < n * 2 + 1; i++)
            {
                int c = (int)counts[i].Value();
                if (c > 0)
                {
                    Console.Write("{0}({1}) ", i, c);
                }
            }
            Console.WriteLine("\n");
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        int m = 3;            // number of dice
        int n = 6;            // number of sides of each die
        int minimize_val = 0; // minimizing max_max (0: no, 1: yes)

        if (args.Length > 0)
        {
            m = Convert.ToInt32(args[0]);
        }

        if (args.Length > 1)
        {
            n = Convert.ToInt32(args[1]);
        }

        if (args.Length > 2)
        {
            minimize_val = Convert.ToInt32(args[2]);
        }

        Solve(m, n, minimize_val);
    }
}
