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

using System;
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.Sat;

public class JobshopFt06Sat
{
    public struct Task
    {
        public Task(IntVar s, IntVar e, IntervalVar i)
        {
            start = s;
            end = e;
            interval = i;
        }

        public IntVar start;
        public IntVar end;
        public IntervalVar interval;
    }

    static void Main()
    {
        int[,] durations = new int[,] { { 1, 3, 6, 7, 3, 6 }, { 8, 5, 10, 10, 10, 4 }, { 5, 4, 8, 9, 1, 7 },
                                        { 5, 5, 5, 3, 8, 9 }, { 9, 3, 5, 4, 3, 1 },    { 3, 3, 9, 10, 4, 1 } };
        int[,] machines = new int[,] { { 2, 0, 1, 3, 5, 4 }, { 1, 2, 4, 5, 0, 3 }, { 2, 3, 5, 0, 1, 4 },
                                       { 1, 0, 2, 3, 4, 5 }, { 2, 1, 4, 5, 0, 3 }, { 1, 3, 5, 0, 4, 2 } };

        int num_jobs = durations.GetLength(0);
        int num_machines = durations.GetLength(1);
        var all_jobs = Enumerable.Range(0, num_jobs);
        var all_machines = Enumerable.Range(0, num_machines);

        int horizon = 0;
        foreach (int j in all_jobs)
        {
            foreach (int m in all_machines)
            {
                horizon += durations[j, m];
            }
        }

        // Creates the model.
        CpModel model = new CpModel();

        // Creates jobs.
        Task[,] all_tasks = new Task[num_jobs, num_machines];
        foreach (int j in all_jobs)
        {
            foreach (int m in all_machines)
            {
                IntVar start_var = model.NewIntVar(0, horizon, String.Format("start_{0}_{1}", j, m));
                int duration = durations[j, m];
                IntVar end_var = model.NewIntVar(0, horizon, String.Format("end_{0}_{1}", j, m));
                IntervalVar interval_var =
                    model.NewIntervalVar(start_var, duration, end_var, String.Format("interval_{0}_{1}", j, m));
                all_tasks[j, m] = new Task(start_var, end_var, interval_var);
            }
        }

        // Create disjuctive constraints.
        List<IntervalVar>[] machine_to_jobs = new List<IntervalVar>[num_machines];
        foreach (int m in all_machines)
        {
            machine_to_jobs[m] = new List<IntervalVar>();
        }
        foreach (int j in all_jobs)
        {
            foreach (int m in all_machines)
            {
                machine_to_jobs[machines[j, m]].Add(all_tasks[j, m].interval);
            }
        }
        foreach (int m in all_machines)
        {
            model.AddNoOverlap(machine_to_jobs[m]);
        }

        // Precedences inside a job.
        foreach (int j in all_jobs)
        {
            for (int k = 0; k < num_machines - 1; ++k)
            {
                model.Add(all_tasks[j, k + 1].start >= all_tasks[j, k].end);
            }
        }

        // Makespan objective.
        IntVar[] all_ends = new IntVar[num_jobs];
        foreach (int j in all_jobs)
        {
            all_ends[j] = all_tasks[j, num_machines - 1].end;
        }
        IntVar makespan = model.NewIntVar(0, horizon, "makespan");
        model.AddMaxEquality(makespan, all_ends);
        model.Minimize(makespan);

        Console.WriteLine(model.ModelStats());

        // Creates the solver and solve.
        CpSolver solver = new CpSolver();
        // Display a few solutions picked at random.
        solver.Solve(model);

        // Statistics.
        Console.WriteLine(solver.ResponseStats());
    }
}
