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

import java.util.List;
import java.util.stream.Collector;

/**
 * Factory functions for creating linear expressions.
 *
 * <p>Users are encouraged to use these functions with static imports.
 *
 * <p>These functions are all defined elsewhere, this class just wraps them. You can use either
 * version. If you care about the exact data structure used, prefer the more explicit versions of
 * these functions (e.g. {@link HashLinearExpression#sum(Iterable)} )}), the default implementation
 * may change in the future.
 */
// TODO(b/266563182): update class description once quadratic expressions are supported.
public final class Expressions {
  private Expressions() {}

  /** Creates and returns a new empty linear expression. */
  public static HashLinearExpression linExpr() {
    return new HashLinearExpression();
  }

  /** Creates and returns a new linear expression with the value {@code constant}. */
  public static HashLinearExpression linExpr(double constant) {
    return new HashLinearExpression(constant);
  }

  /** Creates and returns a new linear expression that is a copy of {@code other}. */
  public static HashLinearExpression linExpr(LinearExpressionView other) {
    return new HashLinearExpression(other);
  }

  /** Creates and returns a linear expression equal to the sum of the terms in summand. */
  public static <T extends LinearExpressionView> HashLinearExpression sum(Iterable<T> summand) {
    return HashLinearExpression.sum(summand);
  }

  /** Returns a {@link Collector} that aggregates linear expressions by adding them together. */
  public static <T extends LinearExpressionView>
      Collector<T, HashLinearExpression, HashLinearExpression> sumCollector() {
    return HashLinearExpression.sumCollector();
  }

  /**
   * Creates and returns a linear expression equal to the inner product of expressions and
   * coefficients.
   *
   * <p>For example, given variables x and y, if expressions is [y, x] and coefficients is [3, 4],
   * the result is the expression 4 * x + 3 * y.
   */
  public static <T extends LinearExpressionView, N extends Number> HashLinearExpression
  innerProduct(List<N> coefficients, List<T> expressions) {
    return HashLinearExpression.innerProduct(coefficients, expressions);
  }

  /**
   * Returns a new linear expression equal to {@code coefficient} times {@code expression}.
   *
   * <p>The input {@code expression} is not modified.
   */
  public static <T extends LinearExpressionView> HashLinearExpression product(
      double coefficient, T expression) {
    return new HashLinearExpression(expression).scale(coefficient);
  }

  /**
   * Returns a new linear expression equal to {@code expressionNumerator} divided by {@code
   * denominator}.
   *
   * <p>The input {@code expressionNumerator} is not modified.
   */
  public static <T extends LinearExpressionView> HashLinearExpression quotient(
      T expressionNumerator, double denominator) {
    return new HashLinearExpression(expressionNumerator).divide(denominator);
  }
}
