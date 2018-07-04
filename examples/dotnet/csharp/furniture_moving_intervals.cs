// Copyright 2010-2017 Google
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
using System.Linq;
using Google.OrTools.ConstraintSolver;

public class FurnitureMovingIntervals
{
  /**
   *
   * Moving furnitures (scheduling) problem in Google CP Solver.
   *
   * Marriott & Stukey: 'Programming with constraints', page  112f
   *
   * Also see http://www.hakank.org/or-tools/furniture_moving.py
   *
   */
  private static void Solve()
  {
    Solver solver = new Solver("FurnitureMovingIntervals");

    const int n = 4;
    int[] durations = {30,10,15,15};
    int[] demand = {3, 1, 3, 2};
    const int upper_limit = 160;
    const int max_num_workers = 5;

    //
    // Decision variables
    //
    IntervalVar[] tasks = new IntervalVar[n];
    for (int i = 0; i < n; ++i)
    {
      tasks[i] = solver.MakeFixedDurationIntervalVar(0,
                                                     upper_limit - durations[i],
                                                     durations[i],
                                                     false,
                                                     "task_" + i);
    }

    // Fillers that span the whole resource and limit the available
    // number of workers.
    IntervalVar[] fillers = new IntervalVar[max_num_workers];
    for (int i = 0; i < max_num_workers; ++i)
    {
      fillers[i] = solver.MakeFixedDurationIntervalVar(0,
                                                       0,
                                                       upper_limit,
                                                       true,
                                                       "filler_" + i);
    }

    // Number of needed resources, to be minimized or constrained.
    IntVar num_workers  = solver.MakeIntVar(0, max_num_workers, "num_workers");
    // Links fillers and num_workers.
    for (int i = 0; i < max_num_workers; ++i)
    {
      solver.Add((num_workers > i) + fillers[i].PerformedExpr() == 1);
    }

    // Creates makespan.
    IntVar[] ends = new IntVar[n];
    for (int i = 0; i < n; ++i)
    {
      ends[i] = tasks[i].EndExpr().Var();
    }
    IntVar end_time = ends.Max().VarWithName("end_time");

    //
    // Constraints
    //
    IntervalVar[] all_tasks = new IntervalVar[n + max_num_workers];
    int[] all_demands = new int[n + max_num_workers];
    for (int i = 0; i < n; ++i)
    {
      all_tasks[i] = tasks[i];
      all_demands[i] = demand[i];
    }
    for (int i = 0; i < max_num_workers; ++i)
    {
      all_tasks[i + n] = fillers[i];
      all_demands[i + n] = 1;
    }
    solver.Add(all_tasks.Cumulative(all_demands, max_num_workers, "workers"));

    //
    // Some extra constraints to play with
    //

    // all tasks must end within an hour
    // solver.Add(end_time <= 60);

    // All tasks should start at time 0
    // for(int i = 0; i < n; i++) {
    //   solver.Add(tasks[i].StartAt(0));
    // }


    // limitation of the number of people
    // solver.Add(num_workers <= 3);
    solver.Add(num_workers <= 4);

    //
    // Objective
    //

    // OptimizeVar obj = num_workers.Minimize(1);
    OptimizeVar obj = end_time.Minimize(1);

    //
    // Search
    //
    DecisionBuilder db = solver.MakePhase(all_tasks, Solver.INTERVAL_DEFAULT);

    solver.NewSearch(db, obj);

    while (solver.NextSolution()) {
      Console.WriteLine(num_workers.ToString() + ", " + end_time.ToString());
      for(int i = 0; i < n; i++) {
        Console.WriteLine("{0} (demand:{1})", tasks[i].ToString(), demand[i]);
      }
      Console.WriteLine();
    }

    Console.WriteLine("Solutions: {0}", solver.Solutions());
    Console.WriteLine("WallTime: {0} ms", solver.WallTime());
    Console.WriteLine("Failures: {0}", solver.Failures());
    Console.WriteLine("Branches: {0} ", solver.Branches());

    solver.EndSearch();

  }

  public static void Main(String[] args)
  {
    Solve();
  }
}
