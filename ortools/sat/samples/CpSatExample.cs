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
using System.Linq;
using Google.OrTools.Sat;
// [END import]

public class CpSatExample
{
    static void Main()
    {
        // Creates the model.
        // [START model]
        CpModel model = new CpModel();
        // [END model]

        // Creates the variables.
        // [START variables]
        int varUpperBound = new int[] { 50, 45, 37 }.Max();

        IntVar x = model.NewIntVar(0, varUpperBound, "x");
        IntVar y = model.NewIntVar(0, varUpperBound, "y");
        IntVar z = model.NewIntVar(0, varUpperBound, "z");
        // [END variables]

        // Creates the constraints.
        // [START constraints]
        model.Add(2 * x + 7 * y + 3 * z <= 50);
        model.Add(3 * x - 5 * y + 7 * z <= 45);
        model.Add(5 * x + 2 * y - 6 * z <= 37);
        // [END constraints]

        // [START objective]
        model.Maximize(2 * x + 2 * y + 3 * z);
        // [END objective]

        // Creates a solver and solves the model.
        // [START solve]
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        // [END solve]

        // [START print_solution]
        if (status == CpSolverStatus.Optimal || status == CpSolverStatus.Feasible)
        {
            Console.WriteLine($"Maximum of objective function: {solver.ObjectiveValue}");
            Console.WriteLine("x = " + solver.Value(x));
            Console.WriteLine("y = " + solver.Value(y));
            Console.WriteLine("z = " + solver.Value(z));
        }
        else
        {
            Console.WriteLine("No solution found.");
        }
        // [END print_solution]

        // [START statistics]
        Console.WriteLine("Statistics");
        Console.WriteLine($"  conflicts: {solver.NumConflicts()}");
        Console.WriteLine($"  branches : {solver.NumBranches()}");
        Console.WriteLine($"  wall time: {solver.WallTime()}s");
        // [END statistics]
    }
}
// [END program]
