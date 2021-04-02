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

// Gate Scheduling problem.
//
// We have a set of jobs to perform (duration, width).
// We have two parallel machines that can perform this job.
// One machine can only perform one job at a time.
// At any point in time, the sum of the width of the two active jobs does not
// exceed a max_length.
//
// The objective is to minimize the max end time of all jobs.

using System;
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.Sat;

public class GateSchedulingSat
{
    static void Main()
    {
        CpModel model = new CpModel();

        int[,] jobs = new[,] { { 3, 3 },  { 2, 5 }, { 1, 3 }, { 3, 7 }, { 7, 3 }, { 2, 2 }, { 2, 2 }, { 5, 5 },
                               { 10, 2 }, { 4, 3 }, { 2, 6 }, { 1, 2 }, { 6, 8 }, { 4, 5 }, { 3, 7 } };

        int max_length = 10;
        int num_jobs = jobs.GetLength(0);
        var all_jobs = Enumerable.Range(0, num_jobs);

        int horizon = 0;
        foreach (int j in all_jobs)
        {
            horizon += jobs[j, 0];
        }

        List<IntervalVar> intervals = new List<IntervalVar>();
        List<IntervalVar> intervals0 = new List<IntervalVar>();
        List<IntervalVar> intervals1 = new List<IntervalVar>();
        List<IntVar> performed = new List<IntVar>();
        List<IntVar> starts = new List<IntVar>();
        List<IntVar> ends = new List<IntVar>();
        List<int> demands = new List<int>();

        foreach (int i in all_jobs)
        {
            // Create main interval.
            IntVar start = model.NewIntVar(0, horizon, String.Format("start_{0}", i));
            int duration = jobs[i, 0];
            IntVar end = model.NewIntVar(0, horizon, String.Format("end_{0}", i));
            IntervalVar interval = model.NewIntervalVar(start, duration, end, String.Format("interval_{0}", i));
            starts.Add(start);
            intervals.Add(interval);
            ends.Add(end);
            demands.Add(jobs[i, 1]);

            IntVar performed_on_m0 = model.NewBoolVar(String.Format("perform_{0}_on_m0", i));
            performed.Add(performed_on_m0);

            // Create an optional copy of interval to be executed on machine 0.
            IntVar start0 = model.NewIntVar(0, horizon, String.Format("start_{0}_on_m0", i));
            IntVar end0 = model.NewIntVar(0, horizon, String.Format("end_{0}_on_m0", i));
            IntervalVar interval0 = model.NewOptionalIntervalVar(start0, duration, end0, performed_on_m0,
                                                                 String.Format("interval_{0}_on_m0", i));
            intervals0.Add(interval0);

            // Create an optional copy of interval to be executed on machine 1.
            IntVar start1 = model.NewIntVar(0, horizon, String.Format("start_{0}_on_m1", i));
            IntVar end1 = model.NewIntVar(0, horizon, String.Format("end_{0}_on_m1", i));
            IntervalVar interval1 = model.NewOptionalIntervalVar(start1, duration, end1, performed_on_m0.Not(),
                                                                 String.Format("interval_{0}_on_m1", i));
            intervals1.Add(interval1);

            // We only propagate the constraint if the tasks is performed on the
            // machine.
            model.Add(start0 == start).OnlyEnforceIf(performed_on_m0);
            model.Add(start1 == start).OnlyEnforceIf(performed_on_m0.Not());
        }

        // Max Length constraint (modeled as a cumulative)
        model.AddCumulative(intervals, demands, max_length);

        // Choose which machine to perform the jobs on.
        model.AddNoOverlap(intervals0);
        model.AddNoOverlap(intervals1);

        // Objective variable.
        IntVar makespan = model.NewIntVar(0, horizon, "makespan");
        model.AddMaxEquality(makespan, ends);
        model.Minimize(makespan);

        // Symmetry breaking.
        model.Add(performed[0] == 0);

        // Creates the solver and solve.
        CpSolver solver = new CpSolver();
        solver.Solve(model);

        // Output solution.
        Console.WriteLine("Solution");
        Console.WriteLine("  - makespan = " + solver.ObjectiveValue);
        foreach (int i in all_jobs)
        {
            long performed_machine = 1 - solver.Value(performed[i]);
            long start = solver.Value(starts[i]);
            Console.WriteLine(String.Format("  - Job {0} starts at {1} on machine {2}", i, start, performed_machine));
        }
        Console.WriteLine("Statistics");
        Console.WriteLine("  - conflicts       : " + solver.NumConflicts());
        Console.WriteLine("  - branches        : " + solver.NumBranches());
        Console.WriteLine("  - wall time       : " + solver.WallTime() + " ms");
    }
}
