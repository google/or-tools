// Copyright 2018 Google LLC
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
using System;
using Google.OrTools.LinearSolver;

public class SimpleProgram
{
  static void Main()
  {
    Solver solver = Solver.CreateSolver("SimpleProgram", "GLOP_LINEAR_PROGRAMMING");
    // Create the variables x and y.
    Variable x = solver.MakeNumVar(0.0, 1.0, "x");
    Variable y = solver.MakeNumVar(0.0, 2.0, "y");
    // Create the objective function, x + y.
    Objective objective = solver.Objective();
    objective.SetCoefficient(x, 1);
    objective.SetCoefficient(y, 1);
    objective.SetMaximization();
    // Call the solver and display the results.
    solver.Solve();
    Console.WriteLine("Solution:");
    Console.WriteLine("x = " + x.SolutionValue());
    Console.WriteLine("y = " + y.SolutionValue());
  }
}
// [END program]
