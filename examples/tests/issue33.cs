// Authors: Johan Wessén
// Copyright 2010-2025 Google LLC
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
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System;

public class Task
{
    public int Id { get; private set; }
    public int TaskType { get; private set; }
    public int LocationId { get; private set; }
    public Dictionary<int, int> Durations { get; private set; }
    public int TaskPosition { get; private set; }

    public Task(int id, int taskType, int locationIndex, int taskPosition, Dictionary<int, int> durations)
    {
        Id = id;
        TaskType = taskType;
        LocationId = locationIndex;
        Durations = durations;
        TaskPosition = taskPosition;
    }

    public Task(int id, int taskType, int locationIndex, int taskPosition)
    {
        Id = id;
        TaskType = taskType;
        LocationId = locationIndex;
        TaskPosition = taskPosition;
        Durations = new Dictionary<int, int>();
    }
}

public class WorkLocation
{
    public int Id { get; private set; }
    public int NbTasks
    {
        get {
            Debug.Assert(Tasks != null);
            return Tasks.Length;
        }
        set {
            Debug.Assert(Tasks == null);
            Tasks = new Task[value];
        }
    }
    public Task[] Tasks { get; private set; }

    public WorkLocation(int index)
    {
        Id = index;
    }
}

public class Tool
{
    public int Id { get; private set; }
    public HashSet<int> TaskTypes { get; set; }
    public int[,] TravellingTime { get; set; }
    public int InitialLocationId { get; set; }

    public Tool(int index, int initialLocation = 0)
    {
        Id = index;
        InitialLocationId = initialLocation;
        TaskTypes = new HashSet<int>();
    }

    public void AddTaskType(int t)
    {
        TaskTypes.Add(t);
    }

    public bool CanPerformTaskType(int taskType)
    {
        return TaskTypes.Contains(taskType);
    }
}

public class FactoryDescription
{
    public Tool[] Tools { get; private set; }
    public WorkLocation[] Locations { get; private set; }

    public int NbWorkLocations
    {
        get {
            return Locations.Length;
        }
    }
    public int NbTools
    {
        get {
            return Tools.Length;
        }
    }

    public int NbTaskPerCycle { get; private set; }
    // TaskType go typically from 0 to 6. InspectionType indicates which
    // is the TaskType that correspond to Inspection.
    public int Inspection { get; private set; }
    // All the time within the schedule horizon in which the blast can start.
    public long[] InspectionStarts { get; private set; }

    public int Horizon { get; private set; }

    // horizon equal to 2 weeks (in minutes).
    public FactoryDescription(int nbTools, int nbLocations, int nbTaskPerCycle, int horizon = 14 * 24 * 60)
    {
        Debug.Assert(nbTools > 0);
        Debug.Assert(nbLocations > 0);
        Debug.Assert(nbTaskPerCycle > 0);
        Debug.Assert(horizon > 0);
        NbTaskPerCycle = nbTaskPerCycle;
        Inspection = NbTaskPerCycle - 1;
        Tools = new Tool[nbTools];
        Horizon = horizon;
        for (int i = 0; i < nbTools; i++)
            Tools[i] = new Tool(i);
        Locations = new WorkLocation[nbLocations];
        for (int i = 0; i < nbLocations; i++)
            Locations[i] = new WorkLocation(i);

        InspectionStarts = new long[] { -1, 600, 1200, 1800, 2400, 2800 };
    }

    public Tool[] getToolPerTaskType(int taskType)
    {
        var elements = from tool in Tools
                       where tool.CanPerformTaskType(taskType) select tool;
        return elements.ToArray();
    }

    public Task[] getFlatTaskList()
    {
        return (from location in Locations from task in location.Tasks orderby task.Id select task).ToArray();
    }

    public int[] getTaskTypes()
    {
        return (from location in Locations from task in location.Tasks select task.TaskType).Distinct().ToArray();
    }

    // TODO: This should be enhanced
    public void SanityCheck()
    {
        foreach (Tool tool in Tools)
        {
            Debug.Assert(tool.TravellingTime.GetLength(0) == NbWorkLocations);
            Debug.Assert(tool.TravellingTime.GetLength(1) == NbWorkLocations);
            for (int i = 0; i < NbWorkLocations; i++)
                Debug.Assert(tool.TravellingTime[i, i] == 0);
        }
    }
}

interface DataReader
{
    FactoryDescription FetchData();
}

public class SmallSyntheticData : DataReader
{
    public SmallSyntheticData()
    {
    }

