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
import com.google.ortools.mathopt.LinearConstraintsProto;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * A linear constraint on the decision variables of an optimization model.
 *
 * <p>Given decision variables x_1, x_2, ..., x_n, and doubles lb, ub, a_1, a_2, ..., a_n, this
 * class models a constraint of the form lb <= a_1 * x_1 + a_2 * x_2 + ... + a_n * x_n <= ub.
 *
 * <p>You cannot construct a {@link LinearConstraint} directly, instead call {@link
 * Model#addLinearConstraint(double, double, String)}.
 *
 * <p>The variable coefficients are stored in a {@link HashMap}, and only nonzero terms are stored.
 *
 * <p>A model will be rejected at solve time if lb == +inf, ub == -inf, any a_i is infinite, or any
 * of these values are NaN.
 *
 * <p>When a {@link Variable} is deleted from the model by {@link Model#deleteVariable(Variable)},
 * each constraint clears its storage of terms with the variable (it is roughly equivalent to
 * setting the coefficient to zero).
 */
public final class LinearConstraint implements ModelElement {
  private double lowerBound;
  private double upperBound;
  private final Map<Variable, Double> terms;
  private final long id;
  private final String name;
  private boolean deleted = false;
  private final OnChangeListener onChangeListener;
  private final ModelId modelId;
  private final Sink termsSink = new Sink();

  public double getLowerBound() {
    return lowerBound;
  }

  public double getUpperBound() {
    return upperBound;
  }

  /**
   * Returns the coefficient for {@code variable} in this constraint, or zero if the variable is not
   * in this constraint.
   *
   * @throws IllegalArgumentException if {@code variable} is from another model
   */
  public double getTerm(Variable variable) {
    checkVariableSameModel(variable);
    return terms.getOrDefault(variable, 0.0);
  }

  /** Returns an unmodifiable view of the linear terms of this constraint. */
  public Map<Variable, Double> getUnmodifiableTerms() {
    return Collections.unmodifiableMap(terms);
  }

  @Override
  public long getId() {
    return id;
  }

  public String getName() {
    return name;
  }

  public boolean isDeleted() {
    return deleted;
  }

  /**
   * Sets {@link #lowerBound} to {@code value} and returns this.
   *
   * @throws IllegalArgumentException if this linear constraint is already deleted.
   */
  public LinearConstraint setLowerBound(double value) {
    Preconditions.checkArgument(!deleted, "linear constraint %s was deleted", this);
    if (value != lowerBound) {
      lowerBound = value;
      onChangeListener.onLowerBoundChanged(this);
    }
    return this;
  }

  /**
   * Sets {@link #upperBound} to {@code value} and returns this.
   *
   * @throws IllegalArgumentException if this linear constraint is already deleted.
   */
  public LinearConstraint setUpperBound(double value) {
    Preconditions.checkArgument(!deleted, "linear constraint %s was deleted", this);
    if (value != upperBound) {
      upperBound = value;
      onChangeListener.onUpperBoundChanged(this);
    }
    return this;
  }

  /**
   * Sets the coefficient to {@code value} and returns this.
   *
   * <p>Setting {@code coefficient} to zero clears the sparse storage for {@code variable}.
   *
   * @throws IllegalArgumentException if this linear constraint is already deleted, if {@code
   *     variable} is already deleted, or if {@code variable} is from a different model.
   */
  public LinearConstraint setTerm(Variable variable, double coefficient) {
    Preconditions.checkArgument(!deleted, "linear constraint %s was deleted", this);
    Preconditions.checkArgument(!variable.isDeleted(), "variable %s was deleted", variable);
    checkVariableSameModel(variable);
    if (coefficient != terms.getOrDefault(variable, 0.0)) {
      if (coefficient == 0.0) {
        terms.remove(variable);
      } else {
        terms.put(variable, coefficient);
      }
      onChangeListener.onTermChanged(this, variable, coefficient);
    }
    return this;
  }

  /** Returns a complete description of this constraint. */
  public String toCompleteString() {
    var str = new StringBuilder(toString()).append(": ").append(lowerBound).append(" ≤ ");
    if (terms.isEmpty()) {
      str.append("0.0");
    } else {
      str.append(Variable.linearTermsToShortString(terms));
    }
    str.append(" ≤ ").append(upperBound);
    if (deleted) {
      str.append(" (deleted)");
    }
    return str.toString();
  }

  /** Returns the name of this constraint, or a short string with the id if the name is empty. */
  @Override
  public String toString() {
    return name.isEmpty() ? ("__lin_con#" + id + "__") : name;
  }

  void onVariableDeleted(Variable variable) {
    terms.remove(variable);
  }

  /** Instead see {@link Model#deleteLinearConstraint(LinearConstraint)}. */
  void markDeleted() {
    deleted = true;
  }

  ModelId getModelId() {
    return modelId;
  }

  void appendToProto(LinearConstraintsProto.Builder linearConstraintsProto) {
    linearConstraintsProto.addLowerBounds(lowerBound);
    linearConstraintsProto.addUpperBounds(upperBound);
    linearConstraintsProto.addNames(name);
    linearConstraintsProto.addIds(id);
  }

  /**
   * Returns {@link LinearExpressionSink} that modifies this constraint by adding variable terms to
   * {@link #terms} and subtracts constants from both {@link #upperBound} and {@link #lowerBound}.
   */
  LinearExpressionSink getTermsSink() {
    return termsSink;
  }

  /** Instead see {@link Model#addLinearConstraint(double, double, String)}. */
  LinearConstraint(double lowerBound, double upperBound, long id, String name,
      OnChangeListener onChangeListener, ModelId modelId) {
    this.lowerBound = lowerBound;
    this.upperBound = upperBound;
    this.terms = new HashMap<>();
    this.id = id;
    this.name = name;
    this.onChangeListener = onChangeListener;
    this.modelId = modelId;
  }

  private void checkVariableSameModel(Variable variable) {
    Preconditions.checkArgument(variable.getModelId() == modelId,
        "Variable %s is not from this model (named: %s), it is from another model (named: %s).",
        variable, modelId.getName(), variable.getModelId().getName());
  }

  interface OnChangeListener {
    void onLowerBoundChanged(LinearConstraint constraint);

    void onUpperBoundChanged(LinearConstraint constraint);

    // NOTE: the implementer of OnChangeListener could look up coefficient by calling
    // constraint.Term(variable), but we provide it directly to avoid an extra hash lookup.
    void onTermChanged(LinearConstraint constraint, Variable variable, double coefficient);
  }

  private final class Sink implements LinearExpressionSink {
    @Override
    public void add(double scale, Variable variable) {
      setTerm(variable, getTerm(variable) + scale);
    }

    @Override
    public void add(double constant) {
      setLowerBound(lowerBound - constant);
      setUpperBound(upperBound - constant);
    }
  }
}
