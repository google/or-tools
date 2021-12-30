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

package com.google.ortools;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolver;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.TableConstraint;
import com.google.ortools.util.Domain;
import java.util.Random;
import java.util.function.Consumer;
import java.util.logging.Logger;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Tests the CP-SAT java interface. */
public class SatSolverTest {
  private static final Logger logger = Logger.getLogger(SatSolverTest.class.getName());
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testDomainGetter() {
    logger.info("testDomainGetter");
    CpModel model = new CpModel();

    // Create decision variables
    IntVar x = model.newIntVar(0, 5, "x");

    Domain d = x.getDomain();
    long[] flat = d.flattenedIntervals();
    if (flat.length != 2 || flat[0] != 0 || flat[1] != 5) {
      throw new RuntimeException("Wrong domain");
    }
  }

  @Test
  public void testCrashInPresolve() {
    logger.info("testCrashInPresolve");
    CpModel model = new CpModel();

    // Create decision variables
    IntVar x = model.newIntVar(0, 5, "x");
    IntVar y = model.newIntVar(0, 5, "y");

    // Create a linear constraint which enforces that only x or y can be greater
    // than 0.
    model.addLinearConstraint(LinearExpr.sum(new IntVar[] {x, y}), 0, 1);

    // Create the objective variable
    IntVar obj = model.newIntVar(0, 3, "obj");

    // Cut the domain of the objective variable
    model.addGreaterOrEqual(obj, 2);

    // Set a constraint that makes the problem infeasible
    model.addMaxEquality(obj, new IntVar[] {x, y});

    // Optimize objective
    model.minimize(obj);

    // Create a solver and solve the model.
    CpSolver solver = new CpSolver();
    com.google.ortools.sat.CpSolverStatus status = solver.solve(model);

    if (status != CpSolverStatus.INFEASIBLE) {
      throw new IllegalStateException("Wrong status in testCrashInPresolve");
    }
  }

