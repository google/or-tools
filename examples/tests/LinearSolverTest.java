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

package com.google.ortools.linearsolver;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.google.ortools.Loader;
import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Test the Linear Solver java interface. */
public final class LinearSolverTest {
  // Numerical tolerance for checking primal, dual, objective values
  // and other values.
  private static final double NUM_TOLERANCE = 1e-5;

  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  private void runBasicCtor(MPSolver.OptimizationProblemType solverType) {
    if (!MPSolver.supportsProblemType(solverType)) {
      return;
    }
    final MPSolver solver = new MPSolver("testBasicCtor", solverType);
    assertNotNull(solver);
    solver.solve();
  }

  @Test
  public void testMPSolver_basicCtor() {
    runBasicCtor(MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING);
    runBasicCtor(MPSolver.OptimizationProblemType.GLPK_LINEAR_PROGRAMMING);
    runBasicCtor(MPSolver.OptimizationProblemType.GLPK_MIXED_INTEGER_PROGRAMMING);
    runBasicCtor(MPSolver.OptimizationProblemType.CLP_LINEAR_PROGRAMMING);
    runBasicCtor(MPSolver.OptimizationProblemType.CBC_MIXED_INTEGER_PROGRAMMING);
  }

  @Test
  public void testMPSolver_destructor() {
    final MPSolver solver =
        new MPSolver("testDestructor", MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING);
    assertNotNull(solver);
    solver.delete();
  }

  private void runLinearSolver(
      MPSolver.OptimizationProblemType problemType, boolean integerVariables) {
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("Solver", problemType);
    assertNotNull(solver);

    final double infinity = MPSolver.infinity();
    final MPVariable x1 = solver.makeNumVar(0.0, infinity, "x1");
    final MPVariable x2 = solver.makeNumVar(0.0, infinity, "x2");
    final MPVariable x3 = solver.makeNumVar(0.0, infinity, "x3");
    if (integerVariables) {
      x1.setInteger(true);
      x2.setInteger(true);
      x3.setInteger(true);
    }
    assertEquals(3, solver.numVariables());

    final MPObjective objective = solver.objective();
    objective.setCoefficient(x1, 10);
    objective.setCoefficient(x2, 6);
    objective.setCoefficient(x3, 4);
    objective.setMaximization();
    assertEquals(6.0, objective.getCoefficient(x2), 1e-6);
    assertTrue(objective.maximization());
    assertFalse(objective.minimization());

    final MPConstraint c0 = solver.makeConstraint(-1000, 100.0);
    c0.setCoefficient(x1, 1);
    c0.setCoefficient(x2, 1);
    c0.setCoefficient(x3, 1);

    final MPConstraint c1 = solver.makeConstraint(-1000, 600.0);
    c1.setCoefficient(x1, 10);
    c1.setCoefficient(x2, 4);
    c1.setCoefficient(x3, 5);
    assertEquals(4.0, c1.getCoefficient(x2), 1e-6);

    final MPConstraint c2 = solver.makeConstraint(-1000, 300.0);
    c2.setCoefficient(x1, 2);
    c2.setCoefficient(x2, 2);
    c2.setCoefficient(x3, 6);

    assertEquals(MPSolver.ResultStatus.OPTIMAL, solver.solve());
    if (integerVariables) {
      assertEquals(732.0, objective.value(), NUM_TOLERANCE);
      assertEquals(33.0, x1.solutionValue(), NUM_TOLERANCE);
      assertEquals(67.0, x2.solutionValue(), NUM_TOLERANCE);
      assertEquals(0.0, x3.solutionValue(), NUM_TOLERANCE);
    } else {
      assertEquals(733.333333, objective.value(), NUM_TOLERANCE);
      assertEquals(33.333333, x1.solutionValue(), NUM_TOLERANCE);
      assertEquals(66.666667, x2.solutionValue(), NUM_TOLERANCE);
      assertEquals(0, x3.solutionValue(), NUM_TOLERANCE);
    }
  }

