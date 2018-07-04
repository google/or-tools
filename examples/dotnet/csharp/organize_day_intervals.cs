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
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;

public class OrganizeDay
{
  /**
   *
   *
   * Organizing a day.
   *
   * Simple scheduling problem.
   *
   * Problem formulation from ECLiPSe:
   * Slides on (Finite Domain) Constraint Logic Programming, page 38f
   * http://eclipseclp.org/reports/eclipse.ppt
   *
   *
   * Also see http://www.hakank.org/google_or_tools/organize_day.py
   *
   */
  private static void Solve()
  {
    Solver solver = new Solver("OrganizeDayIntervals");


    int n = 4;


    int work = 0;
    int mail = 1;
    int shop = 2;
    int bank = 3;
    // the valid times of the day
    int begin = 9;
    int end   = 17;
    // tasks
    int[] tasks = {work, mail, shop, bank};
    // durations
    int[] durations = {4,1,2,1};
    // Arrays for interval variables.
    int[] starts_max = { begin,begin,begin,begin };
    int[] ends_max = { end -4, end - 1, end - 2, end - 1  };

    // task [i,0] must be finished before task [i,1]
    int[,] before_tasks = {
      {bank, shop},
      {mail, work}
    };



    //
    // Decision variables
    //
    IntervalVar[] intervals =
        solver.MakeFixedDurationIntervalVarArray(n,
                                                 starts_max,
                                                 ends_max,
                                                 durations,
                                                 false,
                                                 "task");
    //
    // Constraints
    //
    DisjunctiveConstraint disjunctive = intervals.Disjunctive("Sequence");
    solver.Add(disjunctive);

    // specific constraints
    for(int t = 0; t < before_tasks.GetLength(0); t++) {
      int before = before_tasks[t, 0];
      int after = before_tasks[t, 1];
      solver.Add(intervals[after].StartsAfterEnd(intervals[before]));
    }

    solver.Add(intervals[work].StartsAfter(11));

    //
    // Search
    //
    SequenceVar var = disjunctive.SequenceVar();
    SequenceVar[] seq_array = new SequenceVar[] { var };
    DecisionBuilder db = solver.MakePhase(seq_array, Solver.SEQUENCE_DEFAULT);

    solver.NewSearch(db);

    while (solver.NextSolution()) {
      foreach(int t in tasks) {
        Console.WriteLine(intervals[t].ToString());
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
