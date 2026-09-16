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

// Note: these tests are roughly duplicated from LinearConstraintsAddConstraintsOverloadTests, but
// now with two versions, one for with a name and one for without a name.

package com.google.ortools.mathopt;

import static com.google.ortools.mathopt.Expressions.linExpr;
import static com.google.ortools.mathopt.testing.LinearConstraintSubject.assertThat;

import com.google.ortools.mathopt.testing.LinearConstraintSubject.LinearConstraintMatcher;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class ModelAddLinearConstraintOverloadsTest {
  private Model model;

  private Variable x;
  private Variable y;

  private LinearExpressionView xPlus2yPlus3;
  private HashLinearExpression minus10xPlus10yMinus1;

  @BeforeEach
  public void setUp() {
    model = new Model();
    x = model.addVariable("x");
    y = model.addVariable("y");
    xPlus2yPlus3 = linExpr(x).add(2.0, y).add(3.0);
    minus10xPlus10yMinus1 = linExpr().subtract(10.0, x).add(10.0, y).subtract(1.0);
  }

  static LinearConstraintMatcher matcher() {
    return new LinearConstraintMatcher();
  }

  @Test
  public void addEq_numberExpr_hasRightSign() {
    // Input: 1 == x + 2 * y + 3
    LinearConstraint c = model.addEq(1.0, xPlus2yPlus3);

    // Expected: -2 <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0).setUb(-2));
  }

  @Test
  public void addEq_numberExprName_hasRightSign() {
    // Input: 1 == x + 2 * y + 3
    LinearConstraint c = model.addEq(1.0, xPlus2yPlus3, "c");

    // Expected: -2 <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0).setUb(-2).setName("c"));
  }

  @Test
  public void addEq_exprNumber_hasRightSign() {
    // Input: x + 2 * y + 3 == 1
    LinearConstraint c = model.addEq(xPlus2yPlus3, 1.0);

    // Expected: -2 <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0).setUb(-2));
  }

  @Test
  public void addEq_exprNumberName_hasRightSign() {
    // Input: x + 2 * y + 3 == 1
    LinearConstraint c = model.addEq(xPlus2yPlus3, 1.0, "c");

    // Expected: -2 <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0).setUb(-2).setName("c"));
  }

  @Test
  public void addEq_exprExpr_hasRightSign() {
    // Input: x + 2 * y + 3 == -10 * x + 10 * y - 1
    LinearConstraint c = model.addEq(xPlus2yPlus3, minus10xPlus10yMinus1);

    // Expected: -4 <= 11 * x - 8 * y <= -4
    assertThat(c).matches(matcher().setLb(-4).setTerms(x, 11.0, y, -8.0).setUb(-4));
  }

  @Test
  public void addEq_exprExprName_hasRightSign() {
    // Input: x + 2 * y + 3 == -10 * x + 10 * y - 1
    LinearConstraint c = model.addEq(xPlus2yPlus3, minus10xPlus10yMinus1, "c");

    // Expected: -4 <= 11 * x - 8 * y <= -4
    assertThat(c).matches(matcher().setLb(-4).setTerms(x, 11.0, y, -8.0).setUb(-4).setName("c"));
  }

  @Test
  public void addLe_numberExpr_hasRightSign() {
    // Input: 1 <= x + 2 * y + 3
    LinearConstraint c = model.addLe(1.0, xPlus2yPlus3);

    // Expected: -2 <= x + 2 * y <= +inf
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0));
  }

  @Test
  public void addLe_numberExprName_hasRightSign() {
    // Input: 1 <= x + 2 * y + 3
    LinearConstraint c = model.addLe(1.0, xPlus2yPlus3, "c");

    // Expected: -2 <= x + 2 * y <= +inf
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0).setName("c"));
  }

  @Test
  public void addLe_exprNumber_hasRightSign() {
    // Input: x + 2 * y + 3 <= 1
    LinearConstraint c = model.addLe(xPlus2yPlus3, 1.0);

    // Expected: -inf <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setTerms(x, 1.0, y, 2.0).setUb(-2.0));
  }

  @Test
  public void addLe_exprNumberName_hasRightSign() {
    // Input: x + 2 * y + 3 <= 1
    LinearConstraint c = model.addLe(xPlus2yPlus3, 1.0, "c");

    // Expected: -inf <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setTerms(x, 1.0, y, 2.0).setUb(-2.0).setName("c"));
  }

  @Test
  public void addLe_exprExpr_hasRightSign() {
    // Input: x + 2 * y + 3 <= -10 * x + 10 * y - 1
    LinearConstraint c = model.addLe(xPlus2yPlus3, minus10xPlus10yMinus1);

    // Expected: -inf <= 11 * x - 8 * y <= -4
    assertThat(c).matches(matcher().setTerms(x, 11.0, y, -8.0).setUb(-4));
  }

  @Test
  public void addLe_exprExprName_hasRightSign() {
    // Input: x + 2 * y + 3 <= -10 * x + 10 * y - 1
    LinearConstraint c = model.addLe(xPlus2yPlus3, minus10xPlus10yMinus1, "c");

    // Expected: -inf <= 11 * x - 8 * y <= -4
    assertThat(c).matches(matcher().setTerms(x, 11.0, y, -8.0).setUb(-4).setName("c"));
  }

  @Test
  public void addGe_numberExpr_hasRightSign() {
    // Input: 1 >= x + 2 * y + 3
    LinearConstraint c = model.addGe(1.0, xPlus2yPlus3);

    // Expected: -inf <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setTerms(x, 1.0, y, 2.0).setUb(-2));
  }

  @Test
  public void addGe_numberExprName_hasRightSign() {
    // Input: 1 >= x + 2 * y + 3
    LinearConstraint c = model.addGe(1.0, xPlus2yPlus3, "c");

    // Expected: -inf <= x + 2 * y <= -2
    assertThat(c).matches(matcher().setTerms(x, 1.0, y, 2.0).setUb(-2).setName("c"));
  }

  @Test
  public void addGe_exprNumber_hasRightSign() {
    // Input: x + 2 * y + 3 >= 1
    LinearConstraint c = model.addGe(xPlus2yPlus3, 1.0);

    // Expected: -2 <= x + 2 * y <= inf
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0));
  }

  @Test
  public void addGe_exprNumberName_hasRightSign() {
    // Input: x + 2 * y + 3 >= 1
    LinearConstraint c = model.addGe(xPlus2yPlus3, 1.0, "c");

    // Expected: -2 <= x + 2 * y <= inf
    assertThat(c).matches(matcher().setLb(-2).setTerms(x, 1.0, y, 2.0).setName("c"));
  }

  @Test
  public void addGe_exprExpr_hasRightSign() {
    // Input: x + 2 * y + 3 >= -10 * x + 10 * y - 1
    LinearConstraint c = model.addGe(xPlus2yPlus3, minus10xPlus10yMinus1);

    // Expected: -4 <= 11 * x - 8 * y <= inf
    assertThat(c).matches(matcher().setLb(-4).setTerms(x, 11.0, y, -8.0));
  }

  @Test
  public void addGe_exprExprName_hasRightSign() {
    // Input: x + 2 * y + 3 >= -10 * x + 10 * y - 1
    LinearConstraint c = model.addGe(xPlus2yPlus3, minus10xPlus10yMinus1, "c");

    // Expected: -4 <= 11 * x - 8 * y <= inf
    assertThat(c).matches(matcher().setLb(-4).setTerms(x, 11.0, y, -8.0).setName("c"));
  }

  @Test
  public void addRange_exprHasOffset_offsetIsSubtracted() {
    // Input: 4 <= -10 * x + 10 * y - 1 <= 10
    LinearConstraint c = model.addRange(4, minus10xPlus10yMinus1, 10);

    // Expected: 5 <= -10 * x + 10 * y <= 11
    assertThat(c).matches(matcher().setLb(5).setTerms(x, -10.0, y, 10).setUb(11));
  }

  @Test
  public void addRange_exprHasOffsetName_offsetIsSubtracted() {
    // Input: 4 <= -10 * x + 10 * y - 1 <= 10
    LinearConstraint c = model.addRange(4, minus10xPlus10yMinus1, 10, "c");

    // Expected: 5 <= -10 * x + 10 * y <= 11
    assertThat(c).matches(matcher().setLb(5).setTerms(x, -10.0, y, 10).setUb(11).setName("c"));
  }
}
