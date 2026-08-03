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
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;
import java.util.Comparator;
import java.util.Map;

/**
 * A decision variable in a MathOpt optimization model.
 *
 * <p>Create variables with {@link Model#addVariable(double, double, boolean, String)}, you cannot
 * construct a {@link Variable} directly.
 *
 * <p>In a feasible solution to the optimization model, the variable will take a value in [{@link
 * #lowerBound}, {@link #upperBound}]. Further, this value should be an integer (at least up to the
 * solver's numerical tolerances) if {@link #integer} is true.
 *
 * <p>Once a Variable has been deleted from the model (by {@link Model#deleteVariable(Variable)}),
 * you can still call getters, but calling any setters will result in a {@link RuntimeException}.
 * Likewise, most methods on Model that take a Variable will fail.
 */
public final class Variable implements ModelElement, LinearExpressionView {
  private double lowerBound;
  private double upperBound;
  private boolean integer;
  private final long id;
  private final String name;
  private boolean deleted = false;
  private final OnChangeListener onChangeListener;
  private final ModelId modelId;

  @Override
  public long getId() {
    return id;
  }

  public String getName() {
    return name;
  }

  public boolean isInteger() {
    return integer;
  }

  public double getLowerBound() {
    return lowerBound;
  }

  public double getUpperBound() {
    return upperBound;
  }

  public boolean isDeleted() {
    return deleted;
  }

  public boolean hasSameModel(Variable other) {
    // All variables created in the same model will receive the same Parent callback at construction
    // time, so we can use object identify of the callback to check the identity of the model.
    return modelId == other.modelId;
  }

  /**
   * Sets {@link #lowerBound} to {@code value} and returns this, or throws a {@link
   * RuntimeException} if this variable has already been deleted.
   */
  public Variable setLowerBound(double value) {
    Preconditions.checkArgument(!deleted, "variable %s was deleted", this);
    if (value != lowerBound) {
      lowerBound = value;
      onChangeListener.onLowerBoundChanged(this);
    }
    return this;
  }

  /**
   * Sets {@link #upperBound} to {@code value} and returns this, or throws a {@link
   * RuntimeException} if this variable has already been deleted.
   */
  public Variable setUpperBound(double value) {
    Preconditions.checkArgument(!deleted, "variable %s was deleted", this);
    if (value != upperBound) {
      upperBound = value;
      onChangeListener.onUpperBoundChanged(this);
    }
    return this;
  }

  /**
   * Sets {@link #integer} to {@code value} and returns this, or throws a {@link RuntimeException}
   * if this variable has already been deleted.
   */
  public Variable setInteger(boolean isInteger) {
    Preconditions.checkArgument(!deleted, "variable %s was deleted", this);
    if (isInteger != integer) {
      integer = isInteger;
      onChangeListener.onIntegerChanged(this);
    }
    return this;
  }

  /** Returns a complete description of this variable containing both bounds and integrality. */
  public String toCompleteString() {
    StringBuilder result = new StringBuilder(toString());
    if (integer && lowerBound == 0.0 && upperBound == 1.0) {
      result.append(" (binary)");
    } else {
      if (integer) {
        result.append(" (integer)");
      }
      result.append(" in [").append(lowerBound).append(", ").append(upperBound).append("]");
    }
    if (deleted) {
      result.append(" (was deleted)");
    }
    return result.toString();
  }

  private static String termToString(double coef, Variable var) {
    double absCoef = Math.abs(coef);
    if (absCoef == 1.0) {
      return var.toString();
    }
    return Math.abs(coef) + " * " + var;
  }

  /**
   * Returns a human-readable string of the sum of the product of the entries in terms.
   *
   * <p>E.g. if terms has elements {x, 1.0} and {y, 4.0}, returns "x + 4.0 * y".
   *
   * <p>Elements of terms with coefficient zero are not included.
   */
  public static String linearTermsToShortString(Map<Variable, Double> terms) {
    var termsString = new StringBuilder();
    ImmutableList<Variable> varsInOrder =
        ImmutableList.sortedCopyOf(Comparator.comparingLong(Variable::getId), terms.keySet());
    boolean isFirst = true;
    for (Variable var : varsInOrder) {
      double coef = terms.get(var);
      if (coef == 0.0) {
        continue;
      }
      if (isFirst) {
        isFirst = false;
        if (coef < 0) {
          termsString.append("-");
        }
        termsString.append(termToString(coef, var));
      } else { // not the first term
        termsString.append(coef < 0 ? " - " : " + ");
        termsString.append(termToString(coef, var));
      }
    }
    return termsString.toString();
  }

  /** Returns the name of the variable, or a short string containing the id if the name is empty. */
  @Override
  public String toString() {
    return name.isEmpty() ? ("__var#" + id + "__") : name;
  }

  /** Instead see {@link Model#deleteVariable(Variable)}. */
  void markDeleted() {
    deleted = true;
  }

  ModelId getModelId() {
    return modelId;
  }

  void appendToProto(VariablesProto.Builder variablesProto) {
    variablesProto.addLowerBounds(lowerBound);
    variablesProto.addUpperBounds(upperBound);
    variablesProto.addIntegers(integer);
    variablesProto.addNames(name);
    variablesProto.addIds(id);
  }

  @Override
  public double offset() {
    return 0.0;
  }

  @Override
  public ImmutableSet<Map.Entry<Variable, Double>> unmodifiableTerms() {
    return ImmutableMap.of(this, 1.0).entrySet();
  }

  @Override
  public void addTo(double scale, LinearExpressionSink sink) {
    sink.add(scale, this);
  }

  interface OnChangeListener {
    void onLowerBoundChanged(Variable variable);

    void onUpperBoundChanged(Variable variable);

    void onIntegerChanged(Variable variable);
  }

  /** Instead see {@link Model#addVariable(double, double, boolean, String)}. */
  Variable(double lowerBound, double upperBound, boolean isInteger, String name, long id,
      OnChangeListener onChangeListener, ModelId modelId) {
    this.lowerBound = lowerBound;
    this.upperBound = upperBound;
    integer = isInteger;
    this.name = name;
    this.id = id;
    this.onChangeListener = onChangeListener;
    this.modelId = modelId;
  }
}
