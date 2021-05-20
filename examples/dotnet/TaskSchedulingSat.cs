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

﻿using System;
using System.Collections.Generic;
using Google.OrTools.Sat;

class Job
{
    public Job(List<Task> tasks)
    {
        AlternativeTasks = tasks;
    }
    public Job Successor { get; set; }
    public List<Task> AlternativeTasks { get; set; }
}

class Task
{
    public Task(string name, long duration, long equipment)
    {
        Name = name;
        Duration = duration;
        Equipment = equipment;
    }

    public string Name { get; set; }
    public long StartTime { get; set; }
    public long EndTime
    {
        get {
            return StartTime + Duration;
        }
    }
    public long Duration { get; set; }
    public long Equipment { get; set; }

    public override string ToString()
    {
        return Name + " [ " + Equipment + " ]\tstarts: " + StartTime + " ends:" + EndTime + ", duration: " + Duration;
    }
}

class TaskSchedulingSat
{
    public static List<Job> myJobList = new List<Job>();
    public static Dictionary<long, List<IntervalVar>> tasksToEquipment = new Dictionary<long, List<IntervalVar>>();
    public static Dictionary<string, long> taskIndexes = new Dictionary<string, long>();

    public static void InitTaskList()
    {
        List<Task> taskList = new List<Task>();
        taskList.Add(new Task("Job1Task0a", 15, 0));
        taskList.Add(new Task("Job1Task0b", 25, 1));
        taskList.Add(new Task("Job1Task0c", 10, 2));
        myJobList.Add(new Job(taskList));

        taskList = new List<Task>();
        taskList.Add(new Task("Job1Task1a", 25, 0));
        taskList.Add(new Task("Job1Task1b", 30, 1));
        taskList.Add(new Task("Job1Task1c", 40, 2));
        myJobList.Add(new Job(taskList));

        taskList = new List<Task>();
        taskList.Add(new Task("Job1Task2a", 20, 0));
        taskList.Add(new Task("Job1Task2b", 35, 1));
        taskList.Add(new Task("Job1Task2c", 10, 2));
        myJobList.Add(new Job(taskList));

        taskList = new List<Task>();
        taskList.Add(new Task("Job2Task0a", 15, 0));
        taskList.Add(new Task("Job2Task0b", 25, 1));
        taskList.Add(new Task("Job2Task0c", 10, 2));
        myJobList.Add(new Job(taskList));

        taskList = new List<Task>();
        taskList.Add(new Task("Job2Task1a", 25, 0));
        taskList.Add(new Task("Job2Task1b", 30, 1));
        taskList.Add(new Task("Job2Task1c", 40, 2));
        myJobList.Add(new Job(taskList));

        taskList = new List<Task>();
        taskList.Add(new Task("Job2Task2a", 20, 0));
        taskList.Add(new Task("Job2Task2b", 35, 1));
        taskList.Add(new Task("Job2Task2c", 10, 2));
        myJobList.Add(new Job(taskList));

        taskList = new List<Task>();
        taskList.Add(new Task("Job3Task0a", 10, 0));
        taskList.Add(new Task("Job3Task0b", 15, 1));
        taskList.Add(new Task("Job3Task0c", 50, 2));
        myJobList.Add(new Job(taskList));

        taskList = new List<Task>();
        taskList.Add(new Task("Job3Task1a", 50, 0));
        taskList.Add(new Task("Job3Task1b", 10, 1));
        taskList.Add(new Task("Job3Task1c", 20, 2));
        myJobList.Add(new Job(taskList));

        taskList = new List<Task>();
        taskList.Add(new Task("Job3Task2a", 65, 0));
        taskList.Add(new Task("Job3Task2b", 5, 1));
        taskList.Add(new Task("Job3Task2c", 15, 2));
        myJobList.Add(new Job(taskList));

        myJobList[0].Successor = myJobList[1];
        myJobList[1].Successor = myJobList[2];
        myJobList[2].Successor = null;

        myJobList[3].Successor = myJobList[4];
        myJobList[4].Successor = myJobList[5];
        myJobList[5].Successor = null;

        myJobList[6].Successor = myJobList[7];
        myJobList[7].Successor = myJobList[8];
        myJobList[8].Successor = null;
    }

    public static int GetTaskCount()
    {
        int c = 0;
        foreach (Job j in myJobList)
            foreach (Task t in j.AlternativeTasks)
            {
                taskIndexes[t.Name] = c;
                c++;
            }

        return c;
    }

    public static int GetEndTaskCount()
    {
        int c = 0;
        foreach (Job j in myJobList)
            if (j.Successor == null)
                c += j.AlternativeTasks.Count;
        return c;
    }

    static void Main()
    {
        InitTaskList();
        int taskCount = GetTaskCount();

        CpModel model = new CpModel();

        IntervalVar[] tasks = new IntervalVar[taskCount];
        IntVar[] taskChoosed = new IntVar[taskCount];
        IntVar[] allEnds = new IntVar[GetEndTaskCount()];

        int endJobCounter = 0;
        foreach (Job j in myJobList)
        {
            IntVar[] tmp = new IntVar[j.AlternativeTasks.Count];
            int i = 0;
            foreach (Task t in j.AlternativeTasks)
            {
                long ti = taskIndexes[t.Name];
                taskChoosed[ti] = model.NewBoolVar(t.Name + "_choose");
                tmp[i++] = taskChoosed[ti];
                IntVar start = model.NewIntVar(0, 10000, t.Name + "_start");
                IntVar end = model.NewIntVar(0, 10000, t.Name + "_end");
                tasks[ti] = model.NewIntervalVar(start, t.Duration, end, t.Name + "_interval");
                if (j.Successor == null)
                    allEnds[endJobCounter++] = end;
                if (!tasksToEquipment.ContainsKey(t.Equipment))
                    tasksToEquipment[t.Equipment] = new List<IntervalVar>();
                tasksToEquipment[t.Equipment].Add(tasks[ti]);
            }
            model.Add(LinearExpr.Sum(tmp) == 1);
        }

        foreach (KeyValuePair<long, List<IntervalVar>> pair in tasksToEquipment)
        {
            model.AddNoOverlap(pair.Value);
        }

        IntVar makespan = model.NewIntVar(0, 100000, "makespan");
        model.AddMaxEquality(makespan, allEnds);
        model.Minimize(makespan);

        // Create the solver.
        CpSolver solver = new CpSolver();
        // Solve the problem.
        solver.Solve(model);
        Console.WriteLine(solver.ResponseStats());
    }
}
