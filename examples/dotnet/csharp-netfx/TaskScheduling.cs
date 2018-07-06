using System;
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;

namespace OrToolsConstraint {
class Job {
  public Job(List<Task> tasks) {
    AlternativeTasks = tasks;
  }
  public Job Successor { get; set; }
  public List<Task> AlternativeTasks { get; set; }
}

class Task {
  public Task(string name, long duration, long equipment) {
    Name = name;
    Duration = duration;
    Equipment = equipment;
  }

  public string Name {get; set;}
  public long StartTime {get; set;}
  public long EndTime {
    get {
      return StartTime + Duration;
    }
  }
  public long Duration {get; set;}
  public long Equipment { get; set; }

  public override string ToString() {
    return Name + " [ " + Equipment + " ]\tstarts: " + StartTime + " ends:" +
        EndTime + ", duration: " + Duration;
  }
}

class Prefix : VoidToString
{
  public override string Run()
  {
    return "[TaskScheduling] ";
  }
}

class TaskScheduling {
  public static List<Job> myJobList = new List<Job>();
  public static Dictionary<long, List<IntervalVar>> tasksToEquipment =
      new Dictionary<long, List<IntervalVar>>();
  public static Dictionary<string, long> taskIndexes =
      new Dictionary<string, long>();

  public static void InitTaskList() {
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

  public static int GetTaskCount() {
    int c = 0;
    foreach (Job j in myJobList)
      foreach (Task t in j.AlternativeTasks) {
        taskIndexes[t.Name] = c;
        c++;
      }

    return c;
  }

  public static int GetEndTaskCount() {
    int c = 0;
    foreach (Job j in myJobList)
      if (j.Successor == null)
        c += j.AlternativeTasks.Count;
    return c;
  }

  static void Main(string[] args) {
    InitTaskList();
    int taskCount = GetTaskCount();

    Solver solver = new Solver("ResourceConstraintScheduling");

    IntervalVar[] tasks = new IntervalVar[taskCount];
    IntVar[] taskChoosed = new IntVar[taskCount];
    IntVar[] makeSpan = new IntVar[GetEndTaskCount()];

    int endJobCounter = 0;
    foreach (Job j in myJobList) {
      IntVar[] tmp = new IntVar[j.AlternativeTasks.Count];
      int i = 0;
      foreach (Task t in j.AlternativeTasks) {
        long ti = taskIndexes[t.Name];
        taskChoosed[ti] = solver.MakeIntVar(0, 1, t.Name + "_choose");
        tmp[i++] = taskChoosed[ti];
        tasks[ti] = solver.MakeFixedDurationIntervalVar(
            0, 100000, t.Duration, false, t.Name + "_interval");
        if (j.Successor == null)
          makeSpan[endJobCounter++] = tasks[ti].EndExpr().Var();
        if (!tasksToEquipment.ContainsKey(t.Equipment))
          tasksToEquipment[t.Equipment] = new List<IntervalVar>();
        tasksToEquipment[t.Equipment].Add(tasks[ti]);
      }
      solver.Add(IntVarArrayHelper.Sum(tmp) == 1);
    }

    List<SequenceVar> all_seq = new List<SequenceVar>();
    foreach (KeyValuePair<long, List<IntervalVar>> pair in tasksToEquipment) {
      DisjunctiveConstraint dc = solver.MakeDisjunctiveConstraint(
          pair.Value.ToArray(), pair.Key.ToString());
      solver.Add(dc);
      all_seq.Add(dc.SequenceVar());
    }

    IntVar objective_var = solver.MakeMax(makeSpan).Var();
    OptimizeVar objective_monitor = solver.MakeMinimize(objective_var, 1);

    DecisionBuilder sequence_phase =
        solver.MakePhase(all_seq.ToArray(), Solver.SEQUENCE_DEFAULT);
    DecisionBuilder objective_phase =
        solver.MakePhase(objective_var, Solver.CHOOSE_FIRST_UNBOUND,
                         Solver.ASSIGN_MIN_VALUE);
    DecisionBuilder main_phase = solver.Compose(sequence_phase, objective_phase);

    const int kLogFrequency = 1000000;
    VoidToString prefix = new Prefix();
    SearchMonitor search_log =
        solver.MakeSearchLog(kLogFrequency, objective_monitor, prefix);

    SolutionCollector collector = solver.MakeLastSolutionCollector();
    collector.Add(all_seq.ToArray());
    collector.AddObjective(objective_var);

    if (solver.Solve(main_phase, search_log, objective_monitor, null, collector))
      Console.Out.WriteLine("Optimal solution = " + collector.ObjectiveValue(0));
    else
      Console.Out.WriteLine("No solution.");
  }
}
}
