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
import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;
import java.util.stream.IntStream;
// [END import]

/** Nurses problem with schedule requests. */
public class ScheduleRequestsSat {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // [START data]
    final int numNurses = 5;
    final int numDays = 7;
    final int numShifts = 3;

    final int[] allNurses = IntStream.range(0, numNurses).toArray();
    final int[] allDays = IntStream.range(0, numDays).toArray();
    final int[] allShifts = IntStream.range(0, numShifts).toArray();

    final int[][][] shiftRequests = new int[][][] {
        {
            {0, 0, 1},
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 0},
            {0, 0, 1},
            {0, 1, 0},
            {0, 0, 1},
        },
        {
            {0, 0, 0},
            {0, 0, 0},
            {0, 1, 0},
            {0, 1, 0},
            {1, 0, 0},
            {0, 0, 0},
            {0, 0, 1},
        },
        {
            {0, 1, 0},
            {0, 1, 0},
            {0, 0, 0},
            {1, 0, 0},
            {0, 0, 0},
            {0, 1, 0},
            {0, 0, 0},
        },
        {
            {0, 0, 1},
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0},
            {0, 0, 0},
            {1, 0, 0},
            {0, 0, 0},
        },
        {
            {0, 0, 0},
            {0, 0, 1},
            {0, 1, 0},
            {0, 0, 0},
            {1, 0, 0},
            {0, 1, 0},
            {0, 0, 0},
        },
    };
    // [END data]

    // Creates the model.
    // [START model]
    CpModel model = new CpModel();
    // [END model]

    // Creates shift variables.
    // shifts[(n, d, s)]: nurse 'n' works shift 's' on day 'd'.
    // [START variables]
    IntVar[][][] shifts = new IntVar[numNurses][numDays][numShifts];
    for (int n : allNurses) {
      for (int d : allDays) {
        for (int s : allShifts) {
          shifts[n][d][s] = model.newBoolVar("shifts_n" + n + "d" + d + "s" + s);
        }
      }
    }
    // [END variables]

    // Each shift is assigned to exactly one nurse in the schedule period.
    // [START exactly_one_nurse]
    for (int d : allDays) {
      for (int s : allShifts) {
        IntVar[] x = new IntVar[numNurses];
        for (int n : allNurses) {
          x[n] = shifts[n][d][s];
        }
        model.addEquality(LinearExpr.sum(x), 1);
      }
    }
    // [END exactly_one_nurse]

    // Each nurse works at most one shift per day.
    // [START at_most_one_shift]
    for (int n : allNurses) {
      for (int d : allDays) {
        IntVar[] x = new IntVar[numShifts];
        for (int s : allShifts) {
          x[s] = shifts[n][d][s];
        }
        model.addLessOrEqual(LinearExpr.sum(x), 1);
      }
    }
    // [END at_most_one_shift]

    // [START assign_nurses_evenly]
    // Try to distribute the shifts evenly, so that each nurse works
    // minShiftsPerNurse shifts. If this is not possible, because the total
    // number of shifts is not divisible by the number of nurses, some nurses will
    // be assigned one more shift.
    int minShiftsPerNurse = (numShifts * numDays) / numNurses;
    int maxShiftsPerNurse;
    if ((numShifts * numDays) % numNurses == 0) {
      maxShiftsPerNurse = minShiftsPerNurse;
    } else {
      maxShiftsPerNurse = minShiftsPerNurse + 1;
    }
    for (int n : allNurses) {
      IntVar[] numShiftsWorked = new IntVar[numDays * numShifts];
      for (int d : allDays) {
        for (int s : allShifts) {
          numShiftsWorked[d * numShifts + s] = shifts[n][d][s];
        }
      }
      model.addLinearConstraint(
          LinearExpr.sum(numShiftsWorked), minShiftsPerNurse, maxShiftsPerNurse);
    }
    // [END assign_nurses_evenly]

    // [START objective]
    IntVar[] flatShifts = new IntVar[numNurses * numDays * numShifts];
    int[] flatShiftRequests = new int[numNurses * numDays * numShifts];
    for (int n : allNurses) {
      for (int d : allDays) {
        for (int s : allShifts) {
          flatShifts[n * numDays * numShifts + d * numShifts + s] = shifts[n][d][s];
          flatShiftRequests[n * numDays * numShifts + d * numShifts + s] = shiftRequests[n][d][s];
        }
      }
    }
    model.maximize(LinearExpr.scalProd(flatShifts, flatShiftRequests));
    // [END objective]

    // Creates a solver and solves the model.
    // [START solve]
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);
    // [END solve]

    // [START print_solution]
    if (status == CpSolverStatus.OPTIMAL || status == CpSolverStatus.FEASIBLE) {
      System.out.printf("Solution:%n");
      for (int d : allDays) {
        System.out.printf("Day %d%n", d);
        for (int n : allNurses) {
          for (int s : allShifts) {
            if (solver.value(shifts[n][d][s]) == 1L) {
              if (shiftRequests[n][d][s] == 1) {
                System.out.printf("  Nurse %d works shift %d (requested).%n", n, s);
              } else {
                System.out.printf("  Nurse %d works shift %d (not requested).%n", n, s);
              }
            }
          }
        }
      }
      System.out.printf("Number of shift requests met = %f (out of %d)%n", solver.objectiveValue(),
          numNurses * minShiftsPerNurse);
    } else {
      System.out.printf("No optimal solution found !");
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

  private ScheduleRequestsSat() {}
}
// [END program]