  @Test
  public void testMPSolver_linearSolver() {
    runLinearSolver(MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING, false);
    runLinearSolver(MPSolver.OptimizationProblemType.CLP_LINEAR_PROGRAMMING, false);
    runLinearSolver(MPSolver.OptimizationProblemType.GLPK_LINEAR_PROGRAMMING, false);

    runLinearSolver(MPSolver.OptimizationProblemType.CBC_MIXED_INTEGER_PROGRAMMING, true);
    runLinearSolver(MPSolver.OptimizationProblemType.GLPK_MIXED_INTEGER_PROGRAMMING, true);
    runLinearSolver(MPSolver.OptimizationProblemType.SCIP_MIXED_INTEGER_PROGRAMMING, true);
  }

  private void runFirstLinearExample(MPSolver.OptimizationProblemType problemType) {
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("Solver", problemType);
    assertNotNull(solver);

    final MPVariable x1 = solver.makeNumVar(0.0, Double.POSITIVE_INFINITY, "x1");
    final MPVariable x2 = solver.makeNumVar(0.0, Double.POSITIVE_INFINITY, "x2");
    final MPVariable x3 = solver.makeNumVar(0.0, Double.POSITIVE_INFINITY, "x3");
    assertEquals(3, solver.numVariables());

    final double[] obj = {10.0, 6.0, 4.0};
    final MPObjective objective = solver.objective();
    objective.setCoefficient(x1, obj[0]);
    objective.setCoefficient(x2, obj[1]);
    objective.setCoefficient(x3, obj[2]);
    objective.setMaximization();

    final double rhs0 = 100.0;
    final MPConstraint c0 = solver.makeConstraint(-Double.POSITIVE_INFINITY, rhs0, "c0");
    final double[] coef0 = {1.0, 1.0, 1.0};
    c0.setCoefficient(x1, coef0[0]);
    c0.setCoefficient(x2, coef0[1]);
    c0.setCoefficient(x3, coef0[2]);
    final double rhs1 = 600.0;
    final MPConstraint c1 = solver.makeConstraint(-Double.POSITIVE_INFINITY, rhs1, "c1");
    final double[] coef1 = {10.0, 4.0, 5.0};
    c1.setCoefficient(x1, coef1[0]);
    c1.setCoefficient(x2, coef1[1]);
    c1.setCoefficient(x3, coef1[2]);
    final double rhs2 = 300.0;
    final MPConstraint c2 = solver.makeConstraint(-Double.POSITIVE_INFINITY, rhs2);
    final double[] coef2 = {2.0, 2.0, 6.0};
    c2.setCoefficient(x1, coef2[0]);
    c2.setCoefficient(x2, coef2[1]);
    c2.setCoefficient(x3, coef2[2]);
    assertEquals(3, solver.numConstraints());
    assertEquals("c0", c0.name());
    assertEquals("c1", c1.name());
    assertEquals("auto_c_000000002", c2.name());

    // The problem has an optimal solution.;
    assertEquals(MPSolver.ResultStatus.OPTIMAL, solver.solve());

    assertEquals(733.333333, objective.value(), NUM_TOLERANCE);
    assertEquals(33.333333, x1.solutionValue(), NUM_TOLERANCE);
    assertEquals(66.666667, x2.solutionValue(), NUM_TOLERANCE);
    assertEquals(0, x3.solutionValue(), NUM_TOLERANCE);

    // c0 and c1 are binding;
    final double[] activities = solver.computeConstraintActivities();
    assertEquals(3, activities.length);
    assertEquals(3.333333, c0.dualValue(), NUM_TOLERANCE);
    assertEquals(0.666667, c1.dualValue(), NUM_TOLERANCE);
    assertEquals(rhs0, activities[c0.index()], NUM_TOLERANCE);
    assertEquals(rhs1, activities[c1.index()], NUM_TOLERANCE);
    assertEquals(MPSolver.BasisStatus.AT_UPPER_BOUND, c0.basisStatus());
    assertEquals(MPSolver.BasisStatus.AT_UPPER_BOUND, c1.basisStatus());
    // c2 is not binding;
    assertEquals(0.0, c2.dualValue(), NUM_TOLERANCE);
    assertEquals(200.0, activities[c2.index()], NUM_TOLERANCE);
    assertEquals(MPSolver.BasisStatus.BASIC, c2.basisStatus());
    // The optimum of the dual problem is equal to the optimum of the;
    // primal problem.;
    final double dualObjectiveValue = c0.dualValue() * rhs0 + c1.dualValue() * rhs1;
    assertEquals(objective.value(), dualObjectiveValue, NUM_TOLERANCE);

    // x1 and x2 are basic;
    assertEquals(0.0, x1.reducedCost(), NUM_TOLERANCE);
    assertEquals(0.0, x2.reducedCost(), NUM_TOLERANCE);
    assertEquals(MPSolver.BasisStatus.BASIC, x1.basisStatus());
    assertEquals(MPSolver.BasisStatus.BASIC, x2.basisStatus());
    // x3 is non-basic;
    final double x3ExpectedReducedCost =
        (obj[2] - coef0[2] * c0.dualValue() - coef1[2] * c1.dualValue());
    assertEquals(x3ExpectedReducedCost, x3.reducedCost(), NUM_TOLERANCE);
    assertEquals(MPSolver.BasisStatus.AT_LOWER_BOUND, x3.basisStatus());

    if (solver.problemType() == MPSolver.OptimizationProblemType.GLPK_LINEAR_PROGRAMMING) {
      assertEquals(56.333333, solver.computeExactConditionNumber(), NUM_TOLERANCE);
    }
  }

