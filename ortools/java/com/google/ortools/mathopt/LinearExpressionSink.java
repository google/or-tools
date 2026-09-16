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

import com.google.common.base.Preconditions;
import java.util.Iterator;
import java.util.List;

/**
 * An expression over the variables of the model that you can add {@link LinearExpressionView}
 * objects to.
 *
 * <p>This is an advanced API that can offer a small performance improvement in some situations, but
 * can be ignored by most users, generally just see {@link HashLinearExpression}.
 */
interface LinearExpressionSink {
  /** Adds {@code scale} * {@code variable} to {@code this}. */
  void add(double scale, Variable variable);

  /** Adds {@code constant} to {@code this}. */
  void add(double constant);

  /** Adds {@code variable} to {@code this}. */
  default void add(Variable variable) {
    add(1.0, variable);
  }

  /** Adds {@code scale} * {@code expression} to {@code this}. */
  default void add(double scale, LinearExpressionView expression) {
    expression.addTo(scale, this);
  }

  /** Adds {@code expression} to {@code this}. */
  default void add(LinearExpressionView expression) {
    add(1.0, expression);
  }

  /** Adds each element of {@code summand} to {@code this}. */
  default<T extends LinearExpressionView> void addAll(Iterable<T> summand) {
    for (T term : summand) {
      add(term);
    }
  }

  /**
   * Adds {@code coefficients.get(i)} * {@code expression.get(i)} to {@code this} for every {@code
   * i}.
   *
   * @throws IllegalArgumentException if {@code coefficients.size() != expression.size()}
   */
  default<T extends LinearExpressionView, N extends Number> void addInnerProduct(
      List<N> coefficients, List<T> expressions) {
    Preconditions.checkArgument(expressions.size() == coefficients.size(),
        "Expected expressions.size()=%s and coefficients.size()=%s to be equal.",
        expressions.size(), coefficients.size());
    Iterator<N> coefficientsIter = coefficients.iterator();
    for (T element : expressions) {
      double coefficient = coefficientsIter.next().doubleValue();
      add(coefficient, element);
    }
  }

  /** Subtracts {@code constant} from {@code this}. */
  default void subtract(double constant) {
    add(-constant);
  }

  /** Subtracts {@code scale} * {@code variable} from {@code this}. */
  default void subtract(double scale, Variable variable) {
    add(-scale, variable);
  }

  /** Subtracts {@code variable} from {@code this}. */
  default void subtract(Variable variable) {
    subtract(1.0, variable);
  }

  /** Subtracts {@code scale} * {@code expression} from {@code this}. */
  default void subtract(double scale, LinearExpressionView expression) {
    add(-scale, expression);
  }

  /** Subtracts {@code expression} from {@code this}. */
  default void subtract(LinearExpressionView expression) {
    subtract(1.0, expression);
  }
}
