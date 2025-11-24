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

package com.google.ortools.sat;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.util.Domain;
import java.util.function.Consumer;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests the CpSolver java interface. */
@RunWith(JUnit4.class)
public final class CpSolverTest {
  @SuppressWarnings("EmptyMethods")
  @Before
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
    System.out.println("testCpSolver_solve");
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
  public void testCpSolver_invalidModel() throws Exception {
    System.out.println("testCpSolver_invalidModel");
    final CpModel model = new CpModel();
    assertNotNull(model);
    // Creates the variables.
    int numVals = 3;

    final IntVar x = model.newIntVar(0, -1, "x");
    final IntVar y = model.newIntVar(0, numVals - 1, "y");
    // Creates the constraints.
    model.addDifferent(x, y);

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    final CpSolverStatus status = solver.solve(model);

    assertThat(status).isEqualTo(CpSolverStatus.MODEL_INVALID);
    assertEquals("var #0 has no domain(): name: \"x\"", solver.solutionInfo());
  }

  @Test
  public void testCpSolver_hinting() throws Exception {
    System.out.println("testCpSolver_hinting");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newIntVar(0, 5, "x");
    final IntVar y = model.newIntVar(0, 6, "y");
    // Creates the constraints.
    model.addEquality(LinearExpr.newBuilder().add(x).add(y), 6);

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
    System.out.println("testCpSolver_booleanValue");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
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
    System.out.println("testCpSolver_searchAllSolutions");
    final CpModel model = new CpModel();
    assertNotNull(model);
    // Creates the variables.
    int numVals = 3;
    final IntVar x = model.newIntVar(0, numVals - 1, "x");
    final IntVar y = model.newIntVar(0, numVals - 1, "y");
    final IntVar unusedZ = model.newIntVar(0, numVals - 1, "z");
    // Creates the constraints.
    model.addDifferent(x, y);

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    solver.getParameters().setEnumerateAllSolutions(true);
    final SolutionCounter cb = new SolutionCounter();
    CpSolverStatus status = solver.solve(model, cb);

    assertThat(status).isEqualTo(CpSolverStatus.OPTIMAL);
    assertThat(cb.getSolutionCount()).isEqualTo(18);
    assertThat(solver.numBranches()).isGreaterThan(0L);
  }

  static class BestBoundCallback implements Consumer<Double> {
    public BestBoundCallback() {
      bestBound = 0.0;
    }

    @Override
    public void accept(Double bound) {
      bestBound = bound;
    }

    public double getBestBound() {
      return bestBound;
    }

    double bestBound;
  }

  @Test
  public void testCpSolver_bestBoundCallback() throws Exception {
    System.out.println("testCpSolver_bestBoundCallback");
    final CpModel model = new CpModel();
    assertNotNull(model);
    // Creates the variables.
    final BoolVar b0 = model.newBoolVar("x0");
    final BoolVar b1 = model.newBoolVar("x1");
    final BoolVar b2 = model.newBoolVar("x2");
    final BoolVar b3 = model.newBoolVar("x3");

    model.addBoolOr(new Literal[] {b0, b1, b2, b3});
    model.minimize(DoubleLinearExpr.weightedSumWithOffset(
        new Literal[] {b0, b1, b2, b3}, new double[] {3, 2, 4, 5}, 0.6));

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    solver.getParameters().setNumWorkers(1);
    solver.getParameters().setLinearizationLevel(2);
    BestBoundCallback cb = new BestBoundCallback();
    solver.setBestBoundCallback(cb);
    CpSolverStatus status = solver.solve(model);

    assertThat(status).isEqualTo(CpSolverStatus.OPTIMAL);
    assertThat(cb.getBestBound()).isEqualTo(2.6);
  }

  @Test
  public void testCpSolver_objectiveValue() throws Exception {
    System.out.println("testCpSolver_objectiveValue");
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
    model.maximize(LinearExpr.newBuilder().add(x).addTerm(y, 2).addTerm(z, 3));

    // Creates a solver and solves the model.
    final CpSolver solver = new CpSolver();
    assertNotNull(solver);
    CpSolverStatus status = solver.solve(model);

    assertThat(status).isEqualTo(CpSolverStatus.OPTIMAL);
    assertThat(solver.objectiveValue()).isEqualTo(11.0);
    assertThat(solver.value(LinearExpr.newBuilder().addSum(new IntVar[] {x, y, z}).build()))
        .isEqualTo(solver.value(x) + solver.value(y) + solver.value(z));
  }

