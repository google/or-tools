using System;
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.ConstraintSolver;


class Task {
  public Task(int taskId, int jobId, int duration, int machine) {
    TaskId = taskId;
    JobId = jobId;
    Duration = duration;
    Machine = machine;
    Name = "T" + taskId + "J" + jobId + "M" +
          machine + "D" + duration;
  }
  public int TaskId {get; set;}
  public int JobId {get; set;}
  public int Machine {get; set;}
  public int Duration {get; set;}
  public string Name {get;}
}



class FlexibleJobshop
{
  //Number of machines.
  public const int machinesCount = 3;
  //horizon is the upper bound of the start time of all tasks.
  public const int horizon = 300;
  //this will be set to the size of myJobList variable.
  public static int jobsCount;
  /*Search time limit in milliseconds. if it's equal to 0,
  then no time limit will be used.*/
  public const int timeLimitInMs = 0;
  public static List<List<Task>> myJobList = new List<List<Task>>();
  public static void InitTaskList() {
    List<Task> taskList = new List<Task>();
    taskList.Add(new Task(0, 0, 65, 0));
    taskList.Add(new Task(1 ,0, 5, 1));
    taskList.Add(new Task(2 ,0, 15, 2));
    myJobList.Add(taskList);

    taskList = new List<Task>();
    taskList.Add(new Task(0, 1, 15, 0));
    taskList.Add(new Task(1 ,1, 25, 1));
    taskList.Add(new Task(2 ,1, 10, 2));
    myJobList.Add(taskList);

    taskList = new List<Task>();
    taskList.Add(new Task(0 ,2, 25, 0));
    taskList.Add(new Task(1 ,2, 30, 1));
    taskList.Add(new Task(2 ,2, 40, 2));
    myJobList.Add(taskList);

    taskList = new List<Task>();
    taskList.Add(new Task(0, 3, 20, 0));
    taskList.Add(new Task(1 ,3, 35, 1));
    taskList.Add(new Task(2 ,3, 10, 2));
    myJobList.Add(taskList);

    taskList = new List<Task>();
    taskList.Add(new Task(0, 4, 15, 0));
    taskList.Add(new Task(1 ,4, 25, 1));
    taskList.Add(new Task(2 ,4, 10, 2));
    myJobList.Add(taskList);

    taskList = new List<Task>();
    taskList.Add(new Task(0, 5, 25, 0));
    taskList.Add(new Task(1 ,5, 30, 1));
    taskList.Add(new Task(2 ,5, 40, 2));
    myJobList.Add(taskList);

    taskList = new List<Task>();
    taskList.Add(new Task(0, 6, 20, 0));
    taskList.Add(new Task(1 ,6, 35, 1));
    taskList.Add(new Task(2 ,6, 10, 2));
    myJobList.Add(taskList);

    taskList = new List<Task>();
    taskList.Add(new Task(0, 7, 10, 0));
    taskList.Add(new Task(1 ,7, 15, 1));
    taskList.Add(new Task(2 ,7, 50, 2));
    myJobList.Add(taskList);

    taskList = new List<Task>();
    taskList.Add(new Task(0, 8, 50, 0));
    taskList.Add(new Task(1 ,8, 10, 1));
    taskList.Add(new Task(2 ,8, 20, 2));
    myJobList.Add(taskList);

    jobsCount = myJobList.Count;
  }

