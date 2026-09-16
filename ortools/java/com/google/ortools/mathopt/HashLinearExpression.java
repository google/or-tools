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
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Sets;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.function.BinaryOperator;
import java.util.function.Function;
import java.util.function.Supplier;
import java.util.stream.Collector;

/**
 * A mutable linear expression that stores variable coefficient pairs in a hashmap.
 *
 * <p>{@link Variable} objects not appearing as keys of {@link #getTerms()} are interpreted as
 * having coefficient zero. Some attempt is made to avoid needlessly storing terms with coefficient
 * zero, but zeros can appear (e.g. users can access the mutable map).
 */
public final class HashLinearExpression implements LinearExpressionView {
  private final HashMap<Variable, Double> terms;
  private double offset;
  private final Sink sink = new Sink();

  /** A linear expression with the value zero. */
  public HashLinearExpression() {
    this(0.0);
  }

  /** A linear expression with the value {@code offset}. */
  public HashLinearExpression(double offset) {
    terms = new HashMap<>();
    this.offset = offset;
  }

  /** A linear expression that is a copy of {@code other}. */
  public HashLinearExpression(LinearExpressionView other) {
    this();
    add(other);
  }

  @Override
  public double offset() {
    return offset;
  }

  /**
   * {@inheritDoc}
   *
   * <p>The values returned will have unique keys. The order of the returned entries is arbitrary.
   */
  @Override
  public Set<Map.Entry<Variable, Double>> unmodifiableTerms() {
    return Collections.unmodifiableMap(terms).entrySet();
  }

  /**
   * Returns a mutable view of the variable coefficient pairs in the expression.
   *
   * <p>This map may contain zero coefficients.
   */
  public Map<Variable, Double> getTerms() {
    return terms;
  }

  /** Returns the coefficient for variable, or 0.0 if variable is not a key in terms. */
  public double getTerm(Variable variable) {
    // Using terms.getOrDefault() would create a boxed 0.0 on every call.
    Double result = terms.get(variable);
    if (result == null) {
      return 0.0;
    }
    return result;
  }

  /**
   * Sets the coefficient for {@code variable} to {@code coefficient} (or if coefficient is zero,
   * deletes variable from {@link #terms}) and returns {@code this}.
   */
  public HashLinearExpression setTerm(Variable variable, double coefficient) {
    if (coefficient == 0.0) {
      terms.remove(variable);
    } else {
      terms.put(variable, coefficient);
    }
    return this;
  }

  /**
   * Negates {@link #offset} and the coefficient for each element of {@link #terms} and returns
   * {@code this}.
   */
  public HashLinearExpression negate() {
    offset = -offset;
    for (Map.Entry<Variable, Double> term : terms.entrySet()) {
      term.setValue(-term.getValue());
    }
    return this;
  }

  /** Multiplies the offset and each coefficient by scale and returns {@code this}. */
  public HashLinearExpression scale(double scale) {
    offset *= scale;
    for (Map.Entry<Variable, Double> term : terms.entrySet()) {
      term.setValue(term.getValue() * scale);
    }
    return this;
  }

  /** Divides the offset and each coefficient by divisor. */
  public HashLinearExpression divide(double divisor) {
    Preconditions.checkArgument(divisor != 0.0, "divisor must be nonzero");
    offset /= divisor;
    for (Map.Entry<Variable, Double> term : terms.entrySet()) {
      term.setValue(term.getValue() / divisor);
    }
    return this;
  }

  /** Adds {@code scale} * {@code variable} to {@code this} and returns {@code this}. */
  public HashLinearExpression add(double scale, Variable variable) {
    sink.add(scale, variable);
    return this;
  }

  /** Adds {@code constant} to {@code this} and returns {@code this}. */
  public HashLinearExpression add(double constant) {
    sink.add(constant);
    return this;
  }

  /** Adds {@code variable} to {@code this} and returns {@code this}. */
  public HashLinearExpression add(Variable variable) {
    sink.add(variable);
    return this;
  }

