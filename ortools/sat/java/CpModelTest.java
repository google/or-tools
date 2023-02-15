// Copyright 2010-2022 Google LLC
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
import com.google.ortools.sat.LinearArgumentProto;
import com.google.ortools.util.Domain;
import java.util.ArrayList;
import java.util.List;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Tests the CpSolver java interface. */
public final class CpModelTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testCpModel_newIntVar() throws Exception {
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
  public void testCpModel_newIntervalVar() throws Exception {
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
  public void testCpModel_addBoolOr() throws Exception {
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
  public void testCpModel_addAtLeastOne() throws Exception {
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
  public void testCpModel_addAtMostOne() throws Exception {
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
  public void testCpModel_addExactlyOne() throws Exception {
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
  public void testCpModel_addBoolAnd() throws Exception {
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
  public void testCpModel_addBoolXor() throws Exception {
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
  public void testCpModel_addImplication() throws Exception {
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
  public void testCpModel_addLinear() throws Exception {
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
  public void testLinearExpr_addEquality_literal() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Literal x = model.newBoolVar("x");
    final Literal y = model.newBoolVar("y").not();

    model.addEquality(x, y);
  }

  @Test
  public void testCpModel_addMinEquality() throws Exception {
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
  public void testCpModel_addMaxEquality() throws Exception {
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
  public void testCpModel_addMinExprEquality() throws Exception {
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
  public void testCpModel_addAbsEquality() throws Exception {
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
  public void testCpModel_addCircuit() throws Exception {
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
  public void testCpModel_addMultipleCircuit() throws Exception {
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
  public void testCpModel_addAutomaton() throws Exception {
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
    assertThat(model.model().getConstraints(0).hasAutomaton()).isTrue();
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
  public void testCpModel_addNoOverlap() throws Exception {
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
  public void testCpModel_addCumulative() throws Exception {
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
  public void testCpModel_addNoOverlap2D() throws Exception {
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
  public void testCpModel_modelStats() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    model.addImplication(x, y);

    String stats = model.modelStats();
    assertThat(stats).isNotEmpty();
  }

  @Test
  public void testCpModel_validateOk() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar x = model.newBoolVar("x");
    final BoolVar y = model.newBoolVar("y");
    model.addImplication(x, y);

    String stats = model.validate();
    assertThat(stats).isEmpty();
  }

  @Test
  public void testCpModel_validateNotOk() throws Exception {
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
  public void testCpModel_exceptionVisibility() throws Exception {
    CpModel.MismatchedArrayLengths ex1 = new CpModel.MismatchedArrayLengths("test1", "ar1", "ar2");
    CpModel.WrongLength ex2 = new CpModel.WrongLength("test2", "ar");
    assertThat(ex1).hasMessageThat().isEqualTo("test1: ar1 and ar2 have mismatched lengths");
    assertThat(ex2).hasMessageThat().isEqualTo("test2: ar");
  }

  @Test
  public void testCpModel_clearConstraint() throws Exception {
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
  public void testCpModel_minimize() throws Exception {
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
}
