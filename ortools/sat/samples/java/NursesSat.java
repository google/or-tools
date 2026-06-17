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

// [START program]
package com.google.ortools.sat.samples;
// [START import]
import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverSolutionCallback;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.LinearExprBuilder;
import com.google.ortools.sat.Literal;
import java.util.ArrayList;
import java.util.List;
import java.util.stream.IntStream;
// [END import]

/** Nurses problem. */
public class NursesSat {
  public static void main(String[] args) {
    Loader.loadNativeLibraries();
    // [START data]
    final int numNurses = 4;
    final int numDays = 3;
    final int numShifts = 3;

    final int[] allNurses = IntStream.range(0, numNurses).toArray();
    final int[] allDays = IntStream.range(0, numDays).toArray();
    final int[] allShifts = IntStream.range(0, numShifts).toArray();
    // [END data]

    // Creates the model.
    // [START model]
    CpModel model = new CpModel();
    // [END model]

    // Creates shift variables.
    // shifts[(n, d, s)]: nurse 'n' works shift 's' on day 'd'.
    // [START variables]
    Literal[][][] shifts = new Literal[numNurses][numDays][numShifts];
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
        List<Literal> nurses = new ArrayList<>();
        for (int n : allNurses) {
          nurses.add(shifts[n][d][s]);
        }
        model.addExactlyOne(nurses);
      }
    }
    // [END exactly_one_nurse]

    // Each nurse works at most one shift per day.
    // [START at_most_one_shift]
    for (int n : allNurses) {
      for (int d : allDays) {
        List<Literal> work = new ArrayList<>();
        for (int s : allShifts) {
          work.add(shifts[n][d][s]);
        }
        model.addAtMostOne(work);
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
      LinearExprBuilder shiftsWorked = LinearExpr.newBuilder();
      for (int d : allDays) {
        for (int s : allShifts) {
          shiftsWorked.add(shifts[n][d][s]);
        }
      }
      model.addLinearConstraint(shiftsWorked, minShiftsPerNurse, maxShiftsPerNurse);
    }
    // [END assign_nurses_evenly]

    // [START parameters]
    CpSolver solver = new CpSolver();
    solver.getParameters().setLinearizationLevel(0);
    // Tell the solver to enumerate all solutions.
    solver.getParameters().setEnumerateAllSolutions(true);
    // [END parameters]

    // Display the first five solutions.
    // [START solution_printer]
    final int solutionLimit = 5;
    class VarArraySolutionPrinterWithLimit extends CpSolverSolutionCallback {
      public VarArraySolutionPrinterWithLimit(
          int[] allNurses, int[] allDays, int[] allShifts, Literal[][][] shifts, int limit) {
        solutionCount = 0;
        this.allNurses = allNurses;
        this.allDays = allDays;
        this.allShifts = allShifts;
        this.shifts = shifts;
        solutionLimit = limit;
      }

      @Override
      public void onSolutionCallback() {
        System.out.printf("Solution #%d:%n", solutionCount);
        for (int d : allDays) {
          System.out.printf("Day %d%n", d);
          for (int n : allNurses) {
            boolean isWorking = false;
            for (int s : allShifts) {
              if (booleanValue(shifts[n][d][s])) {
                isWorking = true;
                System.out.printf("  Nurse %d work shift %d%n", n, s);
              }
            }
            if (!isWorking) {
              System.out.printf("  Nurse %d does not work%n", n);
            }
          }
        }
        solutionCount++;
        if (solutionCount >= solutionLimit) {
          System.out.printf("Stop search after %d solutions%n", solutionLimit);
          stopSearch();
        }
      }

      public int getSolutionCount() {
        return solutionCount;
      }

      private int solutionCount;
      private final int[] allNurses;
      private final int[] allDays;
      private final int[] allShifts;
      private final Literal[][][] shifts;
      private final int solutionLimit;
    }

    VarArraySolutionPrinterWithLimit cb =
        new VarArraySolutionPrinterWithLimit(allNurses, allDays, allShifts, shifts, solutionLimit);
    // [END solution_printer]

    // Creates a solver and solves the model.
    // [START solve]
    CpSolverStatus status = solver.solve(model, cb);
    System.out.println("Status: " + status);
    System.out.println(cb.getSolutionCount() + " solutions found.");
    // [END solve]

    // Statistics.
    // [START statistics]
    System.out.println("Statistics");
    System.out.printf("  conflicts: %d%n", solver.numConflicts());
    System.out.printf("  branches : %d%n", solver.numBranches());
    System.out.printf("  wall time: %f s%n", solver.wallTime());
    // [END statistics]
  }

  private NursesSat() {}
}
// [END program]
