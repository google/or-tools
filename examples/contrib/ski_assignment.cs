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
using System.Collections;
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;

public class SkiAssignment
{
    /**
     *
     * Ski assignment in Google CP Solver.
     *
     * From   Jeffrey Lee Hellrung, Jr.:
     * PIC 60, Fall 2008 Final Review, December 12, 2008
     * http://www.math.ucla.edu/~jhellrun/course_files/Fall%25202008/PIC%252060%2520-%2520Data%2520Structures%2520and%2520Algorithms/final_review.pdf
     * """
     * 5. Ski Optimization! Your job at Snapple is pleasant but in the winter
     * you've decided to become a ski bum. You've hooked up with the Mount
     * Baldy Ski Resort. They'll let you ski all winter for free in exchange
     * for helping their ski rental shop with an algorithm to assign skis to
     * skiers. Ideally, each skier should obtain a pair of skis whose height
     * matches his or her own height exactly. Unfortunately, this is generally
     * not possible. We define the disparity between a skier and his or her
     * skis to be the absolute value of the difference between the height of
     * the skier and the pair of skis. Our objective is to find an assignment
     * of skis to skiers that minimizes the sum of the disparities.
     * ...
     * Illustrate your algorithm by explicitly filling out the A[i, j] table
     * for the following sample data:
     *  - Ski heights  : 1, 2, 5, 7, 13, 21.
     *  - Skier heights: 3, 4, 7, 11, 18.
     * """
     *
     * Also see http://www.hakank.org/or-tools/ski_assignment.py
     *

     */
    private static void Solve()
    {
        Solver solver = new Solver("SkiAssignment");

        //
        // Data
        //
        int num_skis = 6;
        int num_skiers = 5;
        int[] ski_heights = { 1, 2, 5, 7, 13, 21 };
        int[] skier_heights = { 3, 4, 7, 11, 18 };

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(num_skiers, 0, num_skis - 1, "x");

        //
        // Constraints
        //
        solver.Add(x.AllDifferent());

        IntVar[] z_tmp = new IntVar[num_skiers];
        for (int i = 0; i < num_skiers; i++)
        {
            z_tmp[i] = (ski_heights.Element(x[i]) - skier_heights[i]).Abs().Var();
        }

        //    IntVar z = solver.MakeIntVar(0, ski_heights.Sum(), "z");
        //    solver.Add(z_tmp.Sum() == z);
        // The direct cast from IntExpr to IntVar is potentially faster than
        // the above code.
        IntVar z = z_tmp.Sum().VarWithName("z");

        //
        // Objective
        //
        OptimizeVar obj = z.Minimize(1);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_FIRST_UNBOUND, Solver.INT_VALUE_DEFAULT);

        solver.NewSearch(db, obj);

        while (solver.NextSolution())
        {
            Console.Write("z: {0} x: ", z.Value());
            for (int i = 0; i < num_skiers; i++)
            {
                Console.Write(x[i].Value() + " ");
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
