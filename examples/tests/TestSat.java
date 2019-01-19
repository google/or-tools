// Copyright 2010-2018 Google LLC
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

import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import java.util.Random;

/** Tests the CP-SAT java interface. */
public class TestSat {
  static {
    System.loadLibrary("jniortools");
  }

  static void testCrashInPresolve() {
    System.out.println("testCrashInPresolve");
    CpModel model = new CpModel();

    // Create decision variables
    IntVar x = model.newIntVar(0, 5, "x");
    IntVar y = model.newIntVar(0, 5, "y");

    // Create a linear constraint which enforces that only x or y can be greater
    // than 0.
    model.addLinearSum(new IntVar[] { x, y }, 0, 1);

    // Create the objective variable
    IntVar obj = model.newIntVar(0, 3, "obj");

    // Cut the domain of the objective variable
    model.addGreaterOrEqual(obj, 2);

    // Set a constraint that makes the problem infeasible
    model.addMaxEquality(obj, new IntVar[] { x, y });

    // Optimize objective
    model.minimize(obj);

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    com.google.ortools.sat.CpSolverStatus status = solver.solve(model);

    if (status != CpSolverStatus.INFEASIBLE) {
      throw new RuntimeException("Wrong status in testCrashInPresolve");
    } else {
      System.out.println("  ... test OK");
    }
  }

  private static IntVar[] entitiesOne;
  private static void testCrashInSolveWithAllowedAssignment() {
    System.out.println("testCrashInSolveWithAllowedAssignment");
    final CpModel model = new CpModel();
    final int numEntityOne = 50000;
    final int numEntityTwo = 100;
    entitiesOne = new IntVar[numEntityOne];
    for (int i = 0; i < entitiesOne.length; i++) {
      entitiesOne[i] = model.newIntVar(1, numEntityTwo, "E" + i);
    }
    final int[][] allAllowedValues = new int[numEntityTwo][entitiesOne.length];
    for (int i = 0; i < numEntityTwo; i++) {
      for (int j = 0; j < entitiesOne.length; j++) {
        allAllowedValues[i][j] = i;
      }
    }
    try {
      model.addAllowedAssignments(entitiesOne, allAllowedValues);
    } catch (final Exception e) {
      e.printStackTrace();
    }
    final Random r = new Random();
    for (int i = 0; i < entitiesOne.length; i++) {
      model.addEquality(entitiesOne[i], r.nextInt((numEntityTwo)));
    }
    final CpSolver solver = new CpSolver();
    solver.solve(model);
    System.out.println("  ... test OK");
  }

  public static void main(String[] args) throws Exception {
    testCrashInPresolve();
    testCrashInSolveWithAllowedAssignment();

  }
}