  public static void Main(String[] args)
  {
    InitTaskList();
    Solver solver = new Solver("Jobshop");

    // ----- Creates all Intervals and vars -----

    // All tasks
    List<IntervalVar> allTasks = new List<IntervalVar>();
    // Stores all tasks attached interval variables per job.
    List<List<IntervalVar>>
      jobsToTasks = new List<List<IntervalVar>>(jobsCount);
    // machinesToTasks stores the same interval variables as above, but
    // grouped my machines instead of grouped by jobs.
    List<List<IntervalVar>>
      machinesToTasks = new List<List<IntervalVar>>(machinesCount);
    for (int i=0; i<machinesCount; i++) {
      machinesToTasks.Add(new List<IntervalVar>());
    }

    // Creates all individual interval variables.
    foreach (List<Task> job in myJobList) {
      jobsToTasks.Add(new List<IntervalVar>());
      foreach (Task task in job) {
        IntervalVar oneTask = solver.MakeFixedDurationIntervalVar(
            0, horizon, task.Duration, false, task.Name);
        jobsToTasks[task.JobId].Add(oneTask);
        allTasks.Add(oneTask);
        machinesToTasks[task.Machine].Add(oneTask);
      }
    }

    // ----- Creates model -----

    // Creates precedences inside jobs.
    foreach (List<IntervalVar> jobToTask in jobsToTasks) {
      int tasksCount = jobToTask.Count;
      for (int task_index = 0; task_index < tasksCount - 1; ++task_index) {
        IntervalVar t1 = jobToTask[task_index];
        IntervalVar t2 = jobToTask[task_index + 1];
        Constraint prec =
            solver.MakeIntervalVarRelation(t2, Solver.STARTS_AFTER_END, t1);
        solver.Add(prec);
      }
    }

    // Adds disjunctive constraints on unary resources, and creates
    // sequence variables. A sequence variable is a dedicated variable
    // whose job is to sequence interval variables.
    SequenceVar[] allSequences = new SequenceVar[machinesCount];
    for (int machineId = 0; machineId < machinesCount; ++machineId) {
      string name = "Machine_" + machineId;
      DisjunctiveConstraint ct =
          solver.MakeDisjunctiveConstraint(machinesToTasks[machineId].ToArray(),
                                          name);
      solver.Add(ct);
      allSequences[machineId] = ct.SequenceVar();
    }
    // Creates array of end_times of jobs.
    IntVar[] allEnds = new IntVar[jobsCount];
    for (int i=0; i<jobsCount; i++) {
      IntervalVar task = jobsToTasks[i].Last();
      allEnds[i] = task.EndExpr().Var();
    }

    // Objective: minimize the makespan (maximum end times of all tasks)
    // of the problem.
    IntVar objectiveVar = solver.MakeMax(allEnds).Var();
    OptimizeVar objectiveMonitor = solver.MakeMinimize(objectiveVar, 1);

    // ----- Search monitors and decision builder -----

    // This decision builder will rank all tasks on all machines.
    DecisionBuilder sequencePhase =
        solver.MakePhase(allSequences, Solver.SEQUENCE_DEFAULT);

    // After the ranking of tasks, the schedule is still loose and any
    // task can be postponed at will. But, because the problem is now a PERT
    // (http://en.wikipedia.org/wiki/Program_Evaluation_and_Review_Technique),
    // we can schedule each task at its earliest start time. This iscs
    // conveniently done by fixing the objective variable to its
    // minimum value.
    DecisionBuilder objPhase = solver.MakePhase(
        objectiveVar, Solver.CHOOSE_FIRST_UNBOUND, Solver.ASSIGN_MIN_VALUE);

    // The main decision builder (ranks all tasks, then fixes the
    // objectiveVariable).
    DecisionBuilder mainPhase = solver.Compose(sequencePhase, objPhase);

    // Search log.
    const int kLogFrequency = 1000000;
    SearchMonitor searchLog =
        solver.MakeSearchLog(kLogFrequency, objectiveMonitor);

    SearchLimit limit = null;
    if (timeLimitInMs > 0) {
      limit = solver.MakeTimeLimit(timeLimitInMs);
    }


    SolutionCollector collector = solver.MakeLastSolutionCollector();
    collector.Add(allSequences);
    collector.Add(allTasks.ToArray());
    // Search.
    bool solutionFound = solver.Solve(mainPhase, searchLog, objectiveMonitor,
                                      limit, collector);
    if(solutionFound) {
      //The index of the solution from the collector
      const int SOLUTION_INDEX = 0;
      Assignment solution = collector.Solution(SOLUTION_INDEX);
      for (int m = 0; m < machinesCount; ++m) {
        Console.WriteLine("Machine " + m + " :");
        SequenceVar seq = allSequences[m];
        int[] storedSequence = collector.ForwardSequence(SOLUTION_INDEX, seq);
        foreach (int taskIndex in storedSequence) {
          IntervalVar task = seq.Interval(taskIndex);
          long startMin = solution.StartMin(task);
          long startMax = solution.StartMax(task);
          if(startMin == startMax) {
            Console.WriteLine("Task " + task.Name() + " starts at " +
                            startMin + ".");
          }
          else {
            Console.WriteLine("Task " + task.Name() + " starts between " +
                            startMin + " and " + startMax + ".");
          }

        }
      }
    }
    else {
      Console.WriteLine("No solution found!");
    }
  }
}
