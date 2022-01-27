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

class Task
{
    public Task(int taskId, int jobId, int duration, int machine)
    {
        TaskId = taskId;
        JobId = jobId;
        Duration = duration;
        Machine = machine;
        Name = "T" + taskId + "J" + jobId + "M" + machine + "D" + duration;
    }
    public int TaskId { get; set; }
    public int JobId { get; set; }
    public int Machine { get; set; }
    public int Duration { get; set; }
    public string Name { get; }
}

class JobshopSat
{
    // Number of machines.
    public const int machinesCount = 3;
    // horizon is the upper bound of the start time of all tasks.
    public const int horizon = 300;
    // this will be set to the size of myJobList variable.
    public static int jobsCount;
    /*Search time limit in milliseconds. if it's equal to 0,
    then no time limit will be used.*/
    public const int timeLimitInSeconds = 0;
    public static List<List<Task>> myJobList = new List<List<Task>>();
    public static void InitTaskList()
    {
        List<Task> taskList = new List<Task>();
        taskList.Add(new Task(0, 0, 65, 0));
        taskList.Add(new Task(1, 0, 5, 1));
        taskList.Add(new Task(2, 0, 15, 2));
        myJobList.Add(taskList);

        taskList = new List<Task>();
        taskList.Add(new Task(0, 1, 15, 0));
        taskList.Add(new Task(1, 1, 25, 1));
        taskList.Add(new Task(2, 1, 10, 2));
        myJobList.Add(taskList);

        taskList = new List<Task>();
        taskList.Add(new Task(0, 2, 25, 0));
        taskList.Add(new Task(1, 2, 30, 1));
        taskList.Add(new Task(2, 2, 40, 2));
        myJobList.Add(taskList);

        taskList = new List<Task>();
        taskList.Add(new Task(0, 3, 20, 0));
        taskList.Add(new Task(1, 3, 35, 1));
        taskList.Add(new Task(2, 3, 10, 2));
        myJobList.Add(taskList);

        taskList = new List<Task>();
        taskList.Add(new Task(0, 4, 15, 0));
        taskList.Add(new Task(1, 4, 25, 1));
        taskList.Add(new Task(2, 4, 10, 2));
        myJobList.Add(taskList);

        taskList = new List<Task>();
        taskList.Add(new Task(0, 5, 25, 0));
        taskList.Add(new Task(1, 5, 30, 1));
        taskList.Add(new Task(2, 5, 40, 2));
        myJobList.Add(taskList);

        taskList = new List<Task>();
        taskList.Add(new Task(0, 6, 20, 0));
        taskList.Add(new Task(1, 6, 35, 1));
        taskList.Add(new Task(2, 6, 10, 2));
        myJobList.Add(taskList);

        taskList = new List<Task>();
        taskList.Add(new Task(0, 7, 10, 0));
        taskList.Add(new Task(1, 7, 15, 1));
        taskList.Add(new Task(2, 7, 50, 2));
        myJobList.Add(taskList);

        taskList = new List<Task>();
        taskList.Add(new Task(0, 8, 50, 0));
        taskList.Add(new Task(1, 8, 10, 1));
        taskList.Add(new Task(2, 8, 20, 2));
        myJobList.Add(taskList);

        jobsCount = myJobList.Count;
    }

    public static void Main(String[] args)
    {
        InitTaskList();
        CpModel model = new CpModel();

        // ----- Creates all intervals and integer variables -----

        // Stores all tasks attached interval variables per job.
        List<List<IntervalVar>> jobsToTasks = new List<List<IntervalVar>>(jobsCount);
        List<List<IntVar>> jobsToStarts = new List<List<IntVar>>(jobsCount);
        List<List<IntVar>> jobsToEnds = new List<List<IntVar>>(jobsCount);

        // machinesToTasks stores the same interval variables as above, but
        // grouped my machines instead of grouped by jobs.
        List<List<IntervalVar>> machinesToTasks = new List<List<IntervalVar>>(machinesCount);
        List<List<IntVar>> machinesToStarts = new List<List<IntVar>>(machinesCount);
        for (int i = 0; i < machinesCount; i++)
        {
            machinesToTasks.Add(new List<IntervalVar>());
            machinesToStarts.Add(new List<IntVar>());
        }

        // Creates all individual interval variables.
        foreach (List<Task> job in myJobList)
        {
            jobsToTasks.Add(new List<IntervalVar>());
            jobsToStarts.Add(new List<IntVar>());
            jobsToEnds.Add(new List<IntVar>());
            foreach (Task task in job)
            {
                IntVar start = model.NewIntVar(0, horizon, task.Name);
                IntVar end = model.NewIntVar(0, horizon, task.Name);
                IntervalVar oneTask = model.NewIntervalVar(start, task.Duration, end, task.Name);
                jobsToTasks[task.JobId].Add(oneTask);
                jobsToStarts[task.JobId].Add(start);
                jobsToEnds[task.JobId].Add(end);
                machinesToTasks[task.Machine].Add(oneTask);
                machinesToStarts[task.Machine].Add(start);
            }
        }

        // ----- Creates model -----

        // Creates precedences inside jobs.
        for (int j = 0; j < jobsToTasks.Count; ++j)
        {
            for (int t = 0; t < jobsToTasks[j].Count - 1; ++t)
            {
                model.Add(jobsToEnds[j][t] <= jobsToStarts[j][t + 1]);
            }
        }

        // Adds no_overkap constraints on unary resources.
        for (int machineId = 0; machineId < machinesCount; ++machineId)
        {
            model.AddNoOverlap(machinesToTasks[machineId]);
        }

        // Creates array of end_times of jobs.
        IntVar[] allEnds = new IntVar[jobsCount];
        for (int i = 0; i < jobsCount; i++)
        {
            allEnds[i] = jobsToEnds[i].Last();
        }

        // Objective: minimize the makespan (maximum end times of all tasks)
        // of the problem.
        IntVar makespan = model.NewIntVar(0, horizon, "makespan");
        model.AddMaxEquality(makespan, allEnds);
        model.Minimize(makespan);

        // Create the solver.
        CpSolver solver = new CpSolver();
        // Set the time limit.
        if (timeLimitInSeconds > 0)
        {
            solver.StringParameters = "max_time_in_seconds:" + timeLimitInSeconds;
        }
        // Solve the problem.
        CpSolverStatus status = solver.Solve(model);

        if (status == CpSolverStatus.Optimal)
        {
            Console.WriteLine("Makespan = " + solver.ObjectiveValue);
            for (int m = 0; m < machinesCount; ++m)
            {
                Console.WriteLine($"Machine {m}:");
                SortedDictionary<long, string> starts = new SortedDictionary<long, string>();
                foreach (IntVar var in machinesToStarts[m])
                {
                    starts[solver.Value(var)] = var.Name();
                }
                foreach (KeyValuePair<long, string> p in starts)
                {
                    Console.WriteLine($"  Task {p.Value} starts at {p.Key}");
                }
            }
        }
        else
        {
            Console.WriteLine("No solution found!");
        }
    }
}
