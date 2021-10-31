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

// [START program]
// [START import]
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.Sat;
// [END import]

public class ScheduleRequestsSat
{
    // [START assigned_task]
    private class AssignedTask : IComparable
    {
        public int jobID;
        public int taskID;
        public int start;
        public int duration;

        public AssignedTask(int jobID, int taskID, int start, int duration)
        {
            this.jobID = jobID;
            this.taskID = taskID;
            this.start = start;
            this.duration = duration;
        }

        public int CompareTo(object obj)
        {
            if (obj == null)
                return 1;

            AssignedTask otherTask = obj as AssignedTask;
            if (otherTask != null)
            {
                if (this.start != otherTask.start)
                    return this.start.CompareTo(otherTask.start);
                else
                    return this.duration.CompareTo(otherTask.duration);
            }
            else
                throw new ArgumentException("Object is not a Temperature");
        }
    }
    // [END assigned_task]

    public static void Main(String[] args)
    {
        // [START data]
        var allJobs =
            new[] {
                new[] {
                    // job0
                    new { machine = 0, duration = 3 }, // task0
                    new { machine = 1, duration = 2 }, // task1
                    new { machine = 2, duration = 2 }, // task2
                }
                    .ToList(),
                new[] {
                    // job1
                    new { machine = 0, duration = 2 }, // task0
                    new { machine = 2, duration = 1 }, // task1
                    new { machine = 1, duration = 4 }, // task2
                }
                    .ToList(),
                new[] {
                    // job2
                    new { machine = 1, duration = 4 }, // task0
                    new { machine = 2, duration = 3 }, // task1
                }
                    .ToList(),
            }
                .ToList();

        int numMachines = 0;
        foreach (var job in allJobs)
        {
            foreach (var task in job)
            {
                numMachines = Math.Max(numMachines, 1 + task.machine);
            }
        }
        int[] allMachines = Enumerable.Range(0, numMachines).ToArray();

        // Computes horizon dynamically as the sum of all durations.
        int horizon = 0;
        foreach (var job in allJobs)
        {
            foreach (var task in job)
            {
                horizon += task.duration;
            }
        }
        // [END data]

        // Creates the model.
        // [START model]
        CpModel model = new CpModel();
        // [END model]

        // [START variables]
        Dictionary<Tuple<int, int>, Tuple<IntVar, IntVar, IntervalVar>> allTasks =
            new Dictionary<Tuple<int, int>, Tuple<IntVar, IntVar, IntervalVar>>(); // (start, end, duration)
        Dictionary<int, List<IntervalVar>> machineToIntervals = new Dictionary<int, List<IntervalVar>>();
        for (int jobID = 0; jobID < allJobs.Count(); ++jobID)
        {
            var job = allJobs[jobID];
            for (int taskID = 0; taskID < job.Count(); ++taskID)
            {
                var task = job[taskID];
                String suffix = $"_{jobID}_{taskID}";
                IntVar start = model.NewIntVar(0, horizon, "start" + suffix);
                IntVar end = model.NewIntVar(0, horizon, "end" + suffix);
                IntervalVar interval = model.NewIntervalVar(start, task.duration, end, "interval" + suffix);
                var key = Tuple.Create(jobID, taskID);
                allTasks[key] = Tuple.Create(start, end, interval);
                if (!machineToIntervals.ContainsKey(task.machine))
                {
                    machineToIntervals.Add(task.machine, new List<IntervalVar>());
                }
                machineToIntervals[task.machine].Add(interval);
            }
        }
        // [END variables]

        // [START constraints]
        // Create and add disjunctive constraints.
        foreach (int machine in allMachines)
        {
            model.AddNoOverlap(machineToIntervals[machine]);
        }

        // Precedences inside a job.
        for (int jobID = 0; jobID < allJobs.Count(); ++jobID)
        {
            var job = allJobs[jobID];
            for (int taskID = 0; taskID < job.Count() - 1; ++taskID)
            {
                var key = Tuple.Create(jobID, taskID);
                var nextKey = Tuple.Create(jobID, taskID + 1);
                model.Add(allTasks[nextKey].Item1 >= allTasks[key].Item2);
            }
        }
        // [END constraints]

        // [START objective]
        // Makespan objective.
        IntVar objVar = model.NewIntVar(0, horizon, "makespan");

        List<IntVar> ends = new List<IntVar>();
        for (int jobID = 0; jobID < allJobs.Count(); ++jobID)
        {
            var job = allJobs[jobID];
            var key = Tuple.Create(jobID, job.Count() - 1);
            ends.Add(allTasks[key].Item2);
        }
        model.AddMaxEquality(objVar, ends);
        model.Minimize(objVar);
        // [END objective]

        // Solve
        // [START solve]
        CpSolver solver = new CpSolver();
        CpSolverStatus status = solver.Solve(model);
        Console.WriteLine($"Solve status: {status}");
        // [END solve]

        // [START print_solution]
        if (status == CpSolverStatus.Optimal || status == CpSolverStatus.Feasible)
        {
            Console.WriteLine("Solution:");

            Dictionary<int, List<AssignedTask>> assignedJobs = new Dictionary<int, List<AssignedTask>>();
            for (int jobID = 0; jobID < allJobs.Count(); ++jobID)
            {
                var job = allJobs[jobID];
                for (int taskID = 0; taskID < job.Count(); ++taskID)
                {
                    var task = job[taskID];
                    var key = Tuple.Create(jobID, taskID);
                    int start = (int)solver.Value(allTasks[key].Item1);
                    if (!assignedJobs.ContainsKey(task.machine))
                    {
                        assignedJobs.Add(task.machine, new List<AssignedTask>());
                    }
                    assignedJobs[task.machine].Add(new AssignedTask(jobID, taskID, start, task.duration));
                }
            }

            // Create per machine output lines.
            String output = "";
            foreach (int machine in allMachines)
            {
                // Sort by starting time.
                assignedJobs[machine].Sort();
                String solLineTasks = $"Machine {machine}: ";
                String solLine = "           ";

                foreach (var assignedTask in assignedJobs[machine])
                {
                    String name = $"job_{assignedTask.jobID}_task_{assignedTask.taskID}";
                    // Add spaces to output to align columns.
                    solLineTasks += $"{name,-15}";

                    String solTmp = $"[{assignedTask.start},{assignedTask.start+assignedTask.duration}]";
                    // Add spaces to output to align columns.
                    solLine += $"{solTmp,-15}";
                }
                output += solLineTasks + "\n";
                output += solLine + "\n";
            }
            // Finally print the solution found.
            Console.WriteLine($"Optimal Schedule Length: {solver.ObjectiveValue}");
            Console.WriteLine($"\n{output}");
        }
        else
        {
            Console.WriteLine("No solution found.");
        }
        // [END print_solution]

        // [START statistics]
        Console.WriteLine("Statistics");
        Console.WriteLine($"  conflicts: {solver.NumConflicts()}");
        Console.WriteLine($"  branches : {solver.NumBranches()}");
        Console.WriteLine($"  wall time: {solver.WallTime()}s");
        // [END statistics]
    }
}
// [END program]