  /** Adds {@code scale} * {@code expression} to {@code this} and returns {@code this}. */
  public HashLinearExpression add(double scale, LinearExpressionView expression) {
    sink.add(scale, expression);
    return this;
  }

  /** Adds {@code expression} to {@code this} and returns {@code this}. */
  public HashLinearExpression add(LinearExpressionView expression) {
    sink.add(expression);
    return this;
  }

  /** Adds each element of {@code summand} to {@code this} and returns {@code this}. */
  public <T extends LinearExpressionView> HashLinearExpression addAll(Iterable<T> summand) {
    sink.addAll(summand);
    return this;
  }

  /**
   * Adds {@code coefficients.get(i)} * {@code expression.get(i)} to {@code this} for every {@code
   * i} and returns {@code this}.
   *
   * @throws IllegalArgumentException if {@code coefficients.size() != expression.size()}
   */
  public <T extends LinearExpressionView, N extends Number> HashLinearExpression addInnerProduct(
      List<N> coefficients, List<T> expressions) {
    sink.addInnerProduct(coefficients, expressions);
    return this;
  }

  /** Subtracts {@code constant} from {@code this} and returns {@code this}. */
  public HashLinearExpression subtract(double constant) {
    sink.subtract(constant);
    return this;
  }

  /** Subtracts {@code scale} * {@code variable} from {@code this} and returns {@code this}. */
  public HashLinearExpression subtract(double scale, Variable variable) {
    sink.subtract(scale, variable);
    return this;
  }

  /** Subtracts {@code variable} from {@code this} and returns {@code this}. */
  public HashLinearExpression subtract(Variable variable) {
    sink.subtract(variable);
    return this;
  }

  /** Subtracts {@code scale} * {@code expression} from {@code this} and returns {@code this}. */
  public HashLinearExpression subtract(double scale, LinearExpressionView expression) {
    sink.subtract(scale, expression);
    return this;
  }

  /** Subtracts {@code expression} from {@code this} and returns {@code this}. */
  public HashLinearExpression subtract(LinearExpressionView expression) {
    sink.subtract(expression);
    return this;
  }

  /** Returns a {@link Collector} that aggregates linear expressions by adding them together. */
  public static <T extends LinearExpressionView>
      Collector<T, HashLinearExpression, HashLinearExpression> sumCollector() {
    return new SumCollector<>();
  }

  /** Creates and returns a linear expression equal to the sum of the terms in summand. */
  public static <T extends LinearExpressionView> HashLinearExpression sum(Iterable<T> summand) {
    var result = new HashLinearExpression();
    result.addAll(summand);
    return result;
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
    var result = new HashLinearExpression();
    result.addInnerProduct(coefficients, expressions);
    return result;
  }

  private class Sink implements LinearExpressionSink {
    @Override
    public void add(double scale, Variable variable) {
      terms.put(variable, terms.getOrDefault(variable, 0.0) + scale);
    }

    @Override
    public void add(double constant) {
      offset += constant;
    }
  }

  private static class SumCollector<T extends LinearExpressionView>
      implements Collector<T, HashLinearExpression, HashLinearExpression> {
    private static final ImmutableSet<Characteristics> CHARACTERISTICS =
        Sets.immutableEnumSet(Characteristics.IDENTITY_FINISH);

    @Override
    public Supplier<HashLinearExpression> supplier() {
      return HashLinearExpression::new;
    }

    @Override
    public BiConsumer<HashLinearExpression, T> accumulator() {
      return HashLinearExpression::add;
    }

    @Override
    public BinaryOperator<HashLinearExpression> combiner() {
      return (left, right) -> {
        left.add(right);
        return left;
      };
    }

    @Override
    public Function<HashLinearExpression, HashLinearExpression> finisher() {
      return expr -> expr;
    }

    @Override
    public Set<Characteristics> characteristics() {
      return CHARACTERISTICS;
    }
  }
}