  @Test
  public void testMPSolver_firstLinearExample() {
    runFirstLinearExample(MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING);
    runFirstLinearExample(MPSolver.OptimizationProblemType.CLP_LINEAR_PROGRAMMING);
    runFirstLinearExample(MPSolver.OptimizationProblemType.GLPK_LINEAR_PROGRAMMING);
    runFirstLinearExample(MPSolver.OptimizationProblemType.GUROBI_LINEAR_PROGRAMMING);
  }

  private void runFirstMIPExample(MPSolver.OptimizationProblemType problemType) {
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("Solver", problemType);
    assertNotNull(solver);

    // Integer variables shouldn't have infinite bounds, nor really large bounds:
    // it can make your solver behave erratically. If you have integer variables
    // with a truly large dynamic range you should consider making it non-integer.
    final double upperBound = 1000;
    final MPVariable x1 = solver.makeIntVar(0.0, upperBound, "x1");
    final MPVariable x2 = solver.makeIntVar(0.0, upperBound, "x2");

    solver.objective().setCoefficient(x1, 1);
    solver.objective().setCoefficient(x2, 2);

    final MPConstraint ct = solver.makeConstraint(17, Double.POSITIVE_INFINITY);
    ct.setCoefficient(x1, 3);
    ct.setCoefficient(x2, 2);

    // Check the solution.
    assertEquals(MPSolver.ResultStatus.OPTIMAL, solver.solve());
    final double optObjValue = 6.0;
    assertEquals(optObjValue, solver.objective().value(), 1e-6);
    assertEquals(optObjValue, solver.objective().bestBound(), 1e-6);
    final double optRowActivity = 18.0;
    assertEquals(optRowActivity, solver.computeConstraintActivities()[ct.index()], NUM_TOLERANCE);
    // BOP does not support nodes().
    if (solver.problemType() != MPSolver.OptimizationProblemType.BOP_INTEGER_PROGRAMMING) {
      assertThat(solver.nodes()).isAtLeast(0);
    }
  }

  @Test
  public void testMPSolver_firstMIPExample() {
    runFirstMIPExample(MPSolver.OptimizationProblemType.BOP_INTEGER_PROGRAMMING);
    runFirstMIPExample(MPSolver.OptimizationProblemType.CBC_MIXED_INTEGER_PROGRAMMING);
    runFirstMIPExample(MPSolver.OptimizationProblemType.GLPK_MIXED_INTEGER_PROGRAMMING);
    runFirstMIPExample(MPSolver.OptimizationProblemType.SCIP_MIXED_INTEGER_PROGRAMMING);
    runFirstMIPExample(MPSolver.OptimizationProblemType.SAT_INTEGER_PROGRAMMING);
    runFirstMIPExample(MPSolver.OptimizationProblemType.GUROBI_MIXED_INTEGER_PROGRAMMING);
  }

