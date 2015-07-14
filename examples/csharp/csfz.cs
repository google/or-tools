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
   * Loads a flatzinc file (passed as the first argument) and solves it.
   */
  private static void Solve(String filename)
  {
    FzModel model = new FzModel(filename);
    model.LoadFromFile(filename);
    // Uncomment to see the model.
    // Console.WriteLine(model.ToString());
    // This is mandatory.
    model.PresolveForCp();
    // Display basic statistics on the model.
    model.PrintStatistics();

    FzSolverParameters parameters = new FzSolverParameters();
    // Initialize to default values as in the C++ runner.
    parameters.all_solutions = false;
    parameters.free_search = false;
    parameters.last_conflict = false;
    parameters.heuristic_period = 100;
    parameters.ignore_unknown = false;
    parameters.log_period = 10000000;
    parameters.luby_restart = -1;
    parameters.num_solutions = 0;
    parameters.restart_log_size = -1;
    parameters.threads = 0;
    parameters.time_limit_in_ms = 10000;
    parameters.use_log = true;
    parameters.verbose_impact = false;
    parameters.worker_id = -1;
    parameters.search_type = FzSolverParameters.DEFAULT;
    // Mandatory to retrieve solutions.
    parameters.store_all_solutions = true;

    FzSolver solver = new FzSolver(model);
    solver.SequentialSolve(parameters);

    int last = solver.NumStoredSolutions() - 1;
    if (last >= 0) {
      FzOnSolutionOutputVector output_vector = model.output();
      foreach (FzOnSolutionOutput output in output_vector) {
        if (output.variable != null) {
          FzIntegerVariable var = output.variable;
          Console.WriteLine(var.name +  " = " + solver.StoredValue(last, var));
        }
        foreach (FzIntegerVariable var in output.flat_variables) {
          Console.WriteLine(var.name +  " = " + solver.StoredValue(last, var));
        }
      }
    }
  }

  public static void Main(String[] args)
  {
    Solve(args[0]);
  }
}
