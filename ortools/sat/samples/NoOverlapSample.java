// Copyright 2010-2017 Google
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

import com.google.ortools.sat.*;

public class NoOverlapSample {

  static {
    System.loadLibrary("jniortools");
  }

  public static void NoOverlapSample() {
    CpModel model = new CpModel();
    // Three weeks.
    int horizon = 21;

    // Task 0, duration 2.
    IntVar start_0 = model.newIntVar(0, horizon, "start_0");
    int duration_0 = 2;
    IntVar end_0 = model.newIntVar(0, horizon, "end_0");
    IntervalVar task_0 =
        model.newIntervalVar(start_0, duration_0, end_0, "task_0");

    //  Task 1, duration 4.
    IntVar start_1 = model.newIntVar(0, horizon, "start_1");
    int duration_1 = 4;
    IntVar end_1 = model.newIntVar(0, horizon, "end_1");
    IntervalVar task_1 =
        model.newIntervalVar(start_1, duration_1, end_1, "task_1");

    // Task 2, duration 3.
    IntVar start_2 = model.newIntVar(0, horizon, "start_2");
    int duration_2 = 3;
    IntVar end_2 = model.newIntVar(0, horizon, "end_2");
    IntervalVar task_2 =
        model.newIntervalVar(start_2, duration_2, end_2, "task_2");

    // Weekends.
    IntervalVar weekend_0 = model.newFixedInterval(5, 2, "weekend_0");
    IntervalVar weekend_1 = model.newFixedInterval(12, 2, "weekend_1");
    IntervalVar weekend_2 = model.newFixedInterval(19, 2, "weekend_2");

    // No Overlap constraint.
    model.addNoOverlap(new IntervalVar[] {task_0, task_1, task_2, weekend_0,
                                          weekend_1, weekend_2});

    // Makespan objective.
    IntVar obj = model.newIntVar(0, horizon, "makespan");
    model.addMaxEquality(obj, new IntVar[] {end_0, end_1, end_2});
    model.minimize(obj);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.OPTIMAL)
    {
      System.out.println("Optimal Schedule Length: " + solver.objectiveValue());
      System.out.println("Task 0 starts at " + solver.value(start_0));
      System.out.println("Task 1 starts at " + solver.value(start_1));
      System.out.println("Task 2 starts at " + solver.value(start_2));
    }
  }

  public static void main(String[] args) throws Exception {
    NoOverlapSample();
  }
}