  private void runSuccessiveObjectives(MPSolver.OptimizationProblemType problemType) {
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("Solver", problemType);
    assertNotNull(solver);

    final MPVariable x1 = solver.makeNumVar(0, 10, "var1");
    final MPVariable x2 = solver.makeNumVar(0, 10, "var2");
    final MPConstraint ct = solver.makeConstraint(0, 10);
    ct.setCoefficient(x1, 1);
    ct.setCoefficient(x2, 2);

    final MPObjective objective = solver.objective();
    objective.setCoefficient(x1, 1);
    objective.setCoefficient(x2, 0);
    objective.setOptimizationDirection(true);

    // Check the solution.
    assertEquals(MPSolver.ResultStatus.OPTIMAL, solver.solve());
    assertEquals(10.0, x1.solutionValue(), NUM_TOLERANCE);
    assertEquals(0.0, x2.solutionValue(), NUM_TOLERANCE);

    objective.setCoefficient(x1, 0);
    objective.setCoefficient(x2, 1);
    objective.setOptimizationDirection(true);

    // Check the solution
    assertEquals(MPSolver.ResultStatus.OPTIMAL, solver.solve());
    assertEquals(0.0, x1.solutionValue(), NUM_TOLERANCE);
    assertEquals(5.0, x2.solutionValue(), NUM_TOLERANCE);

    objective.setCoefficient(x1, -1);
    objective.setCoefficient(x2, 0);
    objective.setOptimizationDirection(false);

    // Check the solution.
    assertEquals(MPSolver.ResultStatus.OPTIMAL, solver.solve());
    assertEquals(10.0, x1.solutionValue(), NUM_TOLERANCE);
    assertEquals(0.0, x2.solutionValue(), NUM_TOLERANCE);
  }

  @Test
  public void testMPSolver_successiveObjectives() {
    runSuccessiveObjectives(MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING);
    runSuccessiveObjectives(MPSolver.OptimizationProblemType.CLP_LINEAR_PROGRAMMING);
    runSuccessiveObjectives(MPSolver.OptimizationProblemType.GLPK_LINEAR_PROGRAMMING);
    runSuccessiveObjectives(MPSolver.OptimizationProblemType.GUROBI_LINEAR_PROGRAMMING);

    runSuccessiveObjectives(MPSolver.OptimizationProblemType.CBC_MIXED_INTEGER_PROGRAMMING);
    runSuccessiveObjectives(MPSolver.OptimizationProblemType.GLPK_MIXED_INTEGER_PROGRAMMING);
    runSuccessiveObjectives(MPSolver.OptimizationProblemType.SCIP_MIXED_INTEGER_PROGRAMMING);
    runSuccessiveObjectives(MPSolver.OptimizationProblemType.SAT_INTEGER_PROGRAMMING);
    runSuccessiveObjectives(MPSolver.OptimizationProblemType.GUROBI_MIXED_INTEGER_PROGRAMMING);
  }

  private void runObjectiveOffset(MPSolver.OptimizationProblemType problemType) {
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("Solver", problemType);
    assertNotNull(solver);

    final MPVariable x1 = solver.makeNumVar(1.0, 10.0, "x1");
    final MPVariable x2 = solver.makeNumVar(1.0, 10.0, "x2");

    final MPConstraint ct = solver.makeConstraint(0, 4.0);
    ct.setCoefficient(x1, 1);
    ct.setCoefficient(x2, 2);

    final double objectiveOffset = 10.0;
    // Simple minimization.
    final MPObjective objective = solver.objective();
    objective.setCoefficient(x1, 1.0);
    objective.setCoefficient(x2, 1.0);
    objective.setOffset(objectiveOffset);
    objective.setOptimizationDirection(false);

    assertEquals(MPSolver.ResultStatus.OPTIMAL, solver.solve());
    assertEquals(2.0 + objectiveOffset, objective.value(), 1e-6);

    // Offset is provided in several separate constants.
    objective.setCoefficient(x1, 1.0);
    objective.setCoefficient(x2, 1.0);
    objective.setOffset(-1.0);
    objective.setOffset(objectiveOffset + objective.offset());
    objective.setOffset(1.0 + objective.offset());
    assertEquals(MPSolver.ResultStatus.OPTIMAL, solver.solve());
    assertEquals(2.0 + objectiveOffset, objective.value(), 1e-6);

    // Simple maximization.
    objective.setCoefficient(x1, 1.0);
    objective.setCoefficient(x2, 1.0);
    objective.setOffset(objectiveOffset);
    objective.setOptimizationDirection(true);
    assertEquals(MPSolver.ResultStatus.OPTIMAL, solver.solve());
    assertEquals(3.0 + objectiveOffset, objective.value(), 1e-6);
  }