    public FactoryDescription FetchData()
    {
        // deterministic seed for result reproducibility
        Random randomDuration = new Random(2);

        // FactoryDescription(nbTools, nblocations, nbTasks per cycle)
        FactoryDescription factoryDescription = new FactoryDescription(5, 4, 3);

        // Travelling time and distance are temporarily identical and they
        // are no different for different tools
        int[,] travellingTime = new int[factoryDescription.NbWorkLocations, factoryDescription.NbWorkLocations];
        for (int i = 0; i < travellingTime.GetLength(0); i++)
        {
            for (int j = 0; j < travellingTime.GetLength(1); j++)
            {
                if (i == j)
                    travellingTime[i, j] = 0;
                else
                    travellingTime[i, j] = (5 * Math.Abs(i - j)) * 10;
            }
        }

        factoryDescription.Tools[0].AddTaskType(0);
        factoryDescription.Tools[1].AddTaskType(0);
        factoryDescription.Tools[2].AddTaskType(1);
        factoryDescription.Tools[3].AddTaskType(1);
        factoryDescription.Tools[4].AddTaskType(2);
        factoryDescription.Tools[1].AddTaskType(1);

        foreach (Tool tool in factoryDescription.Tools)
            tool.TravellingTime = travellingTime;

        int c = 0;
        int nbCyclePerWorkLocation = 2;
        int[] boll = new int[100];
        for (int i = 0; i < factoryDescription.NbWorkLocations; i++)
        {
            factoryDescription.Locations[i].NbTasks = nbCyclePerWorkLocation * factoryDescription.NbTaskPerCycle;
            for (int j = 0; j < nbCyclePerWorkLocation; j++)
            {
                for (int k = 0; k < factoryDescription.NbTaskPerCycle; k++)
                {
                    Task t = new Task(c, k, i, k + j * factoryDescription.NbTaskPerCycle);

                    // Filling in tool-dependent durations
                    Tool[] compatibleTools = factoryDescription.getToolPerTaskType(k);
                    foreach (Tool tool in compatibleTools)
                    {
                        boll[c] = randomDuration.Next(13, 17) * 10;
                        ;
                        t.Durations[tool.Id] = boll[c];
                    }
                    factoryDescription.Locations[i].Tasks[t.TaskPosition] = t;
                    c++;
                }
            }
        }

        factoryDescription.SanityCheck();
        return factoryDescription;
    }
}

public class RandomSelectToolHeuristic : NetDecisionBuilder
{
    private FactoryScheduling factoryScheduling;
    private Random rnd;

    public RandomSelectToolHeuristic(FactoryScheduling factoryScheduling, int seed)
    {
        this.factoryScheduling = factoryScheduling;
        // deterministic seed for result reproducibility
        this.rnd = new Random(seed);
    }

    public override Decision Next(Solver solver)
    {
        foreach (IntVar var in factoryScheduling.SelectedTool)
        {
            if (!var.Bound())
            {
                int min = (int)var.Min();
                int max = (int)var.Max();
                int rndVal = rnd.Next(min, max + 1);
                while (!var.Contains(rndVal))
                    rndVal = rnd.Next(min, max + 1);
                return solver.MakeAssignVariableValue(var, rndVal);
            }
        }
        return null;
    }
}

class TaskAlternative
{
    public Task Task { get; private set; }
    public IntVar ToolVar { get; set; }
    public List<IntervalVar> Intervals { get; private set; }

    public TaskAlternative(Task t)
    {
        Task = t;
        Intervals = new List<IntervalVar>();
    }
}

public class FactoryScheduling
{
    private FactoryDescription factoryData;
    private Solver solver;

    private Task[] tasks;
    private int[] taskTypes;

    /* Flat list of all the tasks */
    private TaskAlternative[] taskStructures;

    /* Task per WorkLocation: location2Task[d][i]: the i-th task of the
     * d-th location */
    private TaskAlternative[][] location2Task;

    /* Task per Tool: tool2Task[t][i]: the i-th task of the t-th tool.
       Note that it does NOT imply that the it will be the i-th
       executed. In other words, it should be considered as an unordered
       set.  Furthermore, tool2Task[t][i] can also be *unperformed* */
    private List<IntervalVar>[] tool2Task;

    /* All the transition times for the tools.
       tool2TransitionTimes[t][i]: the transition time of the t-th tool
       from the i-th task to the next */
    private List<IntVar>[] tool2TransitionTimes;

    /* Map between the interval var of a tool to its related task id.
       toolIntervalVar2TaskId[t][k] = i: in the t-th tool, the k-th
       interval var correspond to tasks[i] */
    private List<int>[] toolIntervalVar2TaskId;

