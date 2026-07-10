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
import com.google.common.collect.ImmutableList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * The objective for an optimization model, a linear function to maximize or minimize.
 *
 * <p>For decision variables x_i, i = 1,...,n, a function f(x) = sum_i c_i * x_i + offset and a
 * direction (either maximization or minimization). Typically, there is only one objective, although
 * some solvers can deal with problems containing multiple objectives (see {@link
 * AuxiliaryObjective}).
 *
 * <p>You cannot construct an {@link Objective} directly, instead call {@link Model#getObjective()}.
 *
 * <p>The variable coefficients are stored using hash tables, and only nonzero terms are stored.
 *
 * <p>The model will be rejected at solve time if any coefficient is infinite or NaN.
 *
 * <p>When a {@link Variable} is deleted from the model by {@link Model#deleteVariable(Variable)},
 * the storage of terms with this variable are cleared (it is roughly equivalent to setting the
 * coefficient to zero).
 */
// TODO(b/258535454): update summary fragment and first paragraph once quadratics are supported,
// e.g. include sum_i sum_{j >= i} a_{ij} * x_i * x_j for f.
// TODO(b/264559415): post Java 19, this should be a sealed class.
public class Objective {
  private boolean maximize = false;
  private double offset = 0.0;
  private final Map<Variable, Double> linearTerms = new HashMap<>();
  private long priority;
  private final String name;
  private final OnChangeListener listener;
  private final ModelId modelId;

  private final Sink sink = new Sink();

  /** Indicates if the optimization problem is to maximize or minimize the objective function. */
  public final boolean isMaximize() {
    return maximize;
  }

  public final double getOffset() {
    return offset;
  }

  /**
   * Determines the order to apply the objectives (lowest to highest) when solving problems with
   * hierarchical objectives.
   *
   * <p>Used only for problems with multiple objectives, has no effect for problems with only one
   * objective. See also {@link Model#addAuxiliaryObjective()} and {@link AuxiliaryObjective}.
   *
   * <p>The priorities of the objectives in the model must be distinct at solve time, or the model
   * will be rejected.
   */
  public final long getPriority() {
    return priority;
  }

  public final String getName() {
    return name;
  }

  /**
   * Returns the linear coefficient for {@code variable} in the objective, or zero if it is not set.
   *
   * @throws IllegalArgumentException if {@code variable} is from another model
   */
  public final double getLinearTerm(Variable variable) {
    checkVariableSameModel(variable);
    return linearTerms.getOrDefault(variable, 0.0).doubleValue();
  }

  /** Returns an unmodifiable view of the linear terms in the objective. */
  public final Map<Variable, Double> getUnmodifiableLinearTerms() {
    return Collections.unmodifiableMap(linearTerms);
  }

  /**
   * Makes the objective maximization if {@code isMaximize} is true (and to minimization otherwise)
   * and returns this.
   *
   * @throws IllegalArgumentException if this objective is already deleted
   */
  public final Objective setMaximize(boolean isMaximize) {
    Preconditions.checkArgument(!isDeleted(), "%s was deleted", this);
    if (maximize != isMaximize) {
      maximize = isMaximize;
      listener.onSenseChanged(this);
    }
    return this;
  }

  /**
   * Makes the objective maximization and returns this.
   *
   * @throws IllegalArgumentException if this objective is already deleted
   */
  public final Objective setToMaximize() {
    return setMaximize(/* isMaximize= */ true);
  }

  /**
   * Makes the objective minimization and returns this.
   *
   * @throws IllegalArgumentException if this objective is already deleted
   */
  public final Objective setToMinimize() {
    return setMaximize(/* isMaximize= */ false);
  }

  /**
   * Sets {@link #offset} to {@code offset} and returns this.
   *
   * @throws IllegalArgumentException if this objective is already deleted
   */
  public final Objective setOffset(double offset) {
    Preconditions.checkArgument(!isDeleted(), "%s was deleted", this);
    if (this.offset != offset) {
      this.offset = offset;
      listener.onOffsetChanged(this);
    }
    return this;
  }

  /**
   * Sets {@link #priority} to {@code priority} and returns this.
   *
   * @throws IllegalArgumentException if this objective is already deleted or if {@code priority} is
   *     negative
   */
  public final Objective setPriority(long priority) {
    Preconditions.checkArgument(!isDeleted(), "%s was deleted", this);
    Preconditions.checkArgument(priority >= 0, "priority should be nonnegative, was %s", priority);
    if (this.priority != priority) {
      this.priority = priority;
      listener.onPriorityChanged(this);
    }
    return this;
  }

  /**
   * Sets the linear objective coefficient for {@code variable} to {@code coefficient} and returns
   * this.
   *
   * <p>Setting {@code coefficient} to zero clears the sparse storage for {@code variable}.
   *
   * @throws IllegalArgumentException if the objective is already deleted, if {@code variable} is
   *     already deleted, or if {@code variable} is from a different model.
   */
  public final Objective setLinearTerm(Variable variable, double coefficient) {
    Preconditions.checkArgument(!isDeleted(), "%s was deleted", this);
    Preconditions.checkArgument(!variable.isDeleted(), "variable %s was deleted", variable);
    checkVariableSameModel(variable);
    double old = linearTerms.getOrDefault(variable, 0.0).doubleValue();
    if (old != coefficient) {
      if (coefficient == 0.0) {
        linearTerms.remove(variable);
      } else {
        linearTerms.put(variable, coefficient);
      }
      listener.onLinearTermChange(this, variable, coefficient);
    }
    return this;
  }

  /**
   * Sets the offset and all linear coefficients to zero.
   *
   * <p>The objective direction is not changed.
   *
   * @throws IllegalArgumentException if the objective was deleted.
   */
  public final void clear() {
    Preconditions.checkArgument(!isDeleted(), "%s was deleted", this);
    setOffset(0.0);
    ImmutableList<Variable> clearedVars = ImmutableList.copyOf(linearTerms.keySet());
    linearTerms.clear();
    for (Variable v : clearedVars) {
      listener.onLinearTermChange(this, v, 0.0);
    }
  }

  /**
   * Replaces the existing offset and linear terms with {@code linearExpression}.
   *
   * <p>The objective direction is not changed.
   *
   * @throws IllegalArgumentException if any variable in {@code linearExpression} is from another
   *     model or was deleted, or if the objective was deleted.
   */
  public final void replace(LinearExpressionView linearExpression) {
    clear();
    add(linearExpression);
  }

  /**
   * Adds {@code value} to {@link #offset} and returns {@code this}.
   *
   * @throws IllegalArgumentException if the objective was deleted.
   */
  public final Objective add(double value) {
    sink.add(value);
    return this;
  }

  /**
   * Adds {@code linearExpression} to the current objective and returns {@code this}.
   *
   * @throws IllegalArgumentException if any variable in {@code linearExpression} is from another
   *     model or was deleted, or if the objective was deleted.
   */
  public final Objective add(LinearExpressionView linearExpression) {
    sink.add(linearExpression);
    return this;
  }

  /**
   * Adds {@code scale} * {@code linearExpression} to the current objective and returns {@code
   * this}.
   *
   * @throws IllegalArgumentException if any variable in {@code linearExpression} is from another
   *     model or was deleted, or if the objective was deleted.
   */
  public final Objective add(double scale, LinearExpressionView linearExpression) {
    sink.add(scale, linearExpression);
    return this;
  }

  /**
   * Adds each term of {@code summand} to the current objective and returns {@code this}.
   *
   * @throws IllegalArgumentException if any {@link LinearExpressionView} in {@code summand} has a
   *     variable that is either from another model or was deleted, or if the objective was deleted.
   */
  public <T extends LinearExpressionView> Objective addAll(Iterable<T> summand) {
    // We need to check here, addAll() is no-op if summand is empty.
    Preconditions.checkArgument(!isDeleted(), "%s was deleted", this);
    sink.addAll(summand);
    return this;
  }

  /**
   * Adds {@code coefficients.get(i)} * {@code expressions.get(i)} to the current objective for
   * every element of the equally sized lists and returns {@code this}.
   *
   * @throws IllegalArgumentException if {@code coefficients.size() != expressions.size()}, if any
   *     {@link LinearExpressionView} in {@code expressions} has a variable that is either from
   *     another model or was deleted, or if the objective was deleted.
   */
  public final <T extends LinearExpressionView, N extends Number> Objective addInnerProduct(
      List<N> coefficients, List<T> expressions) {
    // We need to check here, addInnerProduct() is no-op if summand is empty.
    Preconditions.checkArgument(!isDeleted(), "%s was deleted", this);
    sink.addInnerProduct(coefficients, expressions);
    return this;
  }

  /**
   * Subtracts {@code value} from {@link #offset} and returns {@code this}.
   *
   * @throws IllegalArgumentException if the objective was deleted.
   */
  public final Objective subtract(double value) {
    sink.subtract(value);
    return this;
  }

  /**
   * Subtracts {@code linearExpression} from the current objective and returns {@code this}.
   *
   * @throws IllegalArgumentException if any variable in {@code linearExpression} is from another
   *     model or was deleted, or if the objective was deleted.
   */
  public final Objective subtract(LinearExpressionView linearExpression) {
    sink.subtract(linearExpression);
    return this;
  }

  /**
   * Subtracts {@code scale} * {@code linearExpression} to the current objective and returns {@code
   * this}.
   *
   * @throws IllegalArgumentException if any variable in {@code linearExpression} is from another
   *     model or was deleted, or if the objective was deleted.
   */
  public final Objective subtract(double scale, LinearExpressionView linearExpression) {
    sink.subtract(scale, linearExpression);
    return this;
  }

  /** Returns a short string with the objective's id. */
  @Override
  public String toString() {
    String label = "Primary Objective";
    if (name.isEmpty()) {
      return label;
    } else {
      return name + " (" + label + ")";
    }
  }

  /**
   * Returns a complete description of the objective.
   *
   * @see #toCompleteString(boolean)
   */
  public final String toCompleteString() {
    return toCompleteString(true);
  }

  /**
   * Returns a complete description of the objective like {@link #toCompleteString()} optionally
   * suppressing the priority from the output.
   *
   * <p>The priority is only relevant for problems with more than one objective.
   */
  public final String toCompleteString(boolean printPriority) {
    var result = new StringBuilder();
    result.append(this);
    result.append(": ");
    if (maximize) {
      result.append("maximize ");
    } else {
      result.append("minimize ");
    }
    boolean isStart = true;
    if (!linearTerms.isEmpty()) {
      result.append(Variable.linearTermsToShortString(linearTerms));
      isStart = false;
    }
    if (offset != 0.0) {
      if (isStart) {
        result.append(offset);
      } else {
        // use < instead of > for better printing of NaN.
        result.append(offset < 0 ? " - " : " + ").append(Math.abs(offset));
      }
      isStart = false;
    }
    if (isStart) {
      result.append("0.0");
    }
    if (printPriority) {
      result.append(" priority: ").append(priority);
    }
    if (isDeleted()) {
      result.append(" (deleted)");
    }
    return result.toString();
  }

  /**
   * To be called when {@code variable} is deleted from the model, deletes all coefficients with
   * {@code variable} from the objective.
   */
  final void onVariableDeleted(Variable variable) {
    linearTerms.remove(variable);
  }

  /** Identifies which model this objective belongs to. */
  final ModelId getModelId() {
    return modelId;
  }

  /** Returns a protocol buffer equivalent to {@code this}. */
  final ObjectiveProto toProto() {
    ObjectiveProto.Builder builder = ObjectiveProto.newBuilder();
    builder.setMaximize(maximize);
    builder.setOffset(offset);
    builder.setLinearCoefficients(SparseContainers.toSparseDoubleVectorProto(linearTerms));
    builder.setPriority(priority);
    if (!name.isEmpty()) {
      builder.setName(name);
    }
    return builder.build();
  }

  /**
   * If this objective has been deleted from the model.
   *
   * <p>Note: the primary objective cannot be deleted, so we always return false for the primary
   * objective {@link AuxiliaryObjective}, a subclass, can be deleted (and overrides this method).
   */
  boolean isDeleted() {
    return false;
  }

  /**
   * A callback for all modifications to an objective.
   *
   * <p>The objective is updated before these functions are invoked. We may update multiple parts of
   * the objective and then invoke this function multiple times.
   */
  interface OnChangeListener {
    void onSenseChanged(Objective objective);

    void onOffsetChanged(Objective objective);

    // NOTE: the implementer of this callback could query coefficient directly, but we provide it to
    // avoid an extra hash lookup.
    void onLinearTermChange(Objective objective, Variable variable, double coefficient);

    void onPriorityChanged(Objective objective);
  }

  Objective(long priority, String name, OnChangeListener listener, ModelId modelId) {
    Preconditions.checkArgument(priority >= 0, "priority should be nonnegative, was %s", priority);
    this.priority = priority;
    this.name = name;
    this.listener = listener;
    this.modelId = modelId;
  }

  private void checkVariableSameModel(Variable variable) {
    Preconditions.checkArgument(variable.getModelId() == modelId,
        "Variable %s is not from this model (named: %s), it is from another model (named: %s).",
        variable, modelId.getName(), variable.getModelId().getName());
  }

  private final class Sink implements LinearExpressionSink {
    /**
     * Adds {@code scale} * {@code variable} to {@code Objective.this}.
     *
     * @throws IllegalArgumentException if the objective is already deleted, if {@code variable} is
     *     already deleted, or if {@code variable} is from a different model.
     */
    @Override
    public void add(double scale, Variable variable) {
      setLinearTerm(variable, getLinearTerm(variable) + scale);
    }

    /**
     * Adds {@code constant} to {@code Objective.this}.
     *
     * @throws IllegalArgumentException if the objective was deleted.
     */
    @Override
    public void add(double constant) {
      setOffset(getOffset() + constant);
    }
  }
}
