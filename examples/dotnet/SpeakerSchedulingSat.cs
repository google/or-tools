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

using Google.OrTools.Sat;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System;

class SpeakerScheduling
{
    struct Entry
    {
        public IntVar var;
        public int start;

        public Entry(IntVar v, int s)
        {
            var = v;
            start = s;
        }
    }

    static void Solve(int first_slot)
    {
        Console.WriteLine("----------------------------------------------------");

        // the slots each speaker is available
        int[][] speaker_availability = {
            new int[] { 1,  3,  4,  6,  7,  10, 12, 13, 14, 15, 16, 18, 19, 20, 21, 22, 23, 24, 25, 33, 34, 35,
                        36, 37, 38, 39, 40, 41, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 55, 56, 57, 58, 59 },
            new int[] { 1,  2,  7,  8,  10, 11, 16, 17, 18, 21, 22, 23, 24, 25, 33, 34, 35, 36, 37, 38,
                        39, 40, 42, 43, 44, 45, 46, 47, 48, 49, 52, 53, 54, 55, 56, 57, 58, 59, 60 },
            new int[] { 5,  6,  7,  10, 12, 14, 16, 17, 21, 22, 23, 24, 33, 35, 37, 38,
                        39, 40, 41, 42, 43, 44, 45, 46, 51, 53, 55, 56, 57, 58, 59 },
            new int[] { 1,  2,  3,  4,  5,  6,  7,  11, 13, 14, 15, 16, 20, 24, 25, 33, 34, 35, 37, 38,
                        39, 40, 41, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 58, 59, 60 },
            new int[] { 4,  7,  8,  9,  16, 17, 19, 20, 21, 22, 23, 24, 25, 33, 34, 35, 36,
                        37, 38, 39, 40, 41, 42, 43, 49, 50, 51, 53, 55, 56, 57, 58, 59, 60 },
            new int[] { 4,  7,  9,  11, 12, 13, 14, 15, 16, 17, 18, 22, 23, 24, 33, 34,
                        35, 36, 38, 39, 42, 44, 46, 48, 49, 51, 53, 54, 55, 56, 57 },
            new int[] { 1, 2, 3, 4, 5, 6, 7, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 54, 55, 56, 57, 58, 59, 60 },
            new int[] { 1,  3,  11, 14, 15, 16, 17, 21, 22, 23, 24, 25, 33, 35, 36, 37, 39, 40,
                        41, 42, 43, 44, 45, 47, 48, 49, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60 },
            new int[] { 1,  2,  3,  4,  5,  7,  8,  9,  10, 11, 13, 18, 19, 20, 21, 22, 23, 24, 25, 33, 34, 35,
                        36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 50, 51, 52, 53, 54, 55, 56, 57, 59, 60 },
            new int[] { 24, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 45,
                        49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60 },
            new int[] { 1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 16, 17, 18,
                        19, 20, 22, 23, 24, 25, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
                        44, 45, 46, 47, 48, 50, 51, 52, 53, 55, 56, 57, 58, 59, 60 },
            new int[] { 3,  4,  5,  6,  13, 15, 16, 17, 18, 19, 21, 22, 24, 25, 33, 34, 35,
                        36, 37, 39, 40, 41, 42, 43, 44, 45, 47, 52, 53, 55, 57, 58, 59, 60 },
            new int[] { 4,  5,  6,  8,  11, 12, 13, 14, 17, 19, 20, 22, 23, 24, 25, 33, 34,
                        35, 36, 37, 39, 40, 41, 42, 43, 47, 48, 49, 50, 51, 52, 55, 56, 57 },
            new int[] { 2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                        20, 21, 22, 23, 24, 25, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
                        45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60 },
            new int[] { 12, 25, 33, 35, 36, 37, 39, 41, 42, 43, 48, 51, 52, 53, 54, 57, 59, 60 },
            new int[] { 4, 8, 16, 17, 19, 23, 25, 33, 34, 35, 37, 41, 44, 45, 47, 48, 49, 50 },
            new int[] { 3, 23, 24, 25, 33, 35, 36, 37, 38, 39, 40, 42, 43, 44, 49, 50, 53, 54, 55, 56, 57, 58, 60 },
            new int[] { 7,  13, 19, 20, 22, 23, 24, 25, 33, 34, 35, 38, 40, 41,
                        42, 44, 45, 46, 47, 48, 49, 52, 53, 54, 58, 59, 60 }
        };
        // how long each talk lasts for each speaker
        int[] durations = { 1, 2, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        int sum_of_durations = durations.Sum();
        int number_of_speakers = durations.Length;

        // calculate the total number of slots (maximum in the availability array)
        // (and add the max durations)
        int last_slot = (from s in Enumerable.Range(0, number_of_speakers) select speaker_availability[s].Max()).Max();
        Console.WriteLine($"Scheduling {number_of_speakers} speakers, for a total of " +
                          $"{sum_of_durations} slots, during [{first_slot}..{last_slot}]");

        CpModel model = new CpModel();

        // We store the possible entries (var, start) for all talks filtered
        // from the duration and the speaker availability.
        List<Entry>[] entries = new List<Entry>[number_of_speakers];
        for (int speaker = 0; speaker < number_of_speakers; ++speaker)
        {
            entries[speaker] = new List<Entry>();
        }

        List<IntVar>[] contributions_per_slot = new List<IntVar>[last_slot + 1];
        for (int slot = 1; slot <= last_slot; ++slot)
        {
            contributions_per_slot[slot] = new List<IntVar>();
        }

        for (int speaker = 0; speaker < number_of_speakers; ++speaker)
        {
            List<IntVar> all_vars = new List<IntVar>();
            int duration = durations[speaker];
            // Let's filter the possible starts.
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
                    IntVar var = model.NewBoolVar("speaker " + (speaker + 1) + " starts at " + slot);
                    entries[speaker].Add(new Entry(var, slot));
                    all_vars.Add(var);
                    for (int offset = 0; offset < duration; ++offset)
                    {
                        contributions_per_slot[slot + offset].Add(var);
                    }
                }
            }
            model.Add(LinearExpr.Sum(all_vars) == 1);
        }
        // Force the schedule to be consistent.
        for (int slot = first_slot; slot <= last_slot; ++slot)
        {
            model.Add(LinearExpr.Sum(contributions_per_slot[slot]) <= 1);
        }

        // Creates last_slot.
        IntVar last_slot_var = model.NewIntVar(first_slot + sum_of_durations - 1, last_slot, "last_slot");
        for (int speaker = 0; speaker < number_of_speakers; speaker++)
        {
            int duration = durations[speaker];
            foreach (Entry e in entries[speaker])
            {
                model.Add(last_slot_var >= e.start + duration - 1).OnlyEnforceIf(e.var);
            }
        }

        model.Minimize(last_slot_var);

        // Creates the solver and solve.
        CpSolver solver = new CpSolver();
        solver.StringParameters = "num_search_workers:8";
        CpSolverStatus status = solver.Solve(model);

        if (status == CpSolverStatus.Optimal)
        {
            Console.WriteLine("\nLast used slot: " + solver.Value(last_slot_var));
            Console.WriteLine("Speakers (start..end):");
            for (int speaker = 0; speaker < number_of_speakers; speaker++)
            {
                foreach (Entry e in entries[speaker])
                {
                    if (solver.BooleanValue(e.var))
                    {
                        Console.WriteLine("  - speaker {0,2}: {1,2}..{2,2}", (speaker + 1), e.start,
                                          (e.start + durations[speaker] - 1));
                    }
                }
            }
        }

        // Statistics.
        Console.WriteLine(solver.ResponseStats());
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