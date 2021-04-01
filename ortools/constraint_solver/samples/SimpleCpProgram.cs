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
// [START import]
using System;
using Google.OrTools.ConstraintSolver;
// [END import]

/// <summary>
///   This is a simple CP program.
/// </summary>
public class SimpleCpProgram
{
    public static void Main(String[] args)
    {
        // Instantiate the solver.
        // [START solver]
        Solver solver = new Solver("CpSimple");
        // [END solver]

        // Create the variables.
        // [START variables]
        const long numVals = 3;
        IntVar x = solver.MakeIntVar(0, numVals - 1, "x");
        IntVar y = solver.MakeIntVar(0, numVals - 1, "y");
        IntVar z = solver.MakeIntVar(0, numVals - 1, "z");
        // [END variables]

        // Constraint 0: x != y..
        // [START constraints]
        solver.Add(solver.MakeAllDifferent(new IntVar[] { x, y }));
        Console.WriteLine($"Number of constraints: {solver.Constraints()}");
        // [END constraints]

        // Solve the problem.
        // [START solve]
        DecisionBuilder db =
            solver.MakePhase(new IntVar[] { x, y, z }, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);
        // [END solve]

        // Print solution on console.
        // [START print_solution]
        int count = 0;
        solver.NewSearch(db);
        while (solver.NextSolution())
        {
            ++count;
            Console.WriteLine($"Solution: {count}\n x={x.Value()} y={y.Value()} z={z.Value()}");
        }
        solver.EndSearch();
        Console.WriteLine($"Number of solutions found: {solver.Solutions()}");
        // [END print_solution]

        // [START advanced]
        Console.WriteLine("Advanced usage:");
        Console.WriteLine($"Problem solved in {solver.WallTime()}ms");
        Console.WriteLine($"Memory usage: {Solver.MemoryUsage()}bytes");
        // [END advanced]
    }
}
// [END program]
