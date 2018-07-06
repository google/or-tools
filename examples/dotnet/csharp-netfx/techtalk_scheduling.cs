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

using Google.OrTools.ConstraintSolver;
using Google.OrTools.Graph;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System;

public class SpeakerScheduling
{

  public class FlowAssign : NetDecisionBuilder
  {
    public FlowAssign(IntVar[] vars, int first_slot, IntVar last_slot_var)
    {
      vars_ = vars;
      first_slot_ = first_slot;
      last_slot_var_ = last_slot_var;
    }

    public override Decision Next(Solver solver)
    {
      int large = 100000;
      int number_of_variables = vars_.Length;
      long last_slot = last_slot_var_.Max();
      // Lets build a bipartite graph with equal number of nodes left and right.
      // Variables will be on the left, slots on the right.
      // We will add dummy variables when needed.
      // Arcs will have a cost x is slot x is possible for a variable, a large
      // number otherwise. For dummy variables, the cost will be 0 always.
      LinearSumAssignment matching = new LinearSumAssignment();
      for (int speaker = 0; speaker < number_of_variables; ++speaker)
      {
        IntVar var = vars_[speaker];
        for (int value = first_slot_; value <= last_slot; ++value)
        {
          if (var.Contains(value))
          {
            matching.AddArcWithCost(speaker, value - first_slot_, value);
          }
          else
          {
            matching.AddArcWithCost(speaker, value - first_slot_, large);
          }
        }
      }
      // The Matching algorithms expect the same number of left and right nodes.
      // So we fill the rest with dense zero-cost arcs.
      for (int dummy = number_of_variables;
           dummy <= last_slot - first_slot_; ++dummy) {
        for (int value = first_slot_; value <= last_slot; ++value)
        {
          matching.AddArcWithCost(dummy, value - first_slot_, 0);
        }
      }
      if (matching.Solve() == LinearSumAssignment.OPTIMAL &&
          matching.OptimalCost() < large)  // No violated arcs.
      {
        for (int speaker = 0; speaker < number_of_variables; ++speaker)
        {
          vars_[speaker].SetValue(matching.RightMate(speaker) + first_slot_);
        }
      } else {
        solver.Fail();
      }
      return null;
    }

    private IntVar[] vars_;
    private int first_slot_;
    private IntVar last_slot_var_;
  }

