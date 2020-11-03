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
using System.Diagnostics;
using Google.OrTools.ConstraintSolver;

public class SetPartition
{
    //
    // Partition the sets (binary matrix representation).
    //
    public static void partition_sets(Solver solver, IntVar[,] x, int num_sets, int n)
    {
        for (int i = 0; i < num_sets; i++)
        {
            for (int j = 0; j < num_sets; j++)
            {
                if (i != j)
                {
                    // b = solver.Sum([x[i,k]*x[j,k] for k in range(n)]);
                    // solver.Add(b == 0);
                    solver.Add((from k in Enumerable.Range(0, n) select(x[i, k] * x[j, k])).ToArray().Sum() == 0);
                }
            }
        }

        // ensure that all integers is in
        // (exactly) one partition
        solver.Add(
            (from i in Enumerable.Range(0, num_sets) from j in Enumerable.Range(0, n) select x[i, j]).ToArray().Sum() ==
            n);
    }

    /**
     *
     * Set partition problem.
     *
     * Problem formulation from
     * http://www.koalog.com/resources/samples/PartitionProblem.java.html
     * """
     * This is a partition problem.
     * Given the set S = {1, 2, ..., n},
     * it consists in finding two sets A and B such that:
     *
     *  A U B = S,
     *  |A| = |B|,
     *  sum(A) = sum(B),
     *  sum_squares(A) = sum_squares(B)
     *
     * """
     *
     * This model uses a binary matrix to represent the sets.
     *
     *
     * Also see http://www.hakank.org/or-tools/set_partition.py
     *
     */
    private static void Solve(int n = 16, int num_sets = 2)
    {
        Solver solver = new Solver("SetPartition");

        Console.WriteLine("n: {0}", n);
        Console.WriteLine("num_sets: {0}", num_sets);

        IEnumerable<int> Sets = Enumerable.Range(0, num_sets);
        IEnumerable<int> NRange = Enumerable.Range(0, n);

        //
        // Decision variables
        //
        IntVar[,] a = solver.MakeIntVarMatrix(num_sets, n, 0, 1, "a");
        IntVar[] a_flat = a.Flatten();

        //
        // Constraints
        //

        // partition set
        partition_sets(solver, a, num_sets, n);

        foreach (int i in Sets)
        {
            foreach (int j in Sets)
            {
                // same cardinality
                solver.Add((from k in NRange select a[i, k]).ToArray().Sum() ==
                           (from k in NRange select a[j, k]).ToArray().Sum());

                // same sum
                solver.Add((from k in NRange select(k * a[i, k])).ToArray().Sum() ==
                           (from k in NRange select(k * a[j, k])).ToArray().Sum());

                // same sum squared
                solver.Add((from k in NRange select(k * a[i, k] * k * a[i, k])).ToArray().Sum() ==
                           (from k in NRange select(k * a[j, k] * k * a[j, k])).ToArray().Sum());
            }
        }

        // symmetry breaking for num_sets == 2
        if (num_sets == 2)
        {
            solver.Add(a[0, 0] == 1);
        }

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(a_flat, Solver.INT_VAR_DEFAULT, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            int[,] a_val = new int[num_sets, n];
            foreach (int i in Sets)
            {
                foreach (int j in NRange)
                {
                    a_val[i, j] = (int)a[i, j].Value();
                }
            }
            Console.WriteLine("sums: {0}", (from j in NRange select(j + 1) * a_val[0, j]).ToArray().Sum());

            Console.WriteLine("sums squared: {0}",
                              (from j in NRange select(int) Math.Pow((j + 1) * a_val[0, j], 2)).ToArray().Sum());

            // Show the numbers in each set
            foreach (int i in Sets)
            {
                if ((from j in NRange select a_val[i, j]).ToArray().Sum() > 0)
                {
                    Console.Write(i + 1 + ": ");
                    foreach (int j in NRange)
                    {
                        if (a_val[i, j] == 1)
                        {
                            Console.Write((j + 1) + " ");
                        }
                    }
                    Console.WriteLine();
                }
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
        int n = 16;
        int num_sets = 2;

        if (args.Length > 0)
        {
            n = Convert.ToInt32(args[0]);
        }

        if (args.Length > 1)
        {
            num_sets = Convert.ToInt32(args[1]);
        }

        if (n % num_sets == 0)
        {
            Solve(n, num_sets);
        }
        else
        {
            Console.WriteLine("n {0} num_sets {1}: Equal sets is not possible!", n, num_sets);
        }
    }
}
