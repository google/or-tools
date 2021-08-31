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

package com.google.ortools.sat;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpSolverStatus;
import java.util.function.Consumer;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Tests the CpSolver java interface. */
public final class CpSolverTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  static class SolutionCounter extends CpSolverSolutionCallback {
    public SolutionCounter() {}

    @Override
    public void onSolutionCallback() {
      solutionCount++;
    }

    private int solutionCount;

    public int getSolutionCount() {
      return solutionCount;
    }
  }

  static class LogToString {
    public LogToString() {
      logBuilder = new StringBuilder();
    }

    public void newMessage(String message) {
      logBuilder.append(message).append("\n");
    }

    private final StringBuilder logBuilder;

    public String getLog() {
      return logBuilder.toString();
    }
  }

  @Test
  public void testCpSolver_solve() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    // Creates the variables.
    int numVals = 3;

    final IntVar x = model.newIntVar(0, numVals - 1, "x");
    final IntVar y = model.newIntVar(0, numVals - 1, "y");
    // Creates the constraints.
    model.addDifferent(x, y);

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    final CpSolverStatus status = solver.solve(model);

    assertThat(status).isEqualTo(CpSolverStatus.OPTIMAL);
    assertThat(solver.value(x)).isNotEqualTo(solver.value(y));
    final String stats = solver.responseStats();
    assertThat(stats).isNotEmpty();
  }

  @Test
  public void testCpSolver_hinting() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newIntVar(0, 5, "x");
    final IntVar y = model.newIntVar(0, 6, "y");
    // Creates the constraints.
    model.addEquality(LinearExpr.sum(new IntVar[] {x, y}), 6);

    // Add hints.
    model.addHint(x, 2);
    model.addHint(y, 4);

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    solver.getParameters().setCpModelPresolve(false);
    final CpSolverStatus status = solver.solve(model);

    assertThat(status).isEqualTo(CpSolverStatus.OPTIMAL);
    assertThat(solver.value(x)).isEqualTo(2);
    assertThat(solver.value(y)).isEqualTo(4);
  }

  @Test
  public void testCpSolver_booleanValue() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newBoolVar("x");
    final IntVar y = model.newBoolVar("y");
    model.addBoolOr(new Literal[] {x, y.not()});

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    final CpSolverStatus status = solver.solve(model);

    assertEquals(CpSolverStatus.OPTIMAL, status);
    assertThat(solver.booleanValue(x) || solver.booleanValue(y.not())).isTrue();
  }

  @Test
  public void testCpSolver_searchAllSolutions() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    // Creates the variables.
    int numVals = 3;
    final IntVar x = model.newIntVar(0, numVals - 1, "x");
    final IntVar y = model.newIntVar(0, numVals - 1, "y");
    model.newIntVar(0, numVals - 1, "z");
    // Creates the constraints.
    model.addDifferent(x, y);

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    final SolutionCounter cb = new SolutionCounter();
    solver.searchAllSolutions(model, cb);

    assertThat(cb.getSolutionCount()).isEqualTo(18);
    assertThat(solver.numBranches()).isGreaterThan(0L);
  }

  @Test
  public void testCpSolver_objectiveValue() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    // Creates the variables.
    final int numVals = 3;
    final IntVar x = model.newIntVar(0, numVals - 1, "x");
    final IntVar y = model.newIntVar(0, numVals - 1, "y");
    final IntVar z = model.newIntVar(0, numVals - 1, "z");
    // Creates the constraints.
    model.addDifferent(x, y);

    // Maximizes a linear combination of variables.
    model.maximize(LinearExpr.scalProd(new IntVar[] {x, y, z}, new int[] {1, 2, 3}));

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    CpSolverStatus status = solver.solve(model);

    assertThat(status).isEqualTo(CpSolverStatus.OPTIMAL);
    assertThat(solver.objectiveValue()).isEqualTo(11.0);
  }

  @Test
  public void testCpModel_crashPresolve() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    // Create decision variables
    final IntVar x = model.newIntVar(0, 5, "x");
    final IntVar y = model.newIntVar(0, 5, "y");

    // Create a linear constraint which enforces that only x or y can be greater than 0.
    model.addLinearConstraint(LinearExpr.sum(new IntVar[] {x, y}), 0, 1);

    // Create the objective variable
    final IntVar obj = model.newIntVar(0, 3, "obj");
    // Cut the domain of the objective variable
    model.addGreaterOrEqual(obj, 2);
    // Set a constraint that makes the problem infeasible
    model.addMaxEquality(obj, new IntVar[] {x, y});
    // Optimize objective
    model.minimize(obj);

    // Create a solver and solve the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    com.google.ortools.sat.CpSolverStatus status = solver.solve(model);
    assertThat(status).isEqualTo(CpSolverStatus.INFEASIBLE);
  }

  @Test
  public void testCpSolver_customLog() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    // Creates the variables.
    final int numVals = 3;
    final IntVar x = model.newIntVar(0, numVals - 1, "x");
    final IntVar y = model.newIntVar(0, numVals - 1, "y");
    // Creates the constraints.
    model.addDifferent(x, y);

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    StringBuilder logBuilder = new StringBuilder();
    Consumer<String> appendToLog = (String message) -> logBuilder.append(message).append('\n');
    solver.setLogCallback(appendToLog);
    solver.getParameters().setLogToStdout(false).setLogSearchProgress(true);
    CpSolverStatus status = solver.solve(model);

    assertThat(status).isEqualTo(CpSolverStatus.OPTIMAL);
    String log = logBuilder.toString();
    assertThat(log).isNotEmpty();
    assertThat(log).contains("Parameters");
    assertThat(log).contains("log_to_stdout: false");
    assertThat(log).contains("OPTIMAL");
  }

  @Test
  public void testCpSolver_customLogMultiThread() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    // Creates the variables.
    int numVals = 3;

    IntVar x = model.newIntVar(0, numVals - 1, "x");
    IntVar y = model.newIntVar(0, numVals - 1, "y");
    // Creates the constraints.
    model.addDifferent(x, y);

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    StringBuilder logBuilder = new StringBuilder();
    Consumer<String> appendToLog = (String message) -> logBuilder.append(message).append('\n');
    solver.setLogCallback(appendToLog);
    solver.getParameters()
          .setLogToStdout(false)
          .setLogSearchProgress(true)
          .setNumSearchWorkers(12);
    CpSolverStatus status = solver.solve(model);

    assertThat(status).isEqualTo(CpSolverStatus.OPTIMAL);
    String log = logBuilder.toString();
    assertThat(log).isNotEmpty();
    assertThat(log).contains("Parameters");
    assertThat(log).contains("log_to_stdout: false");
    assertThat(log).contains("OPTIMAL");
  }
}
