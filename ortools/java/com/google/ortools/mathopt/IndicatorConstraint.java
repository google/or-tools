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

import com.google.ortools.mathopt.IndicatorConstraintProto;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import org.jspecify.annotations.Nullable;

/**
 * An indicator constraint on the decision variables of an optimization model.
 *
 * <p>An IndicatorConstraint adds the following restriction on feasible solutions to an optimization
 * model:
 *
 * <pre>{@code
 * if z == 1 then lb <= sum_{i in I} a_i * x_i <= ub
 * }</pre>
 *
 * <p>where z is a binary decision variable (or its negation) and x_i are the decision variables of
 * the problem. Equality constraints lb == ub is allowed, which models the constraint:
 *
 * <pre>
 *   if z == 1 then sum_{i in I} a_i * x_i == b
 * </pre>
 *
 * <p>Setting lb > ub will result in an {@link IllegalArgumentException} exception (with C++ {@code
 * InvalidArgument}) at solve time.
 *
 * <p>You cannot construct an {@link IndicatorConstraint} directly, instead call {@link
 * Model#addIndicatorConstraint(Variable, boolean, double, double, LinearExpressionView, String)}.
 *
 * <p>Indicator constraints do not support in-place updates (e.g. changing the bounds or the
 * expression). They only support deletion.
 *
 * <p>When a {@link Variable} in the expression is deleted from the model by {@link
 * Model#deleteVariable(Variable)}, it is removed from the expression (effectively setting its
 * coefficient to zero). If the indicator variable is deleted, the constraint becomes either vacuous
 * (if active on zero is false) or a standard linear constraint (if active on zero is true), per
 * MathOpt semantics.
 */
public final class IndicatorConstraint implements ModelElement {
  private final long id;
  private final String name;
  private final ModelId modelId;
  private @Nullable Variable indicatorVariable;
  private final boolean activateOnZero;
  private final double lowerBound;
  private final double upperBound;
  // Mutable to support variable deletion
  private final Map<Variable, Double> impliedTerms;
  private boolean deleted = false;

  /**
   * Returns the indicator variable, or null if the variable has been deleted.
   *
   * <p>If the indicator variable is null, the constraint is ignored if {@link #getActivateOnZero()}
   * is false, or enforced unconditionally if {@link #getActivateOnZero()} is true.
   */
  public @Nullable Variable getIndicatorVariable() {
    return indicatorVariable;
  }

  /**
   * If true, the constraint is enforced when the indicator variable is 0. If false, it is enforced
   * when the indicator variable is 1.
   */
  public boolean getActivateOnZero() {
    return activateOnZero;
  }

  public double getLowerBound() {
    return lowerBound;
  }

  public double getUpperBound() {
    return upperBound;
  }

  /** Returns an unmodifiable view of the linear terms of the implied constraint. */
  public Map<Variable, Double> getUnmodifiableImpliedTerms() {
    return Collections.unmodifiableMap(impliedTerms);
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

  /** Returns a complete description of this constraint. */
  public String toCompleteString() {
    var sb = new StringBuilder();
    sb.append(this).append(": ");
    if (indicatorVariable == null) {
      sb.append("unset");
    } else {
      sb.append(indicatorVariable).append(" = ").append(activateOnZero ? "0" : "1");
    }
    sb.append(" => ").append(lowerBound).append(" ≤ ");
    if (impliedTerms.isEmpty()) {
      sb.append("0.0");
    } else {
      sb.append(Variable.linearTermsToShortString(impliedTerms));
    }
    sb.append(" ≤ ").append(upperBound);
    if (deleted) {
      sb.append(" (deleted)");
    }
    return sb.toString();
  }

  /** Returns the name of this constraint, or a short string with the id if the name is empty. */
  @Override
  public String toString() {
    return name.isEmpty() ? ("__ind_con#" + id + "__") : name;
  }

  ModelId getModelId() {
    return modelId;
  }

  void markDeleted() {
    deleted = true;
  }

  void onVariableDeleted(Variable variable) {
    impliedTerms.remove(variable);
    if (variable.equals(indicatorVariable)) {
      indicatorVariable = null;
    }
  }

  IndicatorConstraintProto toProto() {
    IndicatorConstraintProto.Builder builder = IndicatorConstraintProto.newBuilder();
    if (indicatorVariable != null) {
      builder.setIndicatorId(indicatorVariable.getId());
    }
    return builder.setActivateOnZero(activateOnZero)
        .setLowerBound(lowerBound)
        .setUpperBound(upperBound)
        .setName(name)
        .setExpression(SparseContainers.toSparseDoubleVectorProto(impliedTerms))
        .build();
  }

  IndicatorConstraint(long id, String name, ModelId modelId, Variable indicatorVariable,
      boolean activateOnZero, double lowerBound, double upperBound,
      Map<Variable, Double> impliedTerms) {
    this.id = id;
    this.name = name;
    this.modelId = modelId;
    this.indicatorVariable = indicatorVariable;
    this.activateOnZero = activateOnZero;
    this.lowerBound = lowerBound;
    this.upperBound = upperBound;
    this.impliedTerms = new HashMap<>(impliedTerms);
  }
}
