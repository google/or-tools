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
import com.google.ortools.util.Domain;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class LinearExprTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testLinearExpr_term() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Domain domain = new Domain(0, 10);
    final IntVar y = model.newIntVarFromDomain(domain, "y");

    final LinearExpr expr = LinearExpr.term(y, 12);
    assertNotNull(expr);

    assertEquals(1, expr.numElements());
    assertEquals(y, expr.getVariable(0));
    assertEquals(12, expr.getCoefficient(0));
    assertEquals(0, expr.getOffset());
  }

  @Test
  public void testLinearExpr_booleanTerm() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Literal y = model.newBoolVar("y");

    final LinearExpr expr = LinearExpr.term(y.not(), 12);
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(1);
    assertThat(expr.getVariable(0)).isEqualTo(y);
    assertThat(expr.getCoefficient(0)).isEqualTo(-12);
    assertThat(expr.getOffset()).isEqualTo(12);
  }

  @Test
  public void testLinearExpr_affine() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Domain domain = new Domain(0, 10);
    final IntVar y = model.newIntVarFromDomain(domain, "y");

    final LinearExpr expr = LinearExpr.affine(y, 12, 5);
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(1);
    assertThat(expr.getVariable(0)).isEqualTo(y);
    assertThat(expr.getCoefficient(0)).isEqualTo(12);
    assertThat(expr.getOffset()).isEqualTo(5);
  }

  @Test
  public void testLinearExpr_booleanAffine() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Literal y = model.newBoolVar("y");

    final LinearExpr expr = LinearExpr.affine(y.not(), 12, 5);
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(1);
    assertThat(expr.getVariable(0)).isEqualTo(y);
    assertThat(expr.getCoefficient(0)).isEqualTo(-12);
    assertThat(expr.getOffset()).isEqualTo(17);
  }

  @Test
  public void testLinearExpr_sum() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Domain domain = new Domain(0, 10);
    final IntVar x = model.newIntVarFromDomain(domain, "x");
    final IntVar y = model.newIntVarFromDomain(domain, "y");

    final LinearExpr expr = LinearExpr.sum(new IntVar[] {x, y});
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(2);
    assertThat(expr.getVariable(0)).isEqualTo(x);
    assertThat(expr.getCoefficient(0)).isEqualTo(1);
    assertThat(expr.getVariable(1)).isEqualTo(y);
    assertThat(expr.getCoefficient(1)).isEqualTo(1);
    assertThat(expr.getOffset()).isEqualTo(0);
  }

  @Test
  public void testLinearExpr_scalProd() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Domain domain = new Domain(0, 10);
    final IntVar x = model.newIntVarFromDomain(domain, "x");
    final IntVar y = model.newIntVarFromDomain(domain, "y");

    final LinearExpr expr = LinearExpr.scalProd(new IntVar[] {x, y}, new long[] {3, 5});
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(2);
    assertThat(expr.getVariable(0)).isEqualTo(x);
    assertThat(expr.getCoefficient(0)).isEqualTo(3);
    assertThat(expr.getVariable(1)).isEqualTo(y);
    assertThat(expr.getCoefficient(1)).isEqualTo(5);
    assertThat(expr.getOffset()).isEqualTo(0);
  }

  @Test
  public void testLinearExpr_booleanSum() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Literal x = model.newBoolVar("x");
    final Literal y = model.newBoolVar("y");

    final LinearExpr expr = LinearExpr.booleanSum(new Literal[] {x, y.not()});
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(2);
    assertThat(expr.getVariable(0)).isEqualTo(x);
    assertThat(expr.getCoefficient(0)).isEqualTo(1);
    assertThat(expr.getVariable(1)).isEqualTo(y);
    assertThat(expr.getCoefficient(1)).isEqualTo(-1);
    assertThat(expr.getOffset()).isEqualTo(-1);
  }

  @Test
  public void testLinearExpr_booleanScalProd() {
    final CpModel model = new CpModel();
    assertNotNull(model);
    final Literal x = model.newBoolVar("x");
    final Literal y = model.newBoolVar("y");

    final LinearExpr expr =
        LinearExpr.booleanScalProd(new Literal[] {x, y.not()}, new long[] {3, 5});
    assertNotNull(expr);

    assertThat(expr.numElements()).isEqualTo(2);
    assertThat(expr.getVariable(0)).isEqualTo(x);
    assertThat(expr.getCoefficient(0)).isEqualTo(3);
    assertThat(expr.getVariable(1)).isEqualTo(y);
    assertThat(expr.getCoefficient(1)).isEqualTo(-5);
    assertThat(expr.getOffset()).isEqualTo(-5);
  }
}
