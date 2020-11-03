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
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class Langford
{
    /**
     *
     * Langford number problem.
     * See http://www.hakank.org/or-tools/langford.py
     *
     */
    private static void Solve(int k = 8, int num_sol = 0)
    {
        Solver solver = new Solver("Langford");

        Console.WriteLine("k: {0}", k);

        //
        // data
        //
        int p = 2 * k;

        //
        // Decision variables
        //
        IntVar[] position = solver.MakeIntVarArray(p, 0, p - 1, "position");
        IntVar[] solution = solver.MakeIntVarArray(p, 1, k, "solution");

        //
        // Constraints
        //
        solver.Add(position.AllDifferent());

        for (int i = 1; i <= k; i++)
        {
            solver.Add(position[i + k - 1] - (position[i - 1] + solver.MakeIntVar(i + 1, i + 1)) == 0);
            solver.Add(solution.Element(position[i - 1]) == i);
            solver.Add(solution.Element(position[k + i - 1]) == i);
        }

        // Symmetry breaking
        solver.Add(solution[0] < solution[2 * k - 1]);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(position, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        solver.NewSearch(db);

        int num_solutions = 0;
        while (solver.NextSolution())
        {
            Console.Write("solution : ");
            for (int i = 0; i < p; i++)
            {
                Console.Write(solution[i].Value() + " ");
            }
            Console.WriteLine();
            num_solutions++;
            if (num_sol > 0 && num_solutions >= num_sol)
            {
                break;
            }
        }

        Console.WriteLine("\nSolutions: {0}", solver.Solutions());
        Console.WriteLine("WallTime: {0}ms", solver.WallTime());
        Console.WriteLine("Failures: {0}", solver.Failures());
        Console.WriteLine("Branches: {0} ", solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        int k = 8;
        int num_sol = 0; // 0: print all solutions

        if (args.Length > 0)
        {
            k = Convert.ToInt32(args[0]);
        }

        if (args.Length > 1)
        {
            num_sol = Convert.ToInt32(args[1]);
        }

        Solve(k, num_sol);
    }
}