  @Test
  public void testMPSolver_objectiveOffset() {
    runObjectiveOffset(MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING);
    runObjectiveOffset(MPSolver.OptimizationProblemType.CLP_LINEAR_PROGRAMMING);
    runObjectiveOffset(MPSolver.OptimizationProblemType.GLPK_LINEAR_PROGRAMMING);
    runObjectiveOffset(MPSolver.OptimizationProblemType.GUROBI_LINEAR_PROGRAMMING);

    runObjectiveOffset(MPSolver.OptimizationProblemType.CBC_MIXED_INTEGER_PROGRAMMING);
    runObjectiveOffset(MPSolver.OptimizationProblemType.GLPK_MIXED_INTEGER_PROGRAMMING);
    runObjectiveOffset(MPSolver.OptimizationProblemType.SCIP_MIXED_INTEGER_PROGRAMMING);
    runObjectiveOffset(MPSolver.OptimizationProblemType.SAT_INTEGER_PROGRAMMING);
    runObjectiveOffset(MPSolver.OptimizationProblemType.GUROBI_MIXED_INTEGER_PROGRAMMING);
  }

  @Test
  public void testMPSolver_lazyConstraints() {
    final MPSolver.OptimizationProblemType problemType =
        MPSolver.OptimizationProblemType.SCIP_MIXED_INTEGER_PROGRAMMING;
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("testLazyConstraints", problemType);
    assertNotNull(solver);

    final double infinity = MPSolver.infinity();
    final MPVariable x = solver.makeIntVar(0, infinity, "x");
    final MPVariable y = solver.makeIntVar(0, infinity, "y");
    final MPConstraint ct1 = solver.makeConstraint(0, 10.0);
    ct1.setCoefficient(x, 2.0);
    ct1.setCoefficient(y, 1.0);
    final MPConstraint ct2 = solver.makeConstraint(0, 10.0);
    ct2.setCoefficient(x, 1.0);
    ct2.setCoefficient(y, 2.0);
    ct2.setIsLazy(true);
    assertFalse(ct1.isLazy());
    assertTrue(ct2.isLazy());
    final MPObjective objective = solver.objective();
    objective.setCoefficient(x, 1.0);
    objective.setCoefficient(y, 1.0);
    objective.setOptimizationDirection(true);
    assertEquals(MPSolver.ResultStatus.OPTIMAL, solver.solve());
    assertEquals(solver.objective().value(), 6.0, NUM_TOLERANCE);
  }

  @Test
  public void testMPSolver_sameConstraintName() {
    MPSolver solver = MPSolver.createSolver("GLOP");
    assertNotNull(solver);
    boolean success = true;
    solver.makeConstraint("my_const_name");
    try {
      solver.makeConstraint("my_const_name");
    } catch (Throwable e) {
      System.out.println(e);
      success = false;
    }
    assertTrue(success);
  }

  @Test
  public void testMPSolver_exportModelToProto() {
    final MPSolver.OptimizationProblemType problemType =
        MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING;
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("testExportModelToProto", problemType);
    assertNotNull(solver);
    solver.makeNumVar(0.0, 10.0, "x1");
    solver.makeConstraint(0.0, 0.0);
    solver.objective().setOptimizationDirection(true);
    final MPModelProto model = solver.exportModelToProto();
    assertEquals(1, model.getVariableCount());
    assertEquals(1, model.getConstraintCount());
    assertTrue(model.getMaximize());
  }

