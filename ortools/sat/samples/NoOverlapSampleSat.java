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

package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.IntervalVar;
import com.google.ortools.sat.LinearExpr;

/**
 * We want to schedule 3 tasks on 3 weeks excluding weekends, making the final day as early as
 * possible.
 */
public class NoOverlapSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    CpModel model = new CpModel();
    // Three weeks.
    int horizon = 21;

    // Task 0, duration 2.
    IntVar start0 = model.newIntVar(0, horizon, "start0");
    int duration0 = 2;
    IntVar end0 = model.newIntVar(0, horizon, "end0");
    IntervalVar task0 = model.newIntervalVar(start0, LinearExpr.constant(duration0), end0, "task0");

    //  Task 1, duration 4.
    IntVar start1 = model.newIntVar(0, horizon, "start1");
    int duration1 = 4;
    IntVar end1 = model.newIntVar(0, horizon, "end1");
    IntervalVar task1 = model.newIntervalVar(start1, LinearExpr.constant(duration1), end1, "task1");

    // Task 2, duration 3.
    IntVar start2 = model.newIntVar(0, horizon, "start2");
    int duration2 = 3;
    IntVar end2 = model.newIntVar(0, horizon, "end2");
    IntervalVar task2 = model.newIntervalVar(start2, LinearExpr.constant(duration2), end2, "task2");

    // Weekends.
    IntervalVar weekend0 = model.newFixedInterval(5, 2, "weekend0");
    IntervalVar weekend1 = model.newFixedInterval(12, 2, "weekend1");
    IntervalVar weekend2 = model.newFixedInterval(19, 2, "weekend2");

    // No Overlap constraint. This constraint enforces that no two intervals can overlap.
    // In this example, as we use 3 fixed intervals that span over weekends, this constraint makes
    // sure that all tasks are executed on weekdays.
    model.addNoOverlap(new IntervalVar[] {task0, task1, task2, weekend0, weekend1, weekend2});

    // Makespan objective.
    IntVar obj = model.newIntVar(0, horizon, "makespan");
    model.addMaxEquality(obj, new IntVar[] {end0, end1, end2});
    model.minimize(obj);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.OPTIMAL) {
      System.out.println("Optimal Schedule Length: " + solver.objectiveValue());
      System.out.println("Task 0 starts at " + solver.value(start0));
      System.out.println("Task 1 starts at " + solver.value(start1));
      System.out.println("Task 2 starts at " + solver.value(start2));
    }
  }
}