    /* Tools per task type: taskType2Tool[tt][t]: the t-th tool capable
     * of doing the tt-th task type */
    private List<Tool>[] taskType2Tool;

    /* For each task which tools is performed upon */
    private List<IntVar> selectedTool;
    public List<IntVar> SelectedTool
    {
        get {
            return selectedTool;
        }
    }

    /* Sequence of task for each tool */
    private SequenceVar[] allToolSequences;
    public SequenceVar[] AllToolSequences
    {
        get {
            return allToolSequences;
        }
    }

    /* Makespan var */
    private IntVar makespan;

    /* Objective */
    private OptimizeVar objective;

    /* maximum horizon */
    private int horizon;

    /* Start & End times of IntervalVars*/
    IntVar[][] startingTimes;
    IntVar[][] endTimes;

    public FactoryScheduling(FactoryDescription data)
    {
        factoryData = data;
    }

    private void Init()
    {
        horizon = factoryData.Horizon;
        solver = new Solver("Factory Scheduling");
        tasks = factoryData.getFlatTaskList();
        taskTypes = factoryData.getTaskTypes();
        taskStructures = new TaskAlternative[tasks.Length];
        location2Task = new TaskAlternative[factoryData.NbWorkLocations][];
        tool2Task = new List<IntervalVar>[factoryData.NbTools];
        toolIntervalVar2TaskId = new List<int>[factoryData.NbTools];
        tool2TransitionTimes = new List<IntVar>[factoryData.NbTools];

        taskType2Tool = new List<Tool>[taskTypes.Length];
        selectedTool = new List<IntVar>();
        for (int tt = 0; tt < taskTypes.Length; tt++)
            taskType2Tool[tt] = new List<Tool>();

        foreach (Tool tool in factoryData.Tools)
            foreach (int taskType in tool.TaskTypes)
                taskType2Tool[taskType].Add(tool);
        for (int d = 0; d < factoryData.NbWorkLocations; d++)
            location2Task[d] = new TaskAlternative[factoryData.Locations[d].NbTasks];
        for (int t = 0; t < factoryData.NbTools; t++)
        {
            tool2Task[t] = new List<IntervalVar>();
            toolIntervalVar2TaskId[t] = new List<int>();
            tool2TransitionTimes[t] = new List<IntVar>();
        }

        allToolSequences = new SequenceVar[factoryData.NbTools - 1];

        startingTimes = new IntVar[factoryData.NbTools - 1][];
        endTimes = new IntVar[factoryData.NbTools - 1][];
    }