  @Test
  public void testMPsolver_createSolutionResponseProto() {
    final MPSolver.OptimizationProblemType problemType =
        MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING;
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("testCreateSolutionResponseProto", problemType);
    assertNotNull(solver);
    final MPVariable x1 = solver.makeNumVar(0.0, 10.0, "x1");
    solver.objective().setCoefficient(x1, 1.0);
    solver.objective().setOptimizationDirection(true);
    solver.solve();
    final MPSolutionResponse response = solver.createSolutionResponseProto();
    assertEquals(MPSolverResponseStatus.MPSOLVER_OPTIMAL, response.getStatus());
    assertEquals(10.0, response.getObjectiveValue(), 1e-6);
  }

  @Test
  public void testMPSolver_solveWithProto() {
    final MPSolver.OptimizationProblemType problemType =
        MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING;
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPModelProto.Builder modelBuilder = MPModelProto.newBuilder().setMaximize(true);
    final MPVariableProto variable = MPVariableProto.newBuilder()
                                         .setLowerBound(0.0)
                                         .setUpperBound(10.0)
                                         .setName("x1")
                                         .setIsInteger(false)
                                         .setObjectiveCoefficient(1.0)
                                         .build();
    modelBuilder.addVariable(variable);
    final MPModelRequest request = MPModelRequest.newBuilder()
                                 .setModel(modelBuilder.build())
                                 .setSolverType(MPModelRequest.SolverType.GLOP_LINEAR_PROGRAMMING)
                                 .build();
    final MPSolutionResponse response = MPSolver.solveWithProto(request);
    assertEquals(MPSolverResponseStatus.MPSOLVER_OPTIMAL, response.getStatus());
    assertEquals(10.0, response.getObjectiveValue(), 1e-6);
  }

  @Test
  public void testModelExport() {
    final MPSolver.OptimizationProblemType problemType =
        MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING;
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("tesModelExport", problemType);
    assertNotNull(solver);
    final double infinity = MPSolver.infinity();
    // x1, x2 and x3 are continuous non-negative variables.
    final MPVariable x1 = solver.makeNumVar(0.0, infinity, "x1");

    // Maximize 10 * x1.
    solver.objective().setCoefficient(x1, 10);
    solver.objective().setMinimization();

    // 5 * x1 <= 30.
    final MPConstraint c0 = solver.makeConstraint(-infinity, 100.0);
    c0.setCoefficient(x1, 5);

    final MPModelExportOptions obfuscate = new MPModelExportOptions();
    obfuscate.setObfuscate(true);
    String out = solver.exportModelAsLpFormat();
    assertThat(out).isNotEmpty();
    out = solver.exportModelAsLpFormat(obfuscate);
    assertThat(out).isNotEmpty();
    out = solver.exportModelAsMpsFormat();
    assertThat(out).isNotEmpty();
    out = solver.exportModelAsMpsFormat(obfuscate);
    assertThat(out).isNotEmpty();
  }

  @Test
  public void testMPSolver_wrongModelExport() {
    final MPSolver.OptimizationProblemType problemType =
        MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING;
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("testWrongModelExport", problemType);
    assertNotNull(solver);
    // Test that forbidden names are renamed.
    solver.makeBoolVar("<-%$#!&~-+ ⌂"); // Some illegal name.
    String out = solver.exportModelAsLpFormat();
    assertThat(out).isNotEmpty();
    out = solver.exportModelAsMpsFormat();
    assertThat(out).isNotEmpty();
  }

