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
using System.Collections.Generic;
using System.Linq;
using System.Diagnostics;
using Google.OrTools.ConstraintSolver;

public class NurseRostering
{

  /**
   *
   * Nurse rostering
   *
   * This is a simple nurse rostering model using a DFA and
   * the built-in TransitionConstraint.
   *
   * The DFA is from MiniZinc Tutorial, Nurse Rostering example:
   * - one day off every 4 days
   * - no 3 nights in a row.
   *
   * Also see:
   *  - http://www.hakank.org/or-tools/nurse_rostering.py
   *  - http://www.hakank.org/or-tools/nurse_rostering_regular.cs
   *    which use (a decomposition of) regular constraint
   *
   */
  private static void Solve(int nurse_multiplier, int week_multiplier)
  {
    Console.WriteLine("Starting Nurse Rostering");
    Console.WriteLine("  - {0} teams of 7 nurses", nurse_multiplier);
    Console.WriteLine("  - {0} blocks of 14 days", week_multiplier);

    Solver solver = new Solver("NurseRostering");

    //
    // Data
    //

    // Note: If you change num_nurses or num_days,
    //       please also change the constraints
    //       on nurse_stat and/or day_stat.
    int num_nurses = 7 * nurse_multiplier;
    int num_days = 14 * week_multiplier;

    // Note: I had to add a dummy shift.
    int dummy_shift = 0;
    int day_shift = 1;
    int night_shift = 2;
    int off_shift = 3;
    int[] shifts = {dummy_shift, day_shift, night_shift, off_shift};
    int[] valid_shifts = {day_shift, night_shift, off_shift};

    // the DFA (for regular)
    int initial_state = 1;
    int[] accepting_states = {1,2,3,4,5,6};

    /*
      // This is the transition function
      // used in nurse_rostering_regular.cs
    int[,] transition_fn = {
      // d,n,o
      {2,3,1}, // state 1
      {4,4,1}, // state 2
      {4,5,1}, // state 3
      {6,6,1}, // state 4
      {6,0,1}, // state 5
      {0,0,1}  // state 6
    };
    */

    // For TransitionConstraint
    IntTupleSet transition_tuples = new IntTupleSet(3);
    // state, input, next state
    transition_tuples.InsertAll(new int[,] { {1,1,2},
                                             {1,2,3},
                                             {1,3,1},
                                             {2,1,4},
                                             {2,2,4},
                                             {2,3,1},
                                             {3,1,4},
                                             {3,2,5},
                                             {3,3,1},
                                             {4,1,6},
                                             {4,2,6},
                                             {4,3,1},
                                             {5,1,6},
                                             {5,3,1},
                                             {6,3,1} });

    string[] days = {"d","n","o"}; // for presentation

    //
    // Decision variables
    //

    //
    // For TransitionConstraint
    //
    IntVar[,] x =
        solver.MakeIntVarMatrix(num_nurses, num_days, valid_shifts, "x");
    IntVar[] x_flat = x.Flatten();

    //
    // summary of the nurses
    //
    IntVar[] nurse_stat = new IntVar[num_nurses];

    //
    // summary of the shifts per day
    //
    int num_shifts = shifts.Length;
    IntVar[,] day_stat = new IntVar[num_days, num_shifts];
    for(int i = 0; i < num_days; i++) {
      for(int j = 0; j < num_shifts; j++) {
        day_stat[i,j] = solver.MakeIntVar(0, num_nurses, "day_stat");
      }
    }

    //
    // Constraints
    //
    for(int i = 0; i < num_nurses; i++) {
      IntVar[] reg_input = new IntVar[num_days];
      for(int j = 0; j < num_days; j++) {
        reg_input[j] = x[i,j];
      }

      solver.Add(reg_input.Transition(transition_tuples,
                                      initial_state,
                                      accepting_states));
    }

    //
    // Statistics and constraints for each nurse
    //
    for(int nurse = 0; nurse < num_nurses; nurse++) {

      // Number of worked days (either day or night shift)
      IntVar[] nurse_days = new IntVar[num_days];
      for(int day = 0; day < num_days; day++) {
        nurse_days[day] =
            x[nurse, day].IsMember(new int[] { day_shift, night_shift });
      }
      nurse_stat[nurse] = nurse_days.Sum().Var();

      // Each nurse must work between 7 and 10
      // days/nights during this period
      solver.Add(nurse_stat[nurse] >= 7 * week_multiplier / nurse_multiplier);
      solver.Add(nurse_stat[nurse] <= 10 * week_multiplier / nurse_multiplier);

    }

    //
    // Statistics and constraints for each day
    //
    for(int day = 0; day < num_days; day++) {
      IntVar[] nurses = new IntVar[num_nurses];
      for(int nurse = 0; nurse < num_nurses; nurse++) {
        nurses[nurse] = x[nurse, day];
      }
      IntVar[] stats = new IntVar[num_shifts];
      for (int shift = 0; shift < num_shifts; ++shift)
      {
        stats[shift] = day_stat[day, shift];
      }
      solver.Add(nurses.Distribute(stats));

      //
      // Some constraints for each day:
      //
      // Note: We have a strict requirements of
      //       the number of shifts.
      //       Using atleast constraints is harder
      //       in this model.
      //
      if (day % 7 == 5 || day % 7 == 6) {
        // special constraints for the weekends
        solver.Add(day_stat[day, day_shift] == 2 * nurse_multiplier);
        solver.Add(day_stat[day, night_shift] == nurse_multiplier);
        solver.Add(day_stat[day, off_shift] == 4 * nurse_multiplier);
      } else {
        // for workdays:

        // - exactly 3 on day shift
        solver.Add(day_stat[day, day_shift] == 3 * nurse_multiplier);
        // - exactly 2 on night
        solver.Add(day_stat[day, night_shift] == 2 * nurse_multiplier);
        // - exactly 2 off duty
        solver.Add(day_stat[day, off_shift] == 2 * nurse_multiplier);
      }
    }

    //
    // Search
    //
    DecisionBuilder db = solver.MakePhase(x_flat,
                                          Solver.CHOOSE_FIRST_UNBOUND,
                                          Solver.ASSIGN_MIN_VALUE);

    SearchMonitor log = solver.MakeSearchLog(1000000);

    solver.NewSearch(db, log);

    int num_solutions = 0;
    while (solver.NextSolution()) {
      num_solutions++;
      for(int i = 0; i < num_nurses; i++) {
        Console.Write("Nurse #{0,-2}: ", i);
        var occ = new Dictionary<int, int>();
        for(int j = 0; j < num_days; j++) {
          int v = (int)x[i,j].Value()-1;
          if (!occ.ContainsKey(v)) {
            occ[v] = 0;
          }
          occ[v]++;
          Console.Write(days[v] + " ");
        }

        Console.Write(" #workdays: {0,2}", nurse_stat[i].Value());
        foreach(int s in valid_shifts) {
          int v = 0;
          if (occ.ContainsKey(s-1)) {
            v = occ[s-1];
          }
          Console.Write("  {0}:{1}", days[s-1], v);
        }
        Console.WriteLine();

      }
      Console.WriteLine();

      Console.WriteLine("Statistics per day:\nDay      d n o");
      for(int j = 0; j < num_days; j++) {
        Console.Write("Day #{0,2}: ", j);
        foreach(int t in valid_shifts) {
          Console.Write(day_stat[j,t].Value() + " ");
        }
        Console.WriteLine();
      }
      Console.WriteLine();

      // We just show 2 solutions
      if (num_solutions > 1) {
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
    int nurse_multiplier = 1;
    int week_multiplier = 1;
    if (args.Length > 0) {
      nurse_multiplier = Convert.ToInt32(args[0]);
    }
    if (args.Length > 1) {
      week_multiplier = Convert.ToInt32(args[1]);
    }

    Solve(nurse_multiplier, week_multiplier);
  }
}