    private void PostTransitionTimeConstraints(int t, bool postTransitionsConstraint = true)
    {
        Tool tool = factoryData.Tools[t];
        // if it is a inspection, we make sure there are no transitiontimes
        if (tool.CanPerformTaskType(factoryData.Inspection))
            tool2TransitionTimes[t].Add(null);
        else
        {
            int[,] tt = tool.TravellingTime;

            SequenceVar seq = allToolSequences[t];
            long s = seq.Size();
            IntVar[] nextLocation = new IntVar[s + 1];

            // The seq.Next(i) represents the task performed after the i-th
            // task in the sequence seq.Next(0) represents the first task
            // performed for extracting travelling times we need to get the
            // related location In case a task is not performed (seq.Next(i)
            // == i), i.e. it's pointing to itself The last performed task
            // (or pre-start task, if no tasks are performed) will have
            // seq.Next(i) == s + 1 therefore we add a virtual location
            // whose travelling time is equal to 0
            //
            // NOTE: The index of a SequenceVar are 0..n, but the domain
            // range is 1..(n+1), this is due to that the start node = 0 is
            // a dummy node, and the node where seq.Next(i) == n+1 is the
            // end node

            // Extra elements for the unreachable start node (0), and the
            // end node whose next task takes place in a virtual location
            int[] taskIndex2locationId = new int[s + 2];
            taskIndex2locationId[0] = -10;
            for (int i = 0; i < s; i++)
                taskIndex2locationId[i + 1] = tasks[toolIntervalVar2TaskId[t][i]].LocationId;

            // this is the virtual location for unperformed tasks
            taskIndex2locationId[s + 1] = factoryData.NbWorkLocations;

            // Build the travelling time matrix with the additional virtual location
            int[][] ttWithVirtualLocation = new int [factoryData.NbWorkLocations + 1][];
            for (int d1 = 0; d1 < ttWithVirtualLocation.Length; d1++)
            {
                ttWithVirtualLocation[d1] = new int[factoryData.NbWorkLocations + 1];
                for (int d2 = 0; d2 < ttWithVirtualLocation.Length; d2++)
                    if (d1 == factoryData.NbWorkLocations)
                    {
                        ttWithVirtualLocation[d1][d2] = 0;
                    }
                    else
                    {
                        ttWithVirtualLocation[d1][d2] = (d2 == factoryData.NbWorkLocations) ? 0 : tt[d1, d2];
                    }
            }

            for (int i = 0; i < nextLocation.Length; i++)
            {
                // this is the next-location associated with the i-th task
                nextLocation[i] = solver.MakeElement(taskIndex2locationId, seq.Next(i)).Var();

                int d = (i == 0) ? tool.InitialLocationId : tasks[toolIntervalVar2TaskId[t][i - 1]].LocationId;
                if (i == 0)
                {
                    // To be changed - right now we don't have meaningful indata
                    // of previous location Ugly way of setting initial travel
                    // time to = 0, as this is how we find common grounds
                    // between benchmark algorithm and this
                    tool2TransitionTimes[t].Add(
                        solver.MakeElement(new int[ttWithVirtualLocation[d].Length], nextLocation[i]).Var());
                }
                else
                {
                    tool2TransitionTimes[t].Add(solver.MakeElement(ttWithVirtualLocation[d], nextLocation[i]).Var());
                }
            }

            // Extra elements for the unreachable start node (0), and the
            // end node whose next task takes place in a virtual location
            startingTimes[t] = new IntVar[s + 2];
            endTimes[t] = new IntVar[s + 2];

            startingTimes[t][0] = solver.MakeIntConst(0);
            // Tbd: Set this endtime to the estimated time of finishing
            // previous task for the current tool
            endTimes[t][0] = solver.MakeIntConst(0);

            for (int i = 0; i < s; i++)
            {
                startingTimes[t][i + 1] = tool2Task[t][i].SafeStartExpr(-1).Var();
                endTimes[t][i + 1] = tool2Task[t][i].SafeEndExpr(-1).Var();
            }
            startingTimes[t][s + 1] = solver.MakeIntConst(factoryData.Horizon);
            endTimes[t][s + 1] = solver.MakeIntConst(factoryData.Horizon);

            // Enforce (or not) that each task is separated by the
            // transition time to the next task
            for (int i = 0; i < nextLocation.Length; i++)
            {
                IntVar nextStart = solver.MakeElement(startingTimes[t], seq.Next(i).Var()).Var();
                if (postTransitionsConstraint)
                    solver.Add(endTimes[t][i] + tool2TransitionTimes[t][i] <= nextStart);
            }
        }
    }