  private static void Solve(int first_slot)
  {
    Console.WriteLine("----------------------------------------------------");
    Solver solver = new Solver("SpeakerScheduling");

    // the slots each speaker is available
    int[][] speaker_availability = {
      new int[] {1,3,4,6,7,10,12,13,14,15,16,18,19,20,21,22,23,24,25,33,34,35,36,37,38,39,40,41,43,44,45,46,47,48,49,50,51,52,54,55,56,57,58,59},
      new int[] {1,2,7,8,10,11,16,17,18,21,22,23,24,25,33,34,35,36,37,38,39,40,42,43,44,45,46,47,48,49,52,53,54,55,56,57,58,59,60},
      new int[] {5,6,7,10,12,14,16,17,21,22,23,24,33,35,37,38,39,40,41,42,43,44,45,46,51,53,55,56,57,58,59},
      new int[] {1,2,3,4,5,6,7,11,13,14,15,16,20,24,25,33,34,35,37,38,39,40,41,43,44,45,46,47,48,49,50,51,52,53,54,55,56,58,59,60},
      new int[] {4,7,8,9,16,17,19,20,21,22,23,24,25,33,34,35,36,37,38,39,40,41,42,43,49,50,51,53,55,56,57,58,59,60},
      new int[] {4,7,9,11,12,13,14,15,16,17,18,22,23,24,33,34,35,36,38,39,42,44,46,48,49,51,53,54,55,56,57},
      new int[] {1,2,3,4,5,6,7,33,34,35,36,37,38,39,40,41,42,54,55,56,57,58,59,60},
      new int[] {1,3,11,14,15,16,17,21,22,23,24,25,33,35,36,37,39,40,41,42,43,44,45,47,48,49,51,52,53,54,55,56,57,58,59,60},
      new int[] {1,2,3,4,5,7,8,9,10,11,13,18,19,20,21,22,23,24,25,33,34,35,36,37,38,39,40,41,42,43,44,45,46,50,51,52,53,54,55,56,57,59,60},
      new int[] {24,33,34,35,36,37,38,39,40,41,42,43,45,49,50,51,52,53,54,55,56,57,58,59,60},
      new int[] {1,2,3,4,5,6,7,8,9,10,11,12,13,14,16,17,18,19,20,22,23,24,25,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,50,51,52,53,55,56,57,58,59,60},
      new int[] {3,4,5,6,13,15,16,17,18,19,21,22,24,25,33,34,35,36,37,39,40,41,42,43,44,45,47,52,53,55,57,58,59,60},
      new int[] {4,5,6,8,11,12,13,14,17,19,20,22,23,24,25,33,34,35,36,37,39,40,41,42,43,47,48,49,50,51,52,55,56,57},
      new int[] {2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60},
      new int[] {12,25,33,35,36,37,39,41,42,43,48,51,52,53,54,57,59,60},
      new int[] {4,8,16,17,19,23,25,33,34,35,37,41,44,45,47,48,49,50},
      new int[] {3,23,24,25,33,35,36,37,38,39,40,42,43,44,49,50,53,54,55,56,57,58,60},
      new int[] {7,13,19,20,22,23,24,25,33,34,35,38,40,41,42,44,45,46,47,48,49,52,53,54,58,59,60}
    };

    // how long each talk lasts for each speaker
    int[] durations = { 1, 2, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    int sum_of_durations = durations.Sum();

    int number_of_speakers = durations.Length;
    // calculate the total number of slots (maximum in the availability array)
    // (and add the max durations)
    int last_slot = (from s in Enumerable.Range(0, number_of_speakers)
                    select speaker_availability[s].Max()).Max();
    Console.WriteLine(
        "Scheduling {0} speakers, for a total of {1} slots, during [{2}..{3}]",
        number_of_speakers, sum_of_durations, first_slot, last_slot);

    // Start variable for all talks.
    IntVar[] starts = new IntVar[number_of_speakers];
    // We store the possible starts for all talks filtered from the
    // duration and the speaker availability.
    int[][] possible_starts = new int[number_of_speakers][];

    for (int speaker = 0; speaker < number_of_speakers; ++speaker)
    {
      int duration = durations[speaker];
      // Let's filter the possible starts.
      List<int> filtered_starts = new List<int>();
      int availability = speaker_availability[speaker].Length;
      for (int index = 0; index < availability; ++index)
      {
        bool ok = true;
        int slot = speaker_availability[speaker][index];
        if (slot < first_slot)
        {
          continue;
        }
        for (int offset = 1; offset < duration; ++offset)
        {
          if (index + offset >= availability ||
              speaker_availability[speaker][index + offset] != slot + offset)
          {
            // discontinuity.
            ok = false;
            break;
          }
        }
        if (ok)
        {
          filtered_starts.Add(slot);
        }
        possible_starts[speaker] = filtered_starts.ToArray();
      }
      starts[speaker] =
          solver.MakeIntVar(possible_starts[speaker], "start[" + speaker + "]");
    }

    List<IntVar>[] contributions_per_slot =
        new List<IntVar>[last_slot + 1];
    for (int slot = first_slot; slot <= last_slot; ++slot)
    {
      contributions_per_slot[slot] = new List<IntVar>();
    }
    for (int speaker = 0; speaker < number_of_speakers; ++speaker)
    {
      int duration = durations[speaker];
      IntVar start_var = starts[speaker];
      foreach (int start in possible_starts[speaker])
      {
        for (int offset = 0; offset < duration; ++offset)
        {
          contributions_per_slot[start + offset].Add(start_var.IsEqual(start));
        }
      }
    }
    // Force the schedule to be consistent.
    for (int slot = first_slot; slot <= last_slot; ++slot)
    {
      solver.Add(
          solver.MakeSumLessOrEqual(contributions_per_slot[slot].ToArray(), 1));
    }

    // Add minimum start info.
    for (int speaker = 0; speaker < number_of_speakers; ++speaker)
    {
      solver.Add(starts[speaker] >= first_slot);
    }

    // Creates makespan.
    IntVar[] end_times = new IntVar[number_of_speakers];
    for (int speaker = 0; speaker < number_of_speakers; speaker++)
    {
      end_times[speaker] = (starts[speaker] + (durations[speaker] - 1)).Var();
    }
    IntVar last_slot_var = end_times.Max().VarWithName("last_slot");

    // Add trivial bound to objective.
    last_slot_var.SetMin(first_slot + sum_of_durations - 1);

    // Redundant scheduling constraint.
    IntervalVar[] intervals =
        solver.MakeFixedDurationIntervalVarArray(starts, durations, "intervals");
    DisjunctiveConstraint disjunctive =
        solver.MakeDisjunctiveConstraint(intervals, "disjunctive");
    solver.Add(disjunctive);

    //
    // Search
    //
    List<IntVar> short_talks = new List<IntVar>();
    List<IntVar> long_talks = new List<IntVar>();
    for (int speaker = 0; speaker < number_of_speakers; ++speaker)
    {
      if (durations[speaker] == 1)
      {
        short_talks.Add(starts[speaker]);
      }
      else
      {
        long_talks.Add(starts[speaker]);
      }
    }
    OptimizeVar objective_monitor = solver.MakeMinimize(last_slot_var, 1);
    DecisionBuilder long_phase =
        solver.MakePhase(long_talks.ToArray(),
                         Solver.CHOOSE_MIN_SIZE_LOWEST_MIN,
                         Solver.ASSIGN_MIN_VALUE);
    DecisionBuilder short_phase =
        new FlowAssign(short_talks.ToArray(), first_slot, last_slot_var);
    DecisionBuilder obj_phase =
        solver.MakePhase(last_slot_var,
                         Solver.CHOOSE_FIRST_UNBOUND,
                         Solver.ASSIGN_MIN_VALUE);
    DecisionBuilder main_phase =
        solver.Compose(long_phase, short_phase, obj_phase);

    solver.NewSearch(main_phase, objective_monitor);
    while (solver.NextSolution())
    {
      Console.WriteLine("\nLast used slot: " + (last_slot_var.Value()));
      Console.WriteLine("Speakers (start..end):");
      for (int s = 0; s < number_of_speakers; s++)
      {
        long sstart = starts[s].Value();
        Console.WriteLine("  - speaker {0,2}: {1,2}..{2,2}", (s + 1),
                          sstart, (sstart + durations[s] - 1));
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
    int start = 1;
    if (args.Length == 1)
    {
      start = int.Parse(args[0]);
    }
    Stopwatch s = new Stopwatch();
    s.Start();
    for (int i = start; i < 40; i++)
    {
      Solve(i);
    }

    s.Stop();
    Console.WriteLine("Finished in " + s.ElapsedMilliseconds + " ms");
  }
}
