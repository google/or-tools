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
import com.google.ortools.sat.LinearExpr;
import java.util.logging.Logger;
import java.util.Random;

/** Tests the CP-SAT java interface. */
public class TestSatSolver {
  static { System.loadLibrary("jniortools"); }

  private static final Logger logger = Logger.getLogger(TestSatSolver.class.getName());

  static void testCrashInPresolve() {
    System.out.println("testCrashInPresolve");
    CpModel model = new CpModel();

    // Create decision variables
    IntVar x = model.newIntVar(0, 5, "x");
    IntVar y = model.newIntVar(0, 5, "y");

    // Create a linear constraint which enforces that only x or y can be greater
    // than 0.
    model.addLinearConstraint(LinearExpr.sum(new IntVar[] { x, y }), 0, 1);

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

  private static void testCrashEquality() {
    System.out.println("testCrashInSolveWithAllowedAssignment");
    final CpModel model = new CpModel();

    final IntVar[] entities = new IntVar[20];
    for (int i = 0; i < entities.length; i++) {
      entities[i] = model.newIntVar(1, 5, "E" + i);
    }

    final Integer[] equalities = new Integer[] { 18, 4, 19, 3, 12 };
    addEqualities(model, entities, equalities);

    final Integer[] allowedAssignments = new Integer[] { 12, 8, 15 };
    final Integer[] allowedAssignmentValues = new Integer[] { 1, 3 };
    addAllowedAssignMents(model, entities, allowedAssignments, allowedAssignmentValues);

    final Integer[] forbiddenAssignments1 = new Integer[] { 6, 15, 19 };
    final Integer[] forbiddenAssignments1Values = new Integer[] { 3 };
    final Integer[] forbiddenAssignments2 = new Integer[] { 10, 19 };
    final Integer[] forbiddenAssignments2Values = new Integer[] { 4 };
    final Integer[] forbiddenAssignments3 = new Integer[] { 18, 0, 9, 7 };
    final Integer[] forbiddenAssignments3Values = new Integer[] { 4 };
    final Integer[] forbiddenAssignments4 = new Integer[] { 14, 11 };
    final Integer[] forbiddenAssignments4Values = new Integer[] { 1, 2, 3, 4, 5 };
    final Integer[] forbiddenAssignments5 = new Integer[] { 5, 16, 1, 3 };
    final Integer[] forbiddenAssignments5Values = new Integer[] { 1, 2, 3, 4, 5 };
    final Integer[] forbiddenAssignments6 = new Integer[] { 2, 6, 11, 4 };
    final Integer[] forbiddenAssignments6Values = new Integer[] { 1, 2, 3, 4, 5 };
    final Integer[] forbiddenAssignments7 = new Integer[] { 6, 18, 12, 2, 9, 14 };
    final Integer[] forbiddenAssignments7Values = new Integer[] { 1, 2, 3, 4, 5 };

    addForbiddenAssignments(forbiddenAssignments1Values, forbiddenAssignments1, entities, model);
    addForbiddenAssignments(forbiddenAssignments2Values, forbiddenAssignments2, entities, model);
    addForbiddenAssignments(forbiddenAssignments3Values, forbiddenAssignments3, entities, model);
    addForbiddenAssignments(forbiddenAssignments4Values, forbiddenAssignments4, entities, model);
    addForbiddenAssignments(forbiddenAssignments5Values, forbiddenAssignments5, entities, model);
    addForbiddenAssignments(forbiddenAssignments6Values, forbiddenAssignments6, entities, model);
    addForbiddenAssignments(forbiddenAssignments7Values, forbiddenAssignments7, entities, model);

    final Integer[] configuration = new Integer[] { 5, 4, 2, 3, 3, 3, 4, 3, 3, 1, 4, 4, 3, 1, 4, 1, 4, 4, 3, 3 };
    for (int i = 0; i < configuration.length; i++) {
      model.addEquality(entities[i], configuration[i]);
    }

    final CpSolver solver = new CpSolver();
    solver.solve(model);

    System.out.println("  ... test OK");
  }

  private static void addEqualities(final CpModel model, final IntVar[] entities, final Integer[] equalities) {
    for (int i = 0; i < (equalities.length - 1); i++) {
      model.addEquality(entities[equalities[i]], entities[equalities[i + 1]]);
    }
  }

  private static void addAllowedAssignMents(final CpModel model, final IntVar[] entities, final Integer[] allowedAssignments,
      final Integer[] allowedAssignmentValues) {
    final int[][] allAllowedValues = new int[allowedAssignmentValues.length][allowedAssignments.length];
    for (int i = 0; i < allowedAssignmentValues.length; i++) {
      final Integer value = allowedAssignmentValues[i];
      for (int j = 0; j < allowedAssignments.length; j++) {
        allAllowedValues[i][j] = value;
      }
    }
    final IntVar[] specificEntities = new IntVar[allowedAssignments.length];
    for (int i = 0; i < allowedAssignments.length; i++) {
      specificEntities[i] = entities[allowedAssignments[i]];
    }
    try {
      model.addAllowedAssignments(specificEntities, allAllowedValues);
    } catch (final Exception e) {
      e.printStackTrace();
    }
  }

  private static void addForbiddenAssignments(final Integer[] forbiddenAssignmentsValues, final Integer[] forbiddenAssignments,
      final IntVar[] entities, final CpModel model) {
    final IntVar[] specificEntities = new IntVar[forbiddenAssignments.length];
    for (int i = 0; i < forbiddenAssignments.length; i++) {
      specificEntities[i] = entities[forbiddenAssignments[i]];
    }

    final int[][] notAllowedValues = new int[forbiddenAssignmentsValues.length][forbiddenAssignments.length];
    for (int i = 0; i < forbiddenAssignmentsValues.length; i++) {
      final Integer value = forbiddenAssignmentsValues[i];
      for (int j = 0; j < forbiddenAssignments.length; j++) {
        notAllowedValues[i][j] = value;
      }
    }
    try {
      model.addForbiddenAssignments(specificEntities, notAllowedValues);
    } catch (final Exception e) {
      e.printStackTrace();
    }

  }

  public static void main(String[] args) throws Exception {
    testCrashInPresolve();
    testCrashInSolveWithAllowedAssignment();
    testCrashEquality();
  }
}
