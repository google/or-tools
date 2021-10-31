// Copyright 2010-2021 Google LLC
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

// [START program]
// OR-Tools solution to the N-queens problem.
// [START import]
using System;
using Google.OrTools.ConstraintSolver;
// [END import]

public class NQueensCp
{
    public static void Main(String[] args)
    {
        // Instantiate the solver.
        // [START solver]
        Solver solver = new Solver("N-Queens");
        // [END solver]

        // [START variables]
        const int BoardSize = 8;
        IntVar[] queens = new IntVar[BoardSize];
        for (int i = 0; i < BoardSize; ++i)
        {
            queens[i] = solver.MakeIntVar(0, BoardSize - 1, $"x{i}");
        }
        // [END variables]

        // Define constraints.
        // [START constraints]
        // All rows must be different.
        solver.Add(queens.AllDifferent());

        // All columns must be different because the indices of queens are all different.
        // No two queens can be on the same diagonal.
        IntVar[] diag1 = new IntVar[BoardSize];
        IntVar[] diag2 = new IntVar[BoardSize];
        for (int i = 0; i < BoardSize; ++i)
        {
            diag1[i] = solver.MakeSum(queens[i], i).Var();
            diag2[i] = solver.MakeSum(queens[i], -i).Var();
        }

        solver.Add(diag1.AllDifferent());
        solver.Add(diag2.AllDifferent());
        // [END constraints]

        // [START db]
        // Create the decision builder to search for solutions.
        DecisionBuilder db = solver.MakePhase(queens, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
        // [END db]

        // [START solve]
        // Iterates through the solutions, displaying each.
        int SolutionCount = 0;
        solver.NewSearch(db);
        while (solver.NextSolution())
        {
            Console.WriteLine("Solution " + SolutionCount);
            for (int i = 0; i < BoardSize; ++i)
            {
                for (int j = 0; j < BoardSize; ++j)
                {
                    if (queens[j].Value() == i)
                    {
                        Console.Write("Q");
                    }
                    else
                    {
                        Console.Write("_");
                    }
                    if (j != BoardSize - 1)
                        Console.Write(" ");
                }
                Console.WriteLine("");
            }
            SolutionCount++;
        }
        solver.EndSearch();
        // [END solve]

        // Statistics.
        // [START statistics]
        Console.WriteLine("Statistics");
        Console.WriteLine($"  failures: {solver.Failures()}");
        Console.WriteLine($"  branches: {solver.Branches()}");
        Console.WriteLine($"  wall time: {solver.WallTime()} ms");
        Console.WriteLine($"  Solutions found: {SolutionCount}");
        // [END statistics]
    }
}
// [END program]
