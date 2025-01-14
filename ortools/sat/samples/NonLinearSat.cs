// Copyright 2010-2025 Google LLC
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

// Finds a rectangle with maximum available area for given perimeter
// using AddMultiplicationEquality

using System;
using Google.OrTools.Sat;

public class NonLinearSat
{
    public static void Main()
    {
        const int perimeter = 20;

        var model = new CpModel();

        var x = model.NewIntVar(0, perimeter, "x");
        var y = model.NewIntVar(0, perimeter, "y");

        model.Add(2 * (x + y) == perimeter);

        var area = model.NewIntVar(0, perimeter * perimeter, "s");

        model.AddMultiplicationEquality(area, x, y);

        model.Maximize(area);

        var solver = new CpSolver();

        var status = solver.Solve(model);

        if (status == CpSolverStatus.Optimal || status == CpSolverStatus.Feasible)
        {
            Console.WriteLine($"x = {solver.Value(x)}");
            Console.WriteLine($"y = {solver.Value(y)}");
            Console.WriteLine($"s = {solver.Value(area)}");
        }
        else
            Console.WriteLine("No solution found");
    }
}