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
import static org.junit.jupiter.api.Assertions.assertNotNull;

import com.google.ortools.Loader;
import com.google.ortools.util.Domain;
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
    final IntVar t = model.newBoolVar("t");
    final IntVar u = model.newConstant(5);

    assertThat(x.getName()).isEqualTo("x");
    assertThat(y.getName()).isEqualTo("y");
    assertThat(z.getName()).isEmpty();
    assertThat(t.getName()).isEqualTo("t");
    assertThat(u.getName()).isEmpty();
    assertThat(x.getDomain().flattenedIntervals()).isEqualTo(new long[] {0, 10});

    assertThat(x.getShortString()).isEqualTo("x(0..10)");
    assertThat(y.getShortString()).isEqualTo("y(0..2, 5)");
    assertThat(z.getShortString()).isEqualTo("var_2(0..2, 5)");
    assertThat(t.getShortString()).isEqualTo("t(0..1)");
    assertThat(u.getShortString()).isEqualTo("5");
  }

  @Test
  public void testCpModel_addBoolOr() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newBoolVar("x");
    final IntVar y = model.newBoolVar("y");
    final IntVar z = model.newBoolVar("z");
    model.addBoolOr(new Literal[] {x, y.not(), z});

    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasBoolOr()).isTrue();
    assertThat(model.model().getConstraints(0).getBoolOr().getLiteralsCount()).isEqualTo(3);
  }

  @Test
  public void testCpModel_addBoolAnd() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newBoolVar("x");
    final IntVar y = model.newBoolVar("y");
    final IntVar z = model.newBoolVar("z");
    model.addBoolAnd(new Literal[] {x, y.not(), z});

    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasBoolAnd()).isTrue();
    assertThat(model.model().getConstraints(0).getBoolAnd().getLiteralsCount()).isEqualTo(3);
  }

  @Test
  public void testCpModel_addBoolXor() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newBoolVar("x");
    final IntVar y = model.newBoolVar("y");
    final IntVar z = model.newBoolVar("z");
    model.addBoolXor(new Literal[] {x, y.not(), z});

    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasBoolXor()).isTrue();
    assertThat(model.model().getConstraints(0).getBoolXor().getLiteralsCount()).isEqualTo(3);
  }

  @Test
  public void testCpModel_addImplication() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newBoolVar("x");
    final IntVar y = model.newBoolVar("y");
    model.addImplication(x, y);

    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasBoolOr()).isTrue();
    assertThat(model.model().getConstraints(0).getBoolOr().getLiteralsCount()).isEqualTo(2);
  }

  @Test
  public void testCpModel_addLinear() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newBoolVar("x");
    final IntVar y = model.newBoolVar("y");

    model.addEquality(LinearExpr.sum(new IntVar[] {x, y}), 1);
    assertThat(model.model().getConstraintsCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(0).hasLinear()).isTrue();
    assertThat(model.model().getConstraints(0).getLinear().getVarsCount()).isEqualTo(2);

    final IntVar b = model.newBoolVar("b");
    model.addEquality(LinearExpr.sum(new IntVar[] {x, y}), 2).onlyEnforceIf(b.not());
    assertThat(model.model().getConstraintsCount()).isEqualTo(2);
    assertThat(model.model().getConstraints(1).hasLinear()).isTrue();
    assertThat(model.model().getConstraints(1).getEnforcementLiteralCount()).isEqualTo(1);
    assertThat(model.model().getConstraints(1).getEnforcementLiteral(0)).isEqualTo(-3);

    final IntVar c = model.newBoolVar("c");
    model.addEquality(LinearExpr.sum(new IntVar[] {x, y}), 3).onlyEnforceIf(new Literal[] {b, c});
    assertThat(model.model().getConstraintsCount()).isEqualTo(3);
    assertThat(model.model().getConstraints(2).hasLinear()).isTrue();
    assertThat(model.model().getConstraints(2).getEnforcementLiteralCount()).isEqualTo(2);
    assertThat(model.model().getConstraints(2).getEnforcementLiteral(0)).isEqualTo(2);
    assertThat(model.model().getConstraints(2).getEnforcementLiteral(1)).isEqualTo(3);
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

    model.addNoOverlap2D(
        new IntervalVar[] {interval1, interval2}, new IntervalVar[] {interval1, interval2});
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
    final IntVar x = model.newBoolVar("x");
    final IntVar y = model.newBoolVar("y");
    model.addImplication(x, y);

    String stats = model.modelStats();
    assertThat(stats).isNotEmpty();
  }

  @Test
  public void testCpModel_validateOk() throws Exception {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final IntVar x = model.newBoolVar("x");
    final IntVar y = model.newBoolVar("y");
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
    model.addLinearExpressionInDomain(LinearExpr.sum(new IntVar[] {x, y}),
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
}