  @Test
  public void testCpModel_crashPresolve() throws Exception {
    System.out.println("testCpModel_crashPresolve");
    final CpModel model = new CpModel();
    assertNotNull(model);
    // Create decision variables
    final IntVar x = model.newIntVar(0, 5, "x");
    final IntVar y = model.newIntVar(0, 5, "y");

    // Create a linear constraint which enforces that only x or y can be greater than 0.
    model.addLinearConstraint(LinearExpr.newBuilder().add(x).add(y), 0, 1);

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
    System.out.println("testCpSolver_customLog");
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
  public void testCpSolver_logToResponse() throws Exception {
    System.out.println("testCpSolver_logToResponse");
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
    solver.getParameters().setLogToStdout(false).setLogSearchProgress(true).setLogToResponse(true);
    final CpSolverStatus status = solver.solve(model);

    assertThat(status).isEqualTo(CpSolverStatus.OPTIMAL);
    String log = solver.solveLog();
    assertThat(log).isNotEmpty();
    assertThat(log).contains("Parameters");
    assertThat(log).contains("log_to_stdout: false");
    assertThat(log).contains("OPTIMAL");
  }

  @Test
  public void testCpSolver_customLogMultiThread() {
    System.out.println("testCpSolver_customLogMultiThread");
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
    solver.getParameters().setLogToStdout(false).setLogSearchProgress(true).setNumSearchWorkers(12);
    CpSolverStatus status = solver.solve(model);

    assertThat(status).isEqualTo(CpSolverStatus.OPTIMAL);
    String log = logBuilder.toString();
    assertThat(log).isNotEmpty();
    assertThat(log).contains("Parameters");
    assertThat(log).contains("log_to_stdout: false");
    assertThat(log).contains("OPTIMAL");
  }

  @Test
  public void issue3108() {
    System.out.println("issue3108");
    final CpModel model = new CpModel();
    final IntVar var1 = model.newIntVar(0, 1, "CONTROLLABLE__C1[0]");
    final IntVar var2 = model.newIntVar(0, 1, "CONTROLLABLE__C1[1]");
    capacityConstraint(model, new IntVar[] {var1, var2}, new long[] {0L, 1L},
        new long[][] {new long[] {1L, 1L}}, new long[][] {new long[] {1L, 1L}});
    boolean unused = model.exportToFile("/tmp/issue3108.pb.txt");
    final CpSolver solver = new CpSolver();
    solver.getParameters().setLogSearchProgress(true);
    solver.getParameters().setCpModelProbingLevel(0);
    solver.getParameters().setNumSearchWorkers(4);
    solver.getParameters().setMaxTimeInSeconds(1);
    final CpSolverStatus status = solver.solve(model);
    assertEquals(status, CpSolverStatus.OPTIMAL);
  }

  private static void capacityConstraint(final CpModel model, final IntVar[] varsToAssign,
      final long[] domainArr, final long[][] demands, final long[][] capacities) {
    final int numTasks = varsToAssign.length;
    final int numResources = demands.length;
    final IntervalVar[] tasksIntervals = new IntervalVar[numTasks + capacities[0].length];

    final Domain domainT = Domain.fromValues(domainArr);
    final int unitIntervalSize = 1;
    for (int i = 0; i < numTasks; i++) {
      final BoolVar presence = model.newBoolVar("");
      model.addLinearExpressionInDomain(varsToAssign[i], domainT).onlyEnforceIf(presence);
      model.addLinearExpressionInDomain(varsToAssign[i], domainT.complement())
          .onlyEnforceIf(presence.not());
      // interval with start as taskToNodeAssignment and size of 1
      tasksIntervals[i] =
          model.newOptionalFixedSizeIntervalVar(varsToAssign[i], unitIntervalSize, presence, "");
    }

    // Create dummy intervals
    for (int i = numTasks; i < tasksIntervals.length; i++) {
      final int nodeIndex = i - numTasks;
      tasksIntervals[i] = model.newFixedInterval(domainArr[nodeIndex], 1, "");
    }

    // Convert to list of arrays
    final long[][] nodeCapacities = new long[numResources][];
    final long[] maxCapacities = new long[numResources];

    for (int i = 0; i < capacities.length; i++) {
      final long[] capacityArr = capacities[i];
      long maxCapacityValue = Long.MIN_VALUE;
      for (int j = 0; j < capacityArr.length; j++) {
        maxCapacityValue = Math.max(maxCapacityValue, capacityArr[j]);
      }
      nodeCapacities[i] = capacityArr;
      maxCapacities[i] = maxCapacityValue;
    }

    // For each resource, create dummy demands to accommodate heterogeneous capacities
    final long[][] updatedDemands = new long[numResources][];
    for (int i = 0; i < numResources; i++) {
      final long[] demand = new long[numTasks + capacities[0].length];

      // copy ver task demands
      int iter = 0;
      for (final long taskDemand : demands[i]) {
        demand[iter] = taskDemand;
        iter++;
      }

      // copy over dummy demands
      final long maxCapacity = maxCapacities[i];
      for (final long nodeHeterogeneityAdjustment : nodeCapacities[i]) {
        demand[iter] = maxCapacity - nodeHeterogeneityAdjustment;
        iter++;
      }
      updatedDemands[i] = demand;
    }

    // 2. Capacity constraints
    for (int i = 0; i < numResources; i++) {
      model.addCumulative(maxCapacities[i]).addDemands(tasksIntervals, updatedDemands[i]);
    }

    // Cumulative score
    for (int i = 0; i < numResources; i++) {
      final IntVar max = model.newIntVar(0, maxCapacities[i], "");
      model.addCumulative(max).addDemands(tasksIntervals, updatedDemands[i]);
      model.minimize(max);
    }
  }
}
