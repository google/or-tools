// Copyright 2010-2014 Google
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
using Google.OrTools.Flatzinc;

public class CsFz
{
  /**
   * Solves the rabbits + pheasants problem.  We are seing 20 heads
   * and 56 legs. How many rabbits and how many pheasants are we thus
   * seeing?
   */
  private static void Solve(String filename)
  {
    FzModel model = new FzModel(filename);
    model.LoadFromFile(filename);
    Console.WriteLine(model.ToString());
    model.PresolveForCp(/*use_sat=*/true);
    model.PrintStatistics();

    FzSolverParameters parameters = new FzSolverParameters();
    FzParallelSupportInterface support =
        operations_research_flatzinc.MakeSequentialSupport(
            /*all_solutions=*/false, /*num_solutions=*/0);
    FzSolver solver = new FzSolver(model);
    solver.Extract();
    solver.Solve(parameters, support);

    FzOnSolutionOutputVector output_vector = model.output();
    foreach (FzOnSolutionOutput output in output_vector) {
      if (output.variable != null) {
        FzIntegerVariable var = output.variable;
        Console.WriteLine(var.name +  " = " + solver.SolutionValue(var));
      }
    }
  }

  public static void Main(String[] args)
  {
    Solve(args[0]);
  }
}
