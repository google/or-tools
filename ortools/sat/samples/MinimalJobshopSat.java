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
package com.google.ortools.sat.samples;
// [START import]
import static java.lang.Math.max;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.IntervalVar;
import com.google.ortools.sat.LinearExpr;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.IntStream;
// [END import]

/** Minimal Jobshop problem. */
public class MinimalJobshopSat {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // [START data]
    class Task {
      int machine;
      int duration;
      Task(int machine, int duration) {
        this.machine = machine;
        this.duration = duration;
      }
    }

    final List<List<Task>> allJobs =
        Arrays.asList(Arrays.asList(new Task(0, 3), new Task(1, 2), new Task(2, 2)), // Job0
            Arrays.asList(new Task(0, 2), new Task(2, 1), new Task(1, 4)), // Job1
            Arrays.asList(new Task(1, 4), new Task(2, 3)) // Job2
        );

    int numMachines = 1;
    for (List<Task> job : allJobs) {
      for (Task task : job) {
        numMachines = max(numMachines, 1 + task.machine);
      }
    }
    final int[] allMachines = IntStream.range(0, numMachines).toArray();

    // Computes horizon dynamically as the sum of all durations.
    int horizon = 0;
    for (List<Task> job : allJobs) {
      for (Task task : job) {
        horizon += task.duration;
      }
    }
    // [END data]

    // Creates the model.
    // [START model]
    CpModel model = new CpModel();
    // [END model]

    // [START variables]
    class TaskType {
      IntVar start;
      IntVar end;
      IntervalVar interval;
    }
    Map<List<Integer>, TaskType> allTasks = new HashMap<>();
    Map<Integer, List<IntervalVar>> machineToIntervals = new HashMap<>();

    for (int jobID = 0; jobID < allJobs.size(); ++jobID) {
      List<Task> job = allJobs.get(jobID);
      for (int taskID = 0; taskID < job.size(); ++taskID) {
        Task task = job.get(taskID);
        String suffix = "_" + jobID + "_" + taskID;

        TaskType taskType = new TaskType();
        taskType.start = model.newIntVar(0, horizon, "start" + suffix);
        taskType.end = model.newIntVar(0, horizon, "end" + suffix);
        taskType.interval = model.newIntervalVar(
            taskType.start, LinearExpr.constant(task.duration), taskType.end, "interval" + suffix);

        List<Integer> key = Arrays.asList(jobID, taskID);
        allTasks.put(key, taskType);
        machineToIntervals.computeIfAbsent(task.machine, (Integer k) -> new ArrayList<>());
        machineToIntervals.get(task.machine).add(taskType.interval);
      }
    }
    // [END variables]

    // [START constraints]
    // Create and add disjunctive constraints.
    for (int machine : allMachines) {
      List<IntervalVar> list = machineToIntervals.get(machine);
      model.addNoOverlap(list.toArray(new IntervalVar[0]));
    }

    // Precedences inside a job.
    for (int jobID = 0; jobID < allJobs.size(); ++jobID) {
      List<Task> job = allJobs.get(jobID);
      for (int taskID = 0; taskID < job.size() - 1; ++taskID) {
        List<Integer> prevKey = Arrays.asList(jobID, taskID);
        List<Integer> nextKey = Arrays.asList(jobID, taskID + 1);
        model.addGreaterOrEqual(allTasks.get(nextKey).start, allTasks.get(prevKey).end);
      }
    }
    // [END constraints]

    // [START objective]
    // Makespan objective.
    IntVar objVar = model.newIntVar(0, horizon, "makespan");
    List<IntVar> ends = new ArrayList<>();
    for (int jobID = 0; jobID < allJobs.size(); ++jobID) {
      List<Task> job = allJobs.get(jobID);
      List<Integer> key = Arrays.asList(jobID, job.size() - 1);
      ends.add(allTasks.get(key).end);
    }
    model.addMaxEquality(objVar, ends.toArray(new IntVar[0]));
    model.minimize(objVar);
    // [END objective]

    // Creates a solver and solves the model.
    // [START solve]
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);
    // [END solve]

    // [START print_solution]
    if (status == CpSolverStatus.OPTIMAL || status == CpSolverStatus.FEASIBLE) {
      class AssignedTask {
        int jobID;
        int taskID;
        int start;
        int duration;
        // Ctor
        AssignedTask(int jobID, int taskID, int start, int duration) {
          this.jobID = jobID;
          this.taskID = taskID;
          this.start = start;
          this.duration = duration;
        }
      }
      class SortTasks implements Comparator<AssignedTask> {
        @Override
        public int compare(AssignedTask a, AssignedTask b) {
          if (a.start != b.start) {
            return a.start - b.start;
          } else {
            return a.duration - b.duration;
          }
        }
      }
      System.out.println("Solution:");
      // Create one list of assigned tasks per machine.
      Map<Integer, List<AssignedTask>> assignedJobs = new HashMap<>();
      for (int jobID = 0; jobID < allJobs.size(); ++jobID) {
        List<Task> job = allJobs.get(jobID);
        for (int taskID = 0; taskID < job.size(); ++taskID) {
          Task task = job.get(taskID);
          List<Integer> key = Arrays.asList(jobID, taskID);
          AssignedTask assignedTask = new AssignedTask(
              jobID, taskID, (int) solver.value(allTasks.get(key).start), task.duration);
          assignedJobs.computeIfAbsent(task.machine, (Integer k) -> new ArrayList<>());
          assignedJobs.get(task.machine).add(assignedTask);
        }
      }

      // Create per machine output lines.
      String output = "";
      for (int machine : allMachines) {
        // Sort by starting time.
        Collections.sort(assignedJobs.get(machine), new SortTasks());
        String solLineTasks = "Machine " + machine + ": ";
        String solLine = "           ";

        for (AssignedTask assignedTask : assignedJobs.get(machine)) {
          String name = "job_" + assignedTask.jobID + "_task_" + assignedTask.taskID;
          // Add spaces to output to align columns.
          solLineTasks += String.format("%-15s", name);

          String solTmp =
              "[" + assignedTask.start + "," + (assignedTask.start + assignedTask.duration) + "]";
          // Add spaces to output to align columns.
          solLine += String.format("%-15s", solTmp);
        }
        output += solLineTasks + "%n";
        output += solLine + "%n";
      }
      System.out.printf("Optimal Schedule Length: %f%n", solver.objectiveValue());
      System.out.printf(output);
    } else {
      System.out.println("No solution found.");
    }
    // [END print_solution]

    // Statistics.
    // [START statistics]
    System.out.println("Statistics");
    System.out.printf("  conflicts: %d%n", solver.numConflicts());
    System.out.printf("  branches : %d%n", solver.numBranches());
    System.out.printf("  wall time: %f s%n", solver.wallTime());
    // [END statistics]
  }

  private MinimalJobshopSat() {}
}
// [END program]
