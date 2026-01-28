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
import static org.junit.jupiter.api.Assertions.assertNotNull;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.DecisionStrategyProto;
import com.google.ortools.sat.ElementConstraintProto;
import com.google.ortools.sat.LinearArgumentProto;
import com.google.ortools.util.Domain;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Random;
import java.util.function.Consumer;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Tests the CpSolver java interface. */
public final class CpModelTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testCpModelNewIntVar() throws Exception {
    System.out.println("testCpModelNewIntVar");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newIntVar(0, 10, "x");
    final IntVar y = model.newIntVarFromDomain(Domain.fromValues(new long[] {0, 1, 2, 5}), "y");
    final IntVar z =
        model.newIntVarFromDomain(Domain.fromIntervals(new long[][] {{0, 2}, {5}}), "");
    final BoolVar t = model.newBoolVar("t");
    final IntVar u = model.newConstant(5);

    assertThat(x.getName()).isEqualTo("x");
    assertThat(y.getName()).isEqualTo("y");
    assertThat(z.getName()).isEmpty();
    assertThat(t.getName()).isEqualTo("t");
    assertThat(u.getName()).isEmpty();
    assertThat(x.getDomain().flattenedIntervals()).isEqualTo(new long[] {0, 10});

    assertThat(x.toString()).isEqualTo("x(0..10)");
    assertThat(y.toString()).isEqualTo("y(0..2, 5)");
    assertThat(z.toString()).isEqualTo("var_2(0..2, 5)");
    assertThat(t.toString()).isEqualTo("t(0..1)");
    assertThat(u.toString()).isEqualTo("5");
  }

  @Test
  public void testCpModelNewIntervalVar() throws Exception {
    System.out.println("testCpModelNewIntervalVar");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final int horizon = 100;

    final IntVar startVar = model.newIntVar(0, horizon, "start");
    final int duration = 10;
    final IntervalVar interval = model.newFixedSizeIntervalVar(startVar, duration, "interval");

    final LinearExpr startExpr = interval.getStartExpr();
    assertThat(startExpr.numElements()).isEqualTo(1);
    assertThat(startExpr.getOffset()).isEqualTo(0);
    assertThat(startExpr.getVariableIndex(0)).isEqualTo(startVar.getIndex());
    assertThat(startExpr.getCoefficient(0)).isEqualTo(1);

    final LinearExpr sizeExpr = interval.getSizeExpr();
    assertThat(sizeExpr.numElements()).isEqualTo(0);
    assertThat(sizeExpr.getOffset()).isEqualTo(duration);

    final LinearExpr endExpr = interval.getEndExpr();
    assertThat(endExpr.numElements()).isEqualTo(1);
    assertThat(endExpr.getOffset()).isEqualTo(duration);
    assertThat(endExpr.getVariableIndex(0)).isEqualTo(startVar.getIndex());
    assertThat(endExpr.getCoefficient(0)).isEqualTo(1);
  }

  @Test
  public void testCpModelAddBoolOr() throws Exception {
    System.out.println("testCpModelAddBoolOr");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    final BoolVar z = model.newBoolVar("z");
    model.addBoolOr(new Literal[] {x, y.not(), z});

    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasBoolOr()).isTrue();
    assertThat(model.model().getConstraints(0).getBoolOr().getLiteralsCount()).isEqualTo(3);
  }

  @Test
  public void testCpModelAddAtLeastOne() throws Exception {
    System.out.println("testCpModelAddAtLeastOne");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    final BoolVar z = model.newBoolVar("z");
    model.addAtLeastOne(new Literal[] {x, y.not(), z});

    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasBoolOr()).isTrue();
    assertThat(model.model().getConstraints(0).getBoolOr().getLiteralsCount()).isEqualTo(3);
  }

  @Test
  public void testCpModelAddAtMostOne() throws Exception {
    System.out.println("testCpModelAddAtMostOne");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    final BoolVar z = model.newBoolVar("z");
    model.addAtMostOne(new Literal[] {x, y.not(), z});

    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasAtMostOne()).isTrue();
    assertThat(model.model().getConstraints(0).getAtMostOne().getLiteralsCount()).isEqualTo(3);
  }

  @Test
  public void testCpModelAddExactlyOne() throws Exception {
    System.out.println("testCpModelAddExactlyOne");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    final BoolVar z = model.newBoolVar("z");
    model.addExactlyOne(new Literal[] {x, y.not(), z});

    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasExactlyOne()).isTrue();
    assertThat(model.model().getConstraints(0).getExactlyOne().getLiteralsCount()).isEqualTo(3);
  }

  @Test
  public void testCpModelAddBoolAnd() throws Exception {
    System.out.println("testCpModelAddBoolAnd");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    final BoolVar z = model.newBoolVar("z");
    model.addBoolAnd(new Literal[] {x, y.not(), z});

    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasBoolAnd()).isTrue();
    assertThat(model.model().getConstraints(0).getBoolAnd().getLiteralsCount()).isEqualTo(3);
  }

  @Test
  public void testCpModelAddBoolXor() throws Exception {
    System.out.println("testCpModelAddBoolXor");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    final BoolVar z = model.newBoolVar("z");
    model.addBoolXor(new Literal[] {x, y.not(), z});

    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasBoolXor()).isTrue();
    assertThat(model.model().getConstraints(0).getBoolXor().getLiteralsCount()).isEqualTo(3);
  }

  @Test
  public void testCpModelAddImplication() throws Exception {
    System.out.println("testCpModelAddImplication");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    model.addImplication(x, y);

    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasBoolOr()).isTrue();
    assertThat(model.model().getConstraints(0).getBoolOr().getLiteralsCount()).isEqualTo(2);
  }

  @Test
  public void testCpModelAddLinear() throws Exception {
    System.out.println("testCpModelAddLinear");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");

    model.addEquality(LinearExpr.newBuilder().add(x).add(y), 1);
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasLinear()).isTrue();
    assertThat(model.model().getConstraints(0).getLinear().getVarsCount()).isEqualTo(2);

    final BoolVar b = model.newBoolVar("b");
    model.addEquality(LinearExpr.newBuilder().add(x).add(y), 2).onlyEnforceIf(b.not());
    assertThat(model.model().getConstraintsCount()).isEqualTo(2);
    assertThat(model.model().getConstraints(1).hasLinear()).isTrue();
    assertThat(model.model().getConstraints(1).getEnforcementLiteralCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(1).getEnforcementLiteral(0)).isEqualTo(-3);

    final BoolVar c = model.newBoolVar("c");
    model.addEquality(LinearExpr.newBuilder().add(x).add(y), 3).onlyEnforceIf(new Literal[] {b, c});
    assertThat(model.model().getConstraintsCount()).isEqualTo(3);
    assertThat(model.model().getConstraints(2).hasLinear()).isTrue();
    assertThat(model.model().getConstraints(2).getEnforcementLiteralCount()).isEqualTo(2);
    assertThat(model.model().getConstraints(2).getEnforcementLiteral(0)).isEqualTo(2);
    assertThat(model.model().getConstraints(2).getEnforcementLiteral(1)).isEqualTo(3);
  }

  @Test
  public void testLinearExprAddEqualityLiteral() {
    System.out.println("testLinearExprAddEqualityLiteral");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Literal x = model.newBoolVar("x");
    final Literal y = model.newBoolVar("y").not();

    model.addEquality(x, y);
  }

  @Test
  public void testCpModelAddElement() throws Exception {
    System.out.println("testCpModelAddElement");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newIntVar(0, 10, "x");
    final IntVar y = model.newIntVar(0, 10, "y");
    final IntVar z = model.newIntVar(0, 10, "z");
    final IntVar t = model.newIntVar(0, 10, "t");

    model.addElement(z, new IntVar[] {x, y}, t);
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasElement()).isTrue();
    ElementConstraintProto ct = model.model().getConstraints(0).getElement();
    assertThat(ct.getLinearIndex().getVarsCount()).isEqualTo(1);
    assertThat(ct.getLinearIndex().getVars(0)).isEqualTo(2);
    assertThat(ct.getLinearIndex().getCoeffs(0)).isEqualTo(1);
    assertThat(ct.getExprsCount()).isEqualTo(2);
    assertThat(ct.getExprs(0).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(0).getVars(0)).isEqualTo(0);
    assertThat(ct.getExprs(0).getCoeffs(0)).isEqualTo(1);
    assertThat(ct.getExprs(1).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(1).getVars(0)).isEqualTo(1);
    assertThat(ct.getExprs(1).getCoeffs(0)).isEqualTo(1);
    assertThat(ct.getLinearTarget().getVarsCount()).isEqualTo(1);
    assertThat(ct.getLinearTarget().getVars(0)).isEqualTo(3);
    assertThat(ct.getLinearTarget().getCoeffs(0)).isEqualTo(1);
  }

  @Test
  public void testCpModelAddLinearArgumentElement() throws Exception {
    System.out.println("testCpModelAddLinearArgumentElement");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newIntVar(0, 10, "x");
    final IntVar y = model.newIntVar(0, 10, "y");
    final IntVar z = model.newIntVar(0, 10, "z");
    final IntVar t = model.newIntVar(0, 10, "t");

    model.addElement(z, new LinearArgument[] {x, y}, t);
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasElement()).isTrue();
    ElementConstraintProto ct = model.model().getConstraints(0).getElement();
    assertThat(ct.getLinearIndex().getVarsCount()).isEqualTo(1);
    assertThat(ct.getLinearIndex().getVars(0)).isEqualTo(2);
    assertThat(ct.getLinearIndex().getCoeffs(0)).isEqualTo(1);
    assertThat(ct.getExprsCount()).isEqualTo(2);
    assertThat(ct.getExprs(0).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(0).getVars(0)).isEqualTo(0);
    assertThat(ct.getExprs(0).getCoeffs(0)).isEqualTo(1);
    assertThat(ct.getExprs(1).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(1).getVars(0)).isEqualTo(1);
    assertThat(ct.getExprs(1).getCoeffs(0)).isEqualTo(1);
    assertThat(ct.getLinearTarget().getVarsCount()).isEqualTo(1);
    assertThat(ct.getLinearTarget().getVars(0)).isEqualTo(3);
    assertThat(ct.getLinearTarget().getCoeffs(0)).isEqualTo(1);
  }

  @Test
  public void testCpModelAddExprElement() throws Exception {
    System.out.println("testCpModelExceptionVisibility");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newIntVar(0, 10, "x");
    final IntVar y = model.newIntVar(0, 10, "y");
    final IntVar z = model.newIntVar(0, 10, "z");
    final IntVar t = model.newIntVar(0, 10, "t");

    model.addElement(LinearExpr.newBuilder().addTerm(z, 2).add(-1),
        new LinearArgument[] {LinearExpr.constant(3), x, LinearExpr.affine(y, -1, 5)}, t);
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasElement()).isTrue();
    ElementConstraintProto ct = model.model().getConstraints(0).getElement();
    assertThat(ct.getLinearIndex().getVarsCount()).isEqualTo(1);
    assertThat(ct.getLinearIndex().getVars(0)).isEqualTo(2);
    assertThat(ct.getLinearIndex().getCoeffs(0)).isEqualTo(2);
    assertThat(ct.getLinearIndex().getOffset()).isEqualTo(-1);
    assertThat(ct.getExprsCount()).isEqualTo(3);
    assertThat(ct.getExprs(0).getVarsCount()).isEqualTo(0);
    assertThat(ct.getExprs(0).getOffset()).isEqualTo(3);
    assertThat(ct.getExprs(1).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(1).getVars(0)).isEqualTo(0);
    assertThat(ct.getExprs(1).getCoeffs(0)).isEqualTo(1);
    assertThat(ct.getExprs(2).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(2).getVars(0)).isEqualTo(1);
    assertThat(ct.getExprs(2).getCoeffs(0)).isEqualTo(-1);
    assertThat(ct.getExprs(2).getOffset()).isEqualTo(5);
    assertThat(ct.getLinearTarget().getVarsCount()).isEqualTo(1);
    assertThat(ct.getLinearTarget().getVars(0)).isEqualTo(3);
    assertThat(ct.getLinearTarget().getCoeffs(0)).isEqualTo(1);
  }

  @Test
  public void testCpModelAddConstantElement() throws Exception {
    System.out.println("testCpModelAddConstantElement");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar z = model.newIntVar(0, 10, "x");
    final IntVar t = model.newIntVar(0, 10, "t");

    model.addElement(LinearExpr.newBuilder().addTerm(z, 2).add(-1), new long[] {3, 5, -7, 2}, t);
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasElement()).isTrue();
    ElementConstraintProto ct = model.model().getConstraints(0).getElement();
    assertThat(ct.getLinearIndex().getVarsCount()).isEqualTo(1);
    assertThat(ct.getLinearIndex().getVars(0)).isEqualTo(0);
    assertThat(ct.getLinearIndex().getCoeffs(0)).isEqualTo(2);
    assertThat(ct.getLinearIndex().getOffset()).isEqualTo(-1);
    assertThat(ct.getExprsCount()).isEqualTo(4);
    assertThat(ct.getExprs(0).getVarsCount()).isEqualTo(0);
    assertThat(ct.getExprs(0).getOffset()).isEqualTo(3);
    assertThat(ct.getExprs(1).getVarsCount()).isEqualTo(0);
    assertThat(ct.getExprs(1).getOffset()).isEqualTo(5);
    assertThat(ct.getExprs(2).getVarsCount()).isEqualTo(0);
    assertThat(ct.getExprs(2).getOffset()).isEqualTo(-7);
    assertThat(ct.getExprs(3).getVarsCount()).isEqualTo(0);
    assertThat(ct.getExprs(3).getOffset()).isEqualTo(2);
    assertThat(ct.getLinearTarget().getVarsCount()).isEqualTo(1);
    assertThat(ct.getLinearTarget().getVars(0)).isEqualTo(1);
    assertThat(ct.getLinearTarget().getCoeffs(0)).isEqualTo(1);
    assertThat(ct.getLinearTarget().getOffset()).isEqualTo(0);
  }

  @Test
  public void testCpModelAddMinEquality() throws Exception {
    System.out.println("testCpModelAddMinEquality");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    final BoolVar t = model.newBoolVar("t");

    model.addMinEquality(t, new IntVar[] {x, y});
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasLinMax()).isTrue();
    LinearArgumentProto ct = model.model().getConstraints(0).getLinMax();
    assertThat(ct.getTarget().getVarsCount()).isEqualTo(1);
    assertThat(ct.getTarget().getVars(0)).isEqualTo(2);
    assertThat(ct.getTarget().getCoeffs(0)).isEqualTo(-1);
    assertThat(ct.getExprsCount()).isEqualTo(2);
    assertThat(ct.getExprs(0).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(0).getVars(0)).isEqualTo(0);
    assertThat(ct.getExprs(0).getCoeffs(0)).isEqualTo(-1);
    assertThat(ct.getExprs(1).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(1).getVars(0)).isEqualTo(1);
    assertThat(ct.getExprs(1).getCoeffs(0)).isEqualTo(-1);
  }

  @Test
  public void testCpModelAddMaxEquality() throws Exception {
    System.out.println("testCpModelAddMaxEquality");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    final BoolVar t = model.newBoolVar("t");

    model.addMaxEquality(t, new IntVar[] {x, y});
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasLinMax()).isTrue();
    LinearArgumentProto ct = model.model().getConstraints(0).getLinMax();
    assertThat(ct.getTarget().getVarsCount()).isEqualTo(1);
    assertThat(ct.getTarget().getVars(0)).isEqualTo(2);
    assertThat(ct.getTarget().getCoeffs(0)).isEqualTo(1);
    assertThat(ct.getExprsCount()).isEqualTo(2);
    assertThat(ct.getExprs(0).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(0).getVars(0)).isEqualTo(0);
    assertThat(ct.getExprs(0).getCoeffs(0)).isEqualTo(1);
    assertThat(ct.getExprs(1).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(1).getVars(0)).isEqualTo(1);
    assertThat(ct.getExprs(1).getCoeffs(0)).isEqualTo(1);
  }

  @Test
  public void testCpModelAddMinExprEquality() throws Exception {
    System.out.println("testCpModelAddMinExprEquality");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newBoolVar("x");
    final IntVar t = model.newBoolVar("t");

    model.addMinEquality(LinearExpr.newBuilder().addTerm(t, -3),
        new LinearExpr[] {
            LinearExpr.newBuilder().addTerm(x, 2).add(1).build(), LinearExpr.constant(5)});
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasLinMax()).isTrue();
    LinearArgumentProto ct = model.model().getConstraints(0).getLinMax();
    assertThat(ct.getTarget().getVarsCount()).isEqualTo(1);
    assertThat(ct.getTarget().getVars(0)).isEqualTo(1);
    assertThat(ct.getTarget().getCoeffs(0)).isEqualTo(3);
    assertThat(ct.getExprsCount()).isEqualTo(2);
    assertThat(ct.getExprs(0).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(0).getVars(0)).isEqualTo(0);
    assertThat(ct.getExprs(0).getCoeffs(0)).isEqualTo(-2);
    assertThat(ct.getExprs(0).getOffset()).isEqualTo(-1);
    assertThat(ct.getExprs(1).getVarsCount()).isEqualTo(0);
    assertThat(ct.getExprs(1).getOffset()).isEqualTo(-5);
  }

  @Test
  public void testCpModelAddAbsEquality() throws Exception {
    System.out.println("testCpModelAddAbsEquality");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newBoolVar("x");
    final IntVar t = model.newBoolVar("t");

    model.addAbsEquality(
        LinearExpr.newBuilder().addTerm(t, -3), LinearExpr.newBuilder().addTerm(x, 2).add(1));
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasLinMax()).isTrue();
    LinearArgumentProto ct = model.model().getConstraints(0).getLinMax();
    assertThat(ct.getTarget().getVarsCount()).isEqualTo(1);
    assertThat(ct.getTarget().getVars(0)).isEqualTo(1);
    assertThat(ct.getTarget().getCoeffs(0)).isEqualTo(-3);
    assertThat(ct.getExprsCount()).isEqualTo(2);
    assertThat(ct.getExprs(0).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(0).getVars(0)).isEqualTo(0);
    assertThat(ct.getExprs(0).getCoeffs(0)).isEqualTo(2);
    assertThat(ct.getExprs(0).getOffset()).isEqualTo(1);
    assertThat(ct.getExprs(1).getVarsCount()).isEqualTo(1);
    assertThat(ct.getExprs(1).getVars(0)).isEqualTo(0);
    assertThat(ct.getExprs(1).getCoeffs(0)).isEqualTo(-2);
    assertThat(ct.getExprs(1).getOffset()).isEqualTo(-1);
  }

  @Test
  public void testCpModelAddCircuit() throws Exception {
    System.out.println("testCpModelAddCircuit");
    final CpModel model = new CpModel();
    assertNotNull(model);

    final Literal x1 = model.newBoolVar("x1");
    final Literal x2 = model.newBoolVar("x2");
    final Literal x3 = model.newBoolVar("x3");

    CircuitConstraint circuit = model.addCircuit();
    circuit.addArc(0, 1, x1);
    circuit.addArc(1, 2, x2.not());
    circuit.addArc(2, 0, x3);
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasCircuit()).isTrue();
    assertThat(model.model().getConstraints(0).getCircuit().getTailsCount()).isEqualTo(3);
    assertThat(model.model().getConstraints(0).getCircuit().getHeadsCount()).isEqualTo(3);
    assertThat(model.model().getConstraints(0).getCircuit().getLiteralsCount()).isEqualTo(3);
  }

  @Test
  public void testCpModelAddMultipleCircuit() throws Exception {
    System.out.println("testCpModelAddMultipleCircuit");
    final CpModel model = new CpModel();
    assertNotNull(model);

    final Literal x1 = model.newBoolVar("x1");
    final Literal x2 = model.newBoolVar("x2");
    final Literal x3 = model.newBoolVar("x3");

    MultipleCircuitConstraint circuit = model.addMultipleCircuit();
    circuit.addArc(0, 1, x1);
    circuit.addArc(1, 2, x2.not());
    circuit.addArc(2, 0, x3);
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasRoutes()).isTrue();
    assertThat(model.model().getConstraints(0).getRoutes().getTailsCount()).isEqualTo(3);
    assertThat(model.model().getConstraints(0).getRoutes().getHeadsCount()).isEqualTo(3);
    assertThat(model.model().getConstraints(0).getRoutes().getLiteralsCount()).isEqualTo(3);
  }

  @Test
  public void testCpModelAddAutomaton() throws Exception {
    System.out.println("testCpModelAddAutomaton");
    final CpModel model = new CpModel();
    assertNotNull(model);

    final IntVar x1 = model.newIntVar(0, 5, "x1");
    final IntVar x2 = model.newIntVar(0, 5, "x2");
    final IntVar x3 = model.newIntVar(0, 5, "x3");

    AutomatonConstraint automaton =
        model.addAutomaton(new IntVar[] {x1, x2, x3}, 0, new long[] {1, 2});
    automaton.addTransition(0, 1, 0);
    automaton.addTransition(1, 1, 1);
    automaton.addTransition(1, 2, 2);
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasAutomaton()).isTrue();
    assertThat(model.model().getConstraints(0).getAutomaton().getExprsCount()).isEqualTo(3);
    assertThat(model.model().getConstraints(0).getAutomaton().getTransitionTailCount())
        .isEqualTo(3);
    assertThat(model.model().getConstraints(0).getAutomaton().getTransitionHeadCount())
        .isEqualTo(3);
    assertThat(model.model().getConstraints(0).getAutomaton().getTransitionLabelCount())
        .isEqualTo(3);
    assertThat(model.model().getConstraints(0).getAutomaton().getStartingState()).isEqualTo(0);

    assertThat(model.model().getConstraints(0).getAutomaton().getFinalStatesCount()).isEqualTo(2);
  }

  @Test
  public void testCpModelAddAutomatonLinearArgument() throws Exception {
    System.out.println("testCpModelAddAutomaton");
    final CpModel model = new CpModel();
    assertNotNull(model);

    final IntVar x1 = model.newIntVar(0, 5, "x1");
    final IntVar x2 = model.newIntVar(0, 5, "x2");
    final IntVar x3 = model.newIntVar(0, 5, "x3");

    AutomatonConstraint automaton = model.addAutomaton(
        new LinearArgument[] {x1, x2, LinearExpr.constant(1), LinearExpr.affine(x3, -1, 2)}, 0,
        new long[] {1, 2});
    automaton.addTransition(0, 1, 0);
    automaton.addTransition(1, 1, 1);
    automaton.addTransition(1, 2, 2);
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasAutomaton()).isTrue();
    assertThat(model.model().getConstraints(0).getAutomaton().getExprsCount()).isEqualTo(4);
    assertThat(model.model().getConstraints(0).getAutomaton().getTransitionTailCount())
        .isEqualTo(3);
    assertThat(model.model().getConstraints(0).getAutomaton().getTransitionHeadCount())
        .isEqualTo(3);
    assertThat(model.model().getConstraints(0).getAutomaton().getTransitionLabelCount())
        .isEqualTo(3);
    assertThat(model.model().getConstraints(0).getAutomaton().getStartingState()).isEqualTo(0);

    assertThat(model.model().getConstraints(0).getAutomaton().getFinalStatesCount()).isEqualTo(2);
  }

  @Test
  public void testCpModelAddNoOverlap() throws Exception {
    System.out.println("testCpModelAddNoOverlap");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final int horizon = 100;

    final IntVar startVar1 = model.newIntVar(0, horizon, "start1");
    final int duration1 = 10;
    final IntervalVar interval1 = model.newFixedSizeIntervalVar(startVar1, duration1, "interval1");

    final IntVar startVar2 = model.newIntVar(0, horizon, "start2");
    final int duration2 = 15;
    final IntervalVar interval2 = model.newFixedSizeIntervalVar(startVar2, duration2, "interval2");

    model.addNoOverlap(new IntervalVar[] {interval1, interval2});
    assertThat(model.model().getConstraintsCount()).isEqualTo(3);
    assertThat(model.model().getConstraints(0).hasInterval()).isTrue();
    assertThat(model.model().getConstraints(1).hasInterval()).isTrue();
    assertThat(model.model().getConstraints(2).hasNoOverlap()).isTrue();
    assertThat(model.model().getConstraints(2).getNoOverlap().getIntervalsCount()).isEqualTo(2);
  }

  @Test
  public void testCpModelAddCumulative() throws Exception {
    System.out.println("testCpModelAddCumulative");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final int horizon = 100;

    final IntVar startVar1 = model.newIntVar(0, horizon, "start1");
    final int duration1 = 10;
    final int demand1 = 20;
    final IntervalVar interval1 = model.newFixedSizeIntervalVar(startVar1, duration1, "interval1");

    final IntVar startVar2 = model.newIntVar(0, horizon, "start2");
    final IntVar demandVar2 = model.newIntVar(2, 5, "demand2");
    final int duration2 = 15;
    final IntervalVar interval2 = model.newFixedSizeIntervalVar(startVar2, duration2, "interval2");

    CumulativeConstraint cumul = model.addCumulative(13);
    cumul.addDemand(interval1, demand1);
    cumul.addDemand(interval2, demandVar2);

    assertThat(model.model().getConstraintsCount()).isEqualTo(3);
    assertThat(model.model().getConstraints(0).hasInterval()).isTrue();
    assertThat(model.model().getConstraints(1).hasInterval()).isTrue();
    assertThat(model.model().getConstraints(2).hasCumulative()).isTrue();
    assertThat(model.model().getConstraints(2).getCumulative().getIntervalsCount()).isEqualTo(2);

    cumul.addDemands(new IntervalVar[] {interval1}, new int[] {2});
    cumul.addDemands(new IntervalVar[] {interval1}, new long[] {2});
    cumul.addDemands(
        new IntervalVar[] {interval2}, new LinearArgument[] {LinearExpr.affine(demandVar2, 1, 2)});
    cumul.addDemands(
        new IntervalVar[] {interval2}, new LinearExpr[] {LinearExpr.affine(demandVar2, 1, 3)});
    assertThat(model.model().getConstraints(2).getCumulative().getIntervalsCount()).isEqualTo(6);
  }

  @Test
  public void testCpModelAddNoOverlap2D() throws Exception {
    System.out.println("testCpModelAddNoOverlap2D");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final int horizon = 100;

    final IntVar startVar1 = model.newIntVar(0, horizon, "start1");
    final int duration1 = 10;
    final IntervalVar interval1 = model.newFixedSizeIntervalVar(startVar1, duration1, "interval1");

    final IntVar startVar2 = model.newIntVar(0, horizon, "start2");
    final int duration2 = 15;
    final IntervalVar interval2 = model.newFixedSizeIntervalVar(startVar2, duration2, "interval2");

    NoOverlap2dConstraint ct = model.addNoOverlap2D();
    ct.addRectangle(interval1, interval1);
    ct.addRectangle(interval2, interval2);
    assertThat(model.model().getConstraintsCount()).isEqualTo(3);
    assertThat(model.model().getConstraints(0).hasInterval()).isTrue();
    assertThat(model.model().getConstraints(1).hasInterval()).isTrue();
    assertThat(model.model().getConstraints(2).hasNoOverlap2D()).isTrue();

    assertThat(model.model().getConstraints(2).getNoOverlap2D().getXIntervalsCount()).isEqualTo(2);

    assertThat(model.model().getConstraints(2).getNoOverlap2D().getYIntervalsCount()).isEqualTo(2);
  }

  @Test
  public void testCpModelAddDecisionStrategy() throws Exception {
    System.out.println("testCpModelAddDecisionStrategy");
    final CpModel model = new CpModel();
    assertNotNull(model);

    final Literal x1 = model.newBoolVar("x1");
    final Literal x2 = model.newBoolVar("x2");
    final IntVar x3 = model.newIntVar(0, 2, "x3");

    model.addDecisionStrategy(new LinearArgument[] {x1, x2.not(), x3},
        DecisionStrategyProto.VariableSelectionStrategy.CHOOSE_FIRST,
        DecisionStrategyProto.DomainReductionStrategy.SELECT_MIN_VALUE);

    assertThat(model.model().getSearchStrategyCount()).isEqualTo(1);
    assertThat(model.model().getSearchStrategy(0).getExprsCount()).isEqualTo(3);
    assertThat(model.model().getSearchStrategy(0).getExprs(0).getVarsCount()).isEqualTo(1);
    assertThat(model.model().getSearchStrategy(0).getExprs(0).getCoeffsCount()).isEqualTo(1);

    assertThat(model.model().getSearchStrategy(0).getExprs(0).getVars(0)).isEqualTo(x1.getIndex());
    assertThat(model.model().getSearchStrategy(0).getExprs(0).getCoeffs(0)).isEqualTo(1);
    assertThat(model.model().getSearchStrategy(0).getExprs(0).getOffset()).isEqualTo(0);

    assertThat(model.model().getSearchStrategy(0).getExprs(1).getVarsCount()).isEqualTo(1);
    assertThat(model.model().getSearchStrategy(0).getExprs(1).getCoeffsCount()).isEqualTo(1);

    assertThat(model.model().getSearchStrategy(0).getExprs(1).getVars(0)).isEqualTo(x2.getIndex());
    assertThat(model.model().getSearchStrategy(0).getExprs(1).getCoeffs(0)).isEqualTo(-1);
    assertThat(model.model().getSearchStrategy(0).getExprs(1).getOffset()).isEqualTo(1);

    assertThat(model.model().getSearchStrategy(0).getExprs(2).getVarsCount()).isEqualTo(1);
    assertThat(model.model().getSearchStrategy(0).getExprs(2).getCoeffsCount()).isEqualTo(1);

    assertThat(model.model().getSearchStrategy(0).getExprs(2).getVars(0)).isEqualTo(x3.getIndex());
    assertThat(model.model().getSearchStrategy(0).getExprs(2).getCoeffs(0)).isEqualTo(1);
    assertThat(model.model().getSearchStrategy(0).getExprs(2).getOffset()).isEqualTo(0);
  }

  @Test
  public void testCpModelModelStats() throws Exception {
    System.out.println("testCpModelModelStats");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    model.addImplication(x, y);

    String stats = model.modelStats();
    assertThat(stats).isNotEmpty();
  }

  @Test
  public void testCpModelValidateOk() throws Exception {
    System.out.println("testCpModelValidateOk");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    model.addImplication(x, y);

    String stats = model.validate();
    assertThat(stats).isEmpty();
  }

  @Test
  public void testCpModelValidateNotOk() throws Exception {
    System.out.println("testCpModelValidateNotOk");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newIntVar(0, 9223372036854775807L, "x");
    final IntVar y = model.newIntVar(0, 10, "y");
    model.addLinearExpressionInDomain(LinearExpr.newBuilder().add(x).add(y),
        Domain.fromFlatIntervals(new long[] {6, 9223372036854775807L}));

    String stats = model.validate();
    assertThat(stats).isNotEmpty();
  }

  @Test
  public void testCpModelExceptionVisibility() throws Exception {
    System.out.println("testCpModelExceptionVisibility");
    CpModel.MismatchedArrayLengths ex1 = new CpModel.MismatchedArrayLengths("test1", "ar1", "ar2");
    CpModel.WrongLength ex2 = new CpModel.WrongLength("test2", "ar");
    assertThat(ex1).hasMessageThat().isEqualTo("test1: ar1 and ar2 have mismatched lengths");
    assertThat(ex2).hasMessageThat().isEqualTo("test2: ar");
  }

  @Test
  public void testCpModelClearConstraint() throws Exception {
    System.out.println("testCpModelClearConstraint");
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newIntVar(0, 92, "x");
    final IntVar y = model.newIntVar(0, 10, "y");
    List<Constraint> constraints = new ArrayList<>();
    for (int i = 0; i < 10; ++i) {
      Constraint ct = model.addLinearExpressionInDomain(LinearExpr.newBuilder().add(x).add(y),
          Domain.fromFlatIntervals(new long[] {6 + i, 92 - i}));
      constraints.add(ct);
    }

    for (int i = 0; i < 10; ++i) {
      Constraint ct = constraints.get(i);
      assertThat(ct.getBuilder().toString())
          .isEqualTo(model.getBuilder().getConstraintsBuilder(i).toString());
      assertThat(ct.getBuilder().hasLinear()).isTrue();
      assertThat(model.getBuilder().getConstraintsBuilder(i).hasLinear()).isTrue();
      assertThat(ct.getBuilder().toString())
          .isEqualTo(model.getBuilder().getConstraintsBuilder(i).toString());
    }

    for (int i = 0; i < 10; ++i) {
      Constraint ct = constraints.get(i);
      ct.getBuilder().clear();
    }

    for (int i = 0; i < 10; ++i) {
      Constraint ct = constraints.get(i);
      assertThat(ct.getBuilder().hasLinear()).isFalse();
      assertThat(model.getBuilder().getConstraintsBuilder(i).hasLinear()).isFalse();
    }
  }

  @Test
  public void testCpModelMinimize() throws Exception {
    System.out.println("testCpModelMinimize");
    final CpModel model = new CpModel();
    assertNotNull(model);

    final IntVar x1 = model.newIntVar(0, 5, "x1");
    final IntVar x2 = model.newIntVar(0, 5, "x2");
    final IntVar x3 = model.newIntVar(0, 5, "x3");

    model.minimize(LinearExpr.sum(new IntVar[] {x1, x2}));
    assertThat(model.getBuilder().getObjectiveBuilder().getVarsCount()).isEqualTo(2);
    assertThat(model.getBuilder().hasFloatingPointObjective()).isFalse();

    model.minimize(x3);
    assertThat(model.getBuilder().getObjectiveBuilder().getVarsCount()).isEqualTo(1);
    assertThat(model.getBuilder().hasFloatingPointObjective()).isFalse();

    model.minimize(LinearExpr.sum(new IntVar[] {x1, x2}));
    assertThat(model.getBuilder().getObjectiveBuilder().getVarsCount()).isEqualTo(2);
    assertThat(model.getBuilder().hasFloatingPointObjective()).isFalse();

    model.minimize(DoubleLinearExpr.weightedSum(new IntVar[] {x1, x2}, new double[] {2.5, 3.5}));

    assertThat(model.getBuilder().getFloatingPointObjectiveBuilder().getVarsCount()).isEqualTo(2);
    assertThat(model.getBuilder().hasObjective()).isFalse();

    model.minimize(DoubleLinearExpr.affine(x3, 1.4, 1.2));

    assertThat(model.getBuilder().getFloatingPointObjectiveBuilder().getVarsCount()).isEqualTo(1);
    assertThat(model.getBuilder().hasObjective()).isFalse();

    model.maximize(LinearExpr.sum(new IntVar[] {x1, x2}));
    assertThat(model.getBuilder().getObjectiveBuilder().getVarsCount()).isEqualTo(2);
    assertThat(model.getBuilder().hasFloatingPointObjective()).isFalse();

    model.maximize(x3);
    assertThat(model.getBuilder().getObjectiveBuilder().getVarsCount()).isEqualTo(1);
    assertThat(model.getBuilder().hasFloatingPointObjective()).isFalse();

    model.maximize(DoubleLinearExpr.weightedSum(new IntVar[] {x1, x2}, new double[] {2.5, 3.5}));

    assertThat(model.getBuilder().getFloatingPointObjectiveBuilder().getVarsCount()).isEqualTo(2);
    assertThat(model.getBuilder().hasObjective()).isFalse();

    model.maximize(DoubleLinearExpr.affine(x3, 1.4, 1.2));

    assertThat(model.getBuilder().getFloatingPointObjectiveBuilder().getVarsCount()).isEqualTo(1);
    assertThat(model.getBuilder().hasObjective()).isFalse();
  }

  @Test
  public void testDomainGetter() {
    System.out.println("testDomainGetter");
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
    System.out.println("testCrashInPresolve");
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
    CpSolverStatus status = solver.solve(model);

    if (status != CpSolverStatus.INFEASIBLE) {
      throw new IllegalStateException("Wrong status in testCrashInPresolve");
    }
  }

  @Test
  public void testCrashInSolveWithAllowedAssignment() {
    System.out.println("testCrashInSolveWithAllowedAssignment");
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
    TableConstraint table = model.addAllowedAssignments(entitiesOne);
    final int[] oneTuple = new int[entitiesOne.length];
    for (int i = 0; i < numEntityTwo; i++) {
      for (int j = 0; j < entitiesOne.length; j++) {
        oneTuple[j] = i;
      }
      table.addTuple(oneTuple);
    }
    final Random r = new Random();
    for (int i = 0; i < entitiesOne.length; i++) {
      model.addEquality(entitiesOne[i], r.nextInt(numEntityTwo));
    }
    final CpSolver solver = new CpSolver();
    CpSolverStatus unused = solver.solve(model);
  }

  @Test
  public void testCrashEquality() {
    System.out.println("testCrashEquality");
    final CpModel model = new CpModel();

    final IntVar[] entities = new IntVar[20];
    Arrays.setAll(entities, i -> model.newIntVar(1, 5, "E" + i));

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
    solver.getParameters().setLogToStdout(true);
    CpSolverStatus unused = solver.solve(model);
    System.out.println(unused);
  }

  @Test
  public void testLogCapture() {
    System.out.println("testLogCapture");

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
      System.out.println("Current Thread Name:" + Thread.currentThread().getName()
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
      final int value = allowedAssignmentValues[i];
      Arrays.fill(allAllowedValues[i], 0, allowedAssignments.length, value);
    }
    final IntVar[] specificEntities = new IntVar[allowedAssignments.length];
    for (int i = 0; i < allowedAssignments.length; i++) {
      specificEntities[i] = entities[allowedAssignments[i]];
    }
    TableConstraint table = model.addAllowedAssignments(specificEntities);
    for (int[] tuple : allAllowedValues) {
      table.addTuple(tuple);
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
      final int value = forbiddenAssignmentsValues[i];
      Arrays.fill(notAllowedValues[i], 0, forbiddenAssignments.length, value);
    }
    TableConstraint table = model.addForbiddenAssignments(specificEntities);
    for (int[] tuple : notAllowedValues) {
      table.addTuple(tuple);
    }
  }

  @Test
  public void testAddInt() {
    System.out.println("testAddInt");
    CpModel model = new CpModel();

    // Create decision variables
    IntVar x = model.newIntVar(0, 5, "x");
    IntVar y = model.newIntVar(0, 5, "y");

    model.addHint(y, 2);
    model.addHint(x, 3);

    assertThat(model.model().getSolutionHint().getVarsCount()).isEqualTo(2);
    assertThat(model.model().getSolutionHint().getValuesCount()).isEqualTo(2);
    assertThat(model.model().getSolutionHint().getVars(0)).isEqualTo(y.getIndex());
    assertThat(model.model().getSolutionHint().getValues(0)).isEqualTo(2);
    assertThat(model.model().getSolutionHint().getVars(1)).isEqualTo(x.getIndex());
    assertThat(model.model().getSolutionHint().getValues(1)).isEqualTo(3);
  }

  @Test
  public void testAddBooleanInt() {
    System.out.println("testAddBooleanInt");
    CpModel model = new CpModel();

    // Create decision variables
    Literal x = model.newBoolVar("x");
    Literal y = model.newBoolVar("y");

    model.addHint(y, true);
    model.addHint(x.not(), true);

    assertThat(model.model().getSolutionHint().getVarsCount()).isEqualTo(2);
    assertThat(model.model().getSolutionHint().getValuesCount()).isEqualTo(2);
    assertThat(model.model().getSolutionHint().getVars(0)).isEqualTo(y.getIndex());
    assertThat(model.model().getSolutionHint().getValues(0)).isEqualTo(1);
    assertThat(model.model().getSolutionHint().getVars(1)).isEqualTo(x.getIndex());
    assertThat(model.model().getSolutionHint().getValues(1)).isEqualTo(0);
  }
}
