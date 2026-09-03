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

package com.google.ortools.mathopt;

import static com.google.ortools.mathopt.Expressions.linExpr;
import static com.google.ortools.mathopt.testing.LinearConstraintSubject.assertThat;

import com.google.ortools.mathopt.testing.LinearConstraintSubject.LinearConstraintMatcher;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class LinearConstraintsAddConstraintOverloadTest {
  private ModelId modelId;
  private Variables variables;
  private Variable x;
  private Variable y;

  private LinearExpressionView xPlus2yPlus3;
  private HashLinearExpression minus10xPlus10yMinus1;
  private LinearConstraints constraints;

  @BeforeEach
  public void setUp() {
    modelId = new ModelId("m");
    variables = new Variables(modelId);
    x = variables.addVariable("x");
    y = variables.addVariable("y");
    constraints = new LinearConstraints(modelId);
    xPlus2yPlus3 = linExpr(x).add(2.0, y).add(3.0);
    minus10xPlus10yMinus1 = linExpr().subtract(10.0, x).add(10.0, y).subtract(1.0);
  }

  static LinearConstraintMatcher matcher() {
    return new LinearConstraintMatcher();
  }

  @Test
  public void addEq_numberExpr_hasRightSign() {
    // Input: 1 == x + 2 * y + 3
    LinearConstraint c = constraints.addEq(1.0, xPlus2yPlus3, "c");

    // Expected: -2 <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0).setUb(-2).setName("c"));
  }

  @Test
  public void addEq_exprNumber_hasRightSign() {
    // Input: x + 2 * y + 3 == 1
    LinearConstraint c = constraints.addEq(xPlus2yPlus3, 1.0, "c");

    // Expected: -2 <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0).setUb(-2).setName("c"));
  }

  @Test
  public void addEq_exprExpr_hasRightSign() {
    // Input: x + 2 * y + 3 == -10 * x + 10 * y - 1
    LinearConstraint c = constraints.addEq(xPlus2yPlus3, minus10xPlus10yMinus1, "c");

    // Expected: -4 <= 11 * x - 8 * y <= -4
    assertThat(c).matches(matcher().setLb(-4).setTerms(x, 11.0, y, -8.0).setUb(-4).setName("c"));
  }

  @Test
  public void addLe_numberExpr_hasRightSign() {
    // Input: 1 <= x + 2 * y + 3
    LinearConstraint c = constraints.addLe(1.0, xPlus2yPlus3, "c");

    // Expected: -2 <= x + 2 * y <= +inf
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0).setName("c"));
  }

  @Test
  public void addLe_exprNumber_hasRightSign() {
    // Input: x + 2 * y + 3 <= 1
    LinearConstraint c = constraints.addLe(xPlus2yPlus3, 1.0, "c");

    // Expected: -inf <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setTerms(x, 1.0, y, 2.0).setUb(-2.0).setName("c"));
  }

  @Test
  public void addLe_exprExpr_hasRightSign() {
    // Input: x + 2 * y + 3 <= -10 * x + 10 * y - 1
    LinearConstraint c = constraints.addLe(xPlus2yPlus3, minus10xPlus10yMinus1, "c");

    // Expected: -inf <= 11 * x - 8 * y <= -4
    assertThat(c).matches(matcher().setTerms(x, 11.0, y, -8.0).setUb(-4).setName("c"));
  }

  @Test
  public void addGe_numberExpr_hasRightSign() {
    // Input: 1 >= x + 2 * y + 3
    LinearConstraint c = constraints.addGe(1.0, xPlus2yPlus3, "c");

    // Expected: -inf <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setTerms(x, 1.0, y, 2.0).setUb(-2).setName("c"));
  }

  @Test
  public void addGe_exprNumber_hasRightSign() {
    // Input: x + 2 * y + 3 >= 1
    LinearConstraint c = constraints.addGe(xPlus2yPlus3, 1.0, "c");

    // Expected: -2 <= x + 2 * y <= inf
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0).setName("c"));
  }

  @Test
  public void addGe_exprExpr_hasRightSign() {
    // Input: x + 2 * y + 3 >= -10 * x + 10 * y - 1
    LinearConstraint c = constraints.addGe(xPlus2yPlus3, minus10xPlus10yMinus1, "c");

    // Expected: -4 <= 11 * x - 8 * y <= inf
    assertThat(c).matches(matcher().setLb(-4).setTerms(x, 11.0, y, -8.0).setName("c"));
  }

  @Test
  public void addRange_exprHasOffset_offsetIsSubtracted() {
    // Input: 4 <= -10 * x + 10 * y - 1 <= 10
    LinearConstraint c = constraints.addRange(4, minus10xPlus10yMinus1, 10, "c");

    // Expected: 5 <= -10 * x + 10 * y <= 11
    assertThat(c).matches(matcher().setLb(5).setTerms(x, -10.0, y, 10).setUb(11).setName("c"));
  }
}
