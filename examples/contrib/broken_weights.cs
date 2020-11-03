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
using System.Linq;
using Google.OrTools.ConstraintSolver;

public class BrokenWeights
{
    /**
     *
     * Broken weights problem.
     *
     * From http://www.mathlesstraveled.com/?p=701
     * """
     * Here's a fantastic problem I recently heard. Apparently it was first
     * posed by Claude Gaspard Bachet de Meziriac in a book of arithmetic problems
     * published in 1612, and can also be found in Heinrich Dorrie's 100
     * Great Problems of Elementary Mathematics.
     *
     *   A merchant had a forty pound measuring weight that broke
     *   into four pieces as the result of a fall. When the pieces were
     *   subsequently weighed, it was found that the weight of each piece
     *   was a whole number of pounds and that the four pieces could be
     *   used to weigh every integral weight between 1 and 40 pounds. What
     *   were the weights of the pieces?
     *
     * Note that since this was a 17th-century merchant, he of course used a
     * balance scale to weigh things. So, for example, he could use a 1-pound
     * weight and a 4-pound weight to weigh a 3-pound object, by placing the
     * 3-pound object and 1-pound weight on one side of the scale, and
     * the 4-pound weight on the other side.
     * """
     *
     * Also see http://www.hakank.org/or-tools/broken_weights.py
     *
     */
    private static void Solve(int m = 40, int n = 4)
    {
        Solver solver = new Solver("BrokenWeights");

        Console.WriteLine("Total weight (m): {0}", m);
        Console.WriteLine("Number of pieces (n): {0}", n);
        Console.WriteLine();

        //
        // Decision variables
        //

        IntVar[] weights = solver.MakeIntVarArray(n, 1, m, "weights");
        IntVar[,] x = new IntVar[m, n];
        // Note: in x_flat we insert the weights array before x
        IntVar[] x_flat = new IntVar[m * n + n];
        for (int j = 0; j < n; j++)
        {
            x_flat[j] = weights[j];
        }
        for (int i = 0; i < m; i++)
        {
            for (int j = 0; j < n; j++)
            {
                x[i, j] = solver.MakeIntVar(-1, 1, "x[" + i + "," + j + "]");
                x_flat[n + i * n + j] = x[i, j];
            }
        }

        //
        // Constraints
        //

        // symmetry breaking
        for (int j = 1; j < n; j++)
        {
            solver.Add(weights[j - 1] < weights[j]);
        }

        solver.Add(weights.Sum() == m);

        // Check that all weights from 1 to n (default 40) can be made.
        //
        // Since all weights can be on either side
        // of the side of the scale we allow either
        // -1, 0, or 1 of the weights, assuming that
        // -1 is the weights on the left and 1 is on the right.
        //
        for (int i = 0; i < m; i++)
        {
            solver.Add((from j in Enumerable.Range(0, n) select weights[j] * x[i, j]).ToArray().Sum() == i + 1);
        }

        //
        // The objective is to minimize the last weight.
        //
        OptimizeVar obj = weights[n - 1].Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x_flat, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.Write("weights:  ");
            for (int i = 0; i < n; i++)
            {
                Console.Write("{0,3} ", weights[i].Value());
            }
            Console.WriteLine();
            for (int i = 0; i < 10 + n * 4; i++)
            {
                Console.Write("-");
            }
            Console.WriteLine();
            for (int i = 0; i < m; i++)
            {
                Console.Write("weight {0,2}:", i + 1);
                for (int j = 0; j < n; j++)
                {
                    Console.Write("{0,3} ", x[i, j].Value());
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
        int m = 40;
        int n = 4;

        if (args.Length > 0)
        {
            m = Convert.ToInt32(args[0]);
        }

        if (args.Length > 1)
        {
            n = Convert.ToInt32(args[1]);
        }

        Solve(m, n);
    }
}