  @Test
  public void testMPSolver_setHint() {
    final MPSolver.OptimizationProblemType problemType =
        MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING;
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("testSetHint", problemType);
    assertNotNull(solver);
    final MPVariable[] variables = {
        solver.makeNumVar(0.0, 10.0, "x1"), solver.makeNumVar(0.0, 10.0, "x2")};
    final double[] values = {5.0, 6.0};
    solver.setHint(variables, values);

    final MPModelProto model = solver.exportModelToProto();
    final PartialVariableAssignment hint = model.getSolutionHint();
    assertEquals(2, hint.getVarIndexCount());
    assertEquals(2, hint.getVarValueCount());
    assertEquals(0, hint.getVarIndex(0));
    assertEquals(5.0, hint.getVarValue(0), 1e-6);
    assertEquals(1, hint.getVarIndex(1));
    assertEquals(6.0, hint.getVarValue(1), 1e-6);
  }

  @Test
  public void testMPSolver_issue132() {
    final MPSolver.OptimizationProblemType problemType =
        MPSolver.OptimizationProblemType.CLP_LINEAR_PROGRAMMING;
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("CoinError", problemType);
    assertNotNull(solver);
    final double infinity = MPSolver.infinity();
    final MPVariable x0 = solver.makeNumVar(0.0, 1.0, "x0");
    final MPVariable x1 = solver.makeNumVar(0.0, 0.3, "x1");
    final MPVariable x2 = solver.makeNumVar(0.0, 0.3, "x2");
    final MPVariable x3 = solver.makeNumVar(-infinity, infinity, "x3");

    final MPObjective obj = solver.objective();
    obj.setCoefficient(x1, 2.655523);
    obj.setCoefficient(x2, -2.70917);
    obj.setCoefficient(x3, 1);
    obj.setMaximization();

    final MPConstraint c0 = solver.makeConstraint(-infinity, 0.302499);
    c0.setCoefficient(x3, 1);
    c0.setCoefficient(x0, -3.484345);

    final MPConstraint c1 = solver.makeConstraint(-infinity, 0.507194);
    c1.setCoefficient(x3, 1);
    c1.setCoefficient(x0, -3.074807);

    final MPConstraint c2 = solver.makeConstraint(0.594, 0.594);
    c2.setCoefficient(x0, 1);
    c2.setCoefficient(x1, 1.01);
    c2.setCoefficient(x2, -0.99);

    System.out.println("Number of variables = " + solver.numVariables());
    System.out.println("Number of constraints = " + solver.numConstraints());

    solver.enableOutput();
    System.out.println(solver.exportModelAsLpFormat());
    System.out.println(solver.solve());
  }

  @Test
  public void testMPSolver_setHintAndSolverGetters() {
    final MPSolver.OptimizationProblemType problemType =
        MPSolver.OptimizationProblemType.GLOP_LINEAR_PROGRAMMING;
    if (!MPSolver.supportsProblemType(problemType)) {
      return;
    }
    final MPSolver solver = new MPSolver("glop", problemType);
    assertNotNull(solver);

    // x and y are continuous non-negative variables.
    final MPVariable x = solver.makeIntVar(0.0, Double.POSITIVE_INFINITY, "x");
    final MPVariable y = solver.makeIntVar(0.0, Double.POSITIVE_INFINITY, "y");

    // Objectif function: Maximize x + 10 * y.
    final MPObjective objective = solver.objective();
    objective.setCoefficient(x, 1);
    objective.setCoefficient(y, 10);
    objective.setMaximization();

    // x + 7 * y <= 17.5.
    final MPConstraint c0 = solver.makeConstraint(-Double.POSITIVE_INFINITY, 17.5, "c0");
    c0.setCoefficient(x, 1);
    c0.setCoefficient(y, 7);

    // x <= 3.5.
    final MPConstraint c1 = solver.makeConstraint(-Double.POSITIVE_INFINITY, 3.5, "c1");
    c1.setCoefficient(x, 1);
    c1.setCoefficient(y, 0);

    // Test solver getters.
    final MPVariable[] variables = solver.variables();
    assertThat(variables).hasLength(2);
    final MPConstraint[] constraints = solver.constraints();
    assertThat(constraints).hasLength(2);

    // Test API compiles.
    solver.setHint(variables, new double[] {2.0, 3.0});
    assertEquals("y", variables[1].name());
    assertEquals("c0", constraints[0].name());
    // TODO(user): Add API to query the hint.

    assertFalse(solver.setNumThreads(4));
  }
}