    private void Model()
    {
        /* Building basic task data structures */
        for (int i = 0; i < tasks.Length; i++)
        {
            /* Create a new set of possible IntervalVars & IntVar to decide
             * which one (and only 1) is performed */
            taskStructures[i] = new TaskAlternative(tasks[i]);

            /* Container to use when posting constraints */
            location2Task[tasks[i].LocationId][tasks[i].TaskPosition] = taskStructures[i];

            /* Get task type */
            int taskType = tasks[i].TaskType;

            /* Possible tool for this task */
            List<Tool> tools = taskType2Tool[taskType];
            bool optional = tools.Count > 1;

            /* List of boolean variables. If performedOnTool[t] == true then
             * the task is performed on tool t */
            List<IntVar> performedOnTool = new List<IntVar>();
            for (int t = 0; t < tools.Count; t++)
            {
                /* Creating an IntervalVar. If tools.Count > 1 the intervalVar
                 * is *OPTIONAL* */
                int toolId = tools[t].Id;
                Debug.Assert(tasks[i].Durations.ContainsKey(toolId));
                int duration = tasks[i].Durations[toolId];
                string name = "J " + tasks[i].Id + " [" + toolId + "]";

                IntervalVar intervalVar;
                if (taskType == factoryData.Inspection)
                {
                    /* We set a 0 time if the task is an inspection */
                    duration = 0;
                    intervalVar = solver.MakeFixedDurationIntervalVar(0, horizon, duration, optional, name);
                    IntVar start = intervalVar.SafeStartExpr(-1).Var();

                    intervalVar.SafeStartExpr(-1).Var().SetValues(factoryData.InspectionStarts);
                }
                else
                {
                    intervalVar = solver.MakeFixedDurationIntervalVar(0, horizon, duration, optional, name);
                }

                taskStructures[i].Intervals.Add(intervalVar);
                tool2Task[toolId].Add(intervalVar);
                toolIntervalVar2TaskId[toolId].Add(i);

                /* Collecting all the bool vars, even if they are optional */
                performedOnTool.Add(intervalVar.PerformedExpr().Var());
            }

            /* Linking the bool var to a single integer variable: */
            /* if alternativeToolVar == t <=> performedOnTool[t] == true */
            string alternativeName = "J " + tasks[i].Id;
            IntVar alternativeToolVar = solver.MakeIntVar(0, tools.Count - 1, alternativeName);
            taskStructures[i].ToolVar = alternativeToolVar;

            solver.Add(solver.MakeMapDomain(alternativeToolVar, performedOnTool.ToArray()));
            Debug.Assert(performedOnTool.ToArray().Length == alternativeToolVar.Max() + 1);

            selectedTool.Add(alternativeToolVar);
        }

        /* Creates precedences on a work Location in order to enforce a
         * fully ordered set within the same location
         */
        for (int d = 0; d < location2Task.Length; d++)
        {
            for (int i = 0; i < location2Task[d].Length - 1; i++)
            {
                TaskAlternative task1 = location2Task[d][i];
                TaskAlternative task2 = location2Task[d][i + 1];
                /* task1 must end before task2 starts */
                /* Adding precedence for each possible alternative pair */
                for (int t1 = 0; t1 < task1.Intervals.Count(); t1++)
                {
                    IntervalVar task1Alternative = task1.Intervals[t1];
                    for (int t2 = 0; t2 < task2.Intervals.Count(); t2++)
                    {
                        IntervalVar task2Alternative = task2.Intervals[t2];
                        Constraint precedence =
                            solver.MakeIntervalVarRelation(task2Alternative, Solver.STARTS_AFTER_END, task1Alternative);
                        solver.Add(precedence);
                    }
                }
            }
        }

        /* Adds disjunctive constraints on unary resources, and creates
         * sequence variables. */
        for (int t = 0; t < factoryData.NbTools; t++)
        {
            string name = "Tool " + t;

            if (!factoryData.Tools[t].CanPerformTaskType(factoryData.Inspection))
            {
                DisjunctiveConstraint ct = solver.MakeDisjunctiveConstraint(tool2Task[t].ToArray(), name);
                solver.Add(ct);
                allToolSequences[t] = ct.SequenceVar();
            }
            PostTransitionTimeConstraints(t, true);
        }

        /* Collecting all tasks end for makespan objective function */
        List<IntVar> intervalEnds = new List<IntVar>();
        for (int i = 0; i < tasks.Length; i++)
            foreach (IntervalVar var in taskStructures[i].Intervals)
                intervalEnds.Add(var.SafeEndExpr(-1).Var());

        /* Objective: minimize the makespan (maximum end times of all tasks) */
        makespan = solver.MakeMax(intervalEnds.ToArray()).Var();
        objective = solver.MakeMinimize(makespan, 1);
    }

    private void Search()
    {
        int seed = 2; // This is a good seed to show the crash

        /* Assigning first tools */
        DecisionBuilder myToolAssignmentPhase = new RandomSelectToolHeuristic(this, seed);

        /* Ranking of the tools */
        DecisionBuilder sequencingPhase = solver.MakePhase(allToolSequences, Solver.SEQUENCE_DEFAULT);

        /* Then fixing time of tasks as early as possible */
        DecisionBuilder timingPhase = solver.MakePhase(makespan, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

        /* Overall phase */
        DecisionBuilder mainPhase = solver.Compose(myToolAssignmentPhase, sequencingPhase, timingPhase);

        /* Logging */
        const int logFrequency = 1000000;
        SearchMonitor searchLog = solver.MakeSearchLog(logFrequency, objective);

        /* Restarts */
        SearchMonitor searchRestart = solver.MakeLubyRestart(100);

        /* Search Limit in ms */
        SearchLimit limit = solver.MakeTimeLimit(180 * 1000);

        /* Collecting best solution */
        SolutionCollector collector = solver.MakeLastSolutionCollector();
        collector.AddObjective(makespan);

        // collector.Add( pile.ToArray() );
        solver.NewSearch(mainPhase, searchLog, searchRestart, objective, limit);
        while (solver.NextSolution())
        {
            Console.WriteLine("MAKESPAN: " + makespan.Value());
        }
    }

    public void Solve()
    {
        Init();
        Model();
        Search();
    }
}

public class Issue33Test
{
    public static void FactorySchedulingTest()
    {
        FactoryScheduling scheduling = new FactoryScheduling(new SmallSyntheticData().FetchData());
        scheduling.Solve();
    }
    static void Main()
    {
        FactorySchedulingTest();
    }
}