  @Test
  public void testCrashInSolveWithAllowedAssignment() {
    logger.info("testCrashInSolveWithAllowedAssignment");
    final CpModel model = new CpModel();
    final int numEntityOne = 50000;
    final int numEntityTwo = 100;
    final IntVar[] entitiesOne = new IntVar[numEntityOne];
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
      TableConstraint table = model.addAllowedAssignments(entitiesOne);
      final int[] oneTuple = new int[entitiesOne.length];
      for (int i = 0; i < numEntityTwo; i++) {
        for (int j = 0; j < entitiesOne.length; j++) {
          oneTuple[j] = i;
        }
        table.addTuple(oneTuple);
      }
    } catch (final Exception e) {
      e.printStackTrace();
    }
    final Random r = new Random();
    for (int i = 0; i < entitiesOne.length; i++) {
      model.addEquality(entitiesOne[i], r.nextInt((numEntityTwo)));
    }
    final CpSolver solver = new CpSolver();
    solver.solve(model);
  }

  @Test
  public void testCrashEquality() {
    logger.info("testCrashInSolveWithAllowedAssignment");
    final CpModel model = new CpModel();

    final IntVar[] entities = new IntVar[20];
    for (int i = 0; i < entities.length; i++) {
      entities[i] = model.newIntVar(1, 5, "E" + i);
    }

    final int[] equalities = new int[] {18, 4, 19, 3, 12};
    addEqualities(model, entities, equalities);

    final int[] allowedAssignments = new int[] {12, 8, 15};
    final int[] allowedAssignmentValues = new int[] {1, 3};
    addAllowedAssignMents(model, entities, allowedAssignments, allowedAssignmentValues);

    final int[] forbiddenAssignments1 = new int[] {6, 15, 19};
    final int[] forbiddenAssignments1Values = new int[] {3};
    final int[] forbiddenAssignments2 = new int[] {10, 19};
    final int[] forbiddenAssignments2Values = new int[] {4};
    final int[] forbiddenAssignments3 = new int[] {18, 0, 9, 7};
    final int[] forbiddenAssignments3Values = new int[] {4};
    final int[] forbiddenAssignments4 = new int[] {14, 11};
    final int[] forbiddenAssignments4Values = new int[] {1, 2, 3, 4, 5};
    final int[] forbiddenAssignments5 = new int[] {5, 16, 1, 3};
    final int[] forbiddenAssignments5Values = new int[] {1, 2, 3, 4, 5};
    final int[] forbiddenAssignments6 = new int[] {2, 6, 11, 4};
    final int[] forbiddenAssignments6Values = new int[] {1, 2, 3, 4, 5};
    final int[] forbiddenAssignments7 = new int[] {6, 18, 12, 2, 9, 14};
    final int[] forbiddenAssignments7Values = new int[] {1, 2, 3, 4, 5};

    addForbiddenAssignments(forbiddenAssignments1Values, forbiddenAssignments1, entities, model);
    addForbiddenAssignments(forbiddenAssignments2Values, forbiddenAssignments2, entities, model);
    addForbiddenAssignments(forbiddenAssignments3Values, forbiddenAssignments3, entities, model);
    addForbiddenAssignments(forbiddenAssignments4Values, forbiddenAssignments4, entities, model);
    addForbiddenAssignments(forbiddenAssignments5Values, forbiddenAssignments5, entities, model);
    addForbiddenAssignments(forbiddenAssignments6Values, forbiddenAssignments6, entities, model);
    addForbiddenAssignments(forbiddenAssignments7Values, forbiddenAssignments7, entities, model);

    final int[] configuration =
        new int[] {5, 4, 2, 3, 3, 3, 4, 3, 3, 1, 4, 4, 3, 1, 4, 1, 4, 4, 3, 3};
    for (int i = 0; i < configuration.length; i++) {
      model.addEquality(entities[i], configuration[i]);
    }

    final CpSolver solver = new CpSolver();
    solver.solve(model);
  }

  @Test
  public void testLogCapture() {
    logger.info("testLogCapture");

    // Creates the model.
    CpModel model = new CpModel();
    // Creates the variables.
    int numVals = 3;

    IntVar x = model.newIntVar(0, numVals - 1, "x");
    IntVar y = model.newIntVar(0, numVals - 1, "y");
    // Creates the constraints.
    model.addDifferent(x, y);

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    StringBuilder logBuilder = new StringBuilder();
    Consumer<String> appendToLog = (String message) -> {
      logger.info("Current Thread Name:" + Thread.currentThread().getName()
          + " Id:" + Thread.currentThread().getId() + " msg:" + message);
      logBuilder.append(message).append('\n');
    };
    solver.setLogCallback(appendToLog);
    solver.getParameters().setLogToStdout(false).setLogSearchProgress(true);
    CpSolverStatus status = solver.solve(model);
    if (status != CpSolverStatus.OPTIMAL) {
      throw new IllegalStateException("Wrong status in testCrashInPresolve");
    }

    String log = logBuilder.toString();
    if (log.isEmpty()) {
      throw new IllegalStateException("Log should not be empty");
    }
  }

  private void addEqualities(final CpModel model, final IntVar[] entities, final int[] equalities) {
    for (int i = 0; i < (equalities.length - 1); i++) {
      model.addEquality(entities[equalities[i]], entities[equalities[i + 1]]);
    }
  }

  private void addAllowedAssignMents(final CpModel model, final IntVar[] entities,
      final int[] allowedAssignments, final int[] allowedAssignmentValues) {
    final int[][] allAllowedValues =
        new int[allowedAssignmentValues.length][allowedAssignments.length];
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
      TableConstraint table = model.addAllowedAssignments(specificEntities);
      for (int[] tuple : allAllowedValues) {
        table.addTuple(tuple);
      }
    } catch (final Exception e) {
      e.printStackTrace();
    }
  }

  private void addForbiddenAssignments(final int[] forbiddenAssignmentsValues,
      final int[] forbiddenAssignments, final IntVar[] entities, final CpModel model) {
    final IntVar[] specificEntities = new IntVar[forbiddenAssignments.length];
    for (int i = 0; i < forbiddenAssignments.length; i++) {
      specificEntities[i] = entities[forbiddenAssignments[i]];
    }

    final int[][] notAllowedValues =
        new int[forbiddenAssignmentsValues.length][forbiddenAssignments.length];
    for (int i = 0; i < forbiddenAssignmentsValues.length; i++) {
      final Integer value = forbiddenAssignmentsValues[i];
      for (int j = 0; j < forbiddenAssignments.length; j++) {
        notAllowedValues[i][j] = value;
      }
    }
    try {
      TableConstraint table = model.addForbiddenAssignments(specificEntities);
      for (int[] tuple : notAllowedValues) {
        table.addTuple(tuple);
      }
    } catch (final Exception e) {
      e.printStackTrace();
    }
  }
}
