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
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;

import com.google.ortools.Loader;
import com.google.ortools.util.Domain;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class LinearExprTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testLinearExprAdd() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Domain domain = new Domain(0, 10);
    final IntVar y = model.newIntVarFromDomain(domain, "y");

    final LinearExpr expr = LinearExpr.newBuilder().add(y).build();
    assertNotNull(expr);

    assertEquals(1, expr.numElements());
    assertEquals(y.getIndex(), expr.getVariableIndex(0));
    assertEquals(1, expr.getCoefficient(0));
    assertEquals(0, expr.getOffset());
  }

  @Test
  public void testLinearExprAddLiteral() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar y = model.newBoolVar("y");

    final LinearExpr expr = LinearExpr.newBuilder().add(y).build();
    assertNotNull(expr);

    assertEquals(1, expr.numElements());
    assertEquals(y.getIndex(), expr.getVariableIndex(0));
    assertEquals(1, expr.getCoefficient(0));
    assertEquals(0, expr.getOffset());
  }

  @Test
  public void testLinearExprAddNegatedLiteral() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final BoolVar y = model.newBoolVar("y");

    final LinearExpr expr = LinearExpr.newBuilder().add(y.not()).build();
    assertNotNull(expr);

    assertEquals(1, expr.numElements());
    assertEquals(y.getIndex(), expr.getVariableIndex(0));
    assertEquals(-1, expr.getCoefficient(0));
    assertEquals(1, expr.getOffset());
  }

  @Test
  public void testLinearExprTerm() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Domain domain = new Domain(0, 10);
    final IntVar y = model.newIntVarFromDomain(domain, "y");

    final LinearExpr expr = LinearExpr.newBuilder().addTerm(y, 12).build();
    assertNotNull(expr);

    assertEquals(1, expr.numElements());
    assertEquals(y.getIndex(), expr.getVariableIndex(0));
    assertEquals(12, expr.getCoefficient(0));
    assertEquals(0, expr.getOffset());
  }

  @Test
  public void testLinearExprBooleanTerm() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Literal y = model.newBoolVar("y");

    final LinearExpr expr = LinearExpr.newBuilder().addTerm(y.not(), 12).build();
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(1);
    assertThat(expr.getVariableIndex(0)).isEqualTo(y.getIndex());
    assertThat(expr.getCoefficient(0)).isEqualTo(-12);
    assertThat(expr.getOffset()).isEqualTo(12);
  }

  @Test
  public void testLinearExprAffine() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Domain domain = new Domain(0, 10);
    final IntVar y = model.newIntVarFromDomain(domain, "y");

    final LinearExpr expr = LinearExpr.newBuilder().addTerm(y, 12).add(5).build();
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(1);
    assertThat(expr.getVariableIndex(0)).isEqualTo(y.getIndex());
    assertThat(expr.getCoefficient(0)).isEqualTo(12);
    assertThat(expr.getOffset()).isEqualTo(5);
  }

  @Test
  public void testLinearExprBooleanAffine() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Literal y = model.newBoolVar("y");

    final LinearExpr expr = LinearExpr.newBuilder().addTerm(y.not(), 12).add(5).build();
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(1);
    assertThat(expr.getVariableIndex(0)).isEqualTo(y.getIndex());
    assertThat(expr.getCoefficient(0)).isEqualTo(-12);
    assertThat(expr.getOffset()).isEqualTo(17);
  }

  @Test
  public void testLinearExprSum() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Domain domain = new Domain(0, 10);
    final IntVar x = model.newIntVarFromDomain(domain, "x");
    final IntVar y = model.newIntVarFromDomain(domain, "y");

    final LinearExpr expr = LinearExpr.newBuilder().add(x).add(y).build();
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(2);
    assertThat(expr.getVariableIndex(0)).isEqualTo(x.getIndex());
    assertThat(expr.getCoefficient(0)).isEqualTo(1);
    assertThat(expr.getVariableIndex(1)).isEqualTo(y.getIndex());
    assertThat(expr.getCoefficient(1)).isEqualTo(1);
    assertThat(expr.getOffset()).isEqualTo(0);
  }

  @Test
  public void testLinearExprWeightedSum() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Domain domain = new Domain(0, 10);
    final IntVar x = model.newIntVarFromDomain(domain, "x");
    final IntVar y = model.newIntVarFromDomain(domain, "y");

    final LinearExpr expr = LinearExpr.newBuilder().addTerm(x, 3).addTerm(y, 5).build();
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(2);
    assertThat(expr.getVariableIndex(0)).isEqualTo(x.getIndex());
    assertThat(expr.getCoefficient(0)).isEqualTo(3);
    assertThat(expr.getVariableIndex(1)).isEqualTo(y.getIndex());
    assertThat(expr.getCoefficient(1)).isEqualTo(5);
    assertThat(expr.getOffset()).isEqualTo(0);
  }

  @Test
  public void testLinearExprBooleanSum() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Literal x = model.newBoolVar("x");
    final Literal y = model.newBoolVar("y");

    final LinearExpr expr = LinearExpr.newBuilder().add(x).add(y.not()).build();
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(2);
    assertThat(expr.getVariableIndex(0)).isEqualTo(x.getIndex());
    assertThat(expr.getCoefficient(0)).isEqualTo(1);
    assertThat(expr.getVariableIndex(1)).isEqualTo(y.getIndex());
    assertThat(expr.getCoefficient(1)).isEqualTo(-1);
    assertThat(expr.getOffset()).isEqualTo(1);
  }

  @Test
  public void testLinearExprBooleanWeightedSum() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Literal x = model.newBoolVar("x");
    final Literal y = model.newBoolVar("y");

    final LinearExpr expr = LinearExpr.newBuilder().addTerm(x, 3).addTerm(y.not(), 5).build();
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(2);
    assertThat(expr.getVariableIndex(0)).isEqualTo(x.getIndex());
    assertThat(expr.getCoefficient(0)).isEqualTo(3);
    assertThat(expr.getVariableIndex(1)).isEqualTo(y.getIndex());
    assertThat(expr.getCoefficient(1)).isEqualTo(-5);
    assertThat(expr.getOffset()).isEqualTo(5);
  }
}
