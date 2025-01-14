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

package com.google.ortools.modelbuilder;

/** Wrapper around a linear constraint stored in the ModelBuilderHelper instance. */
public class EnforcedLinearConstraint {
  public EnforcedLinearConstraint(ModelBuilderHelper helper) {
    this.helper = helper;
    this.index = helper.addEnforcedLinearConstraint();
  }

  EnforcedLinearConstraint(ModelBuilderHelper helper, int index) {
    if (!helper.isEnforcedConstraint(index)) {
      throw new IllegalArgumentException(
          "the given index does not refer to an enforced linear constraint");
    }
    this.helper = helper;
    this.index = index;
  }

  /** Returns the index of the constraint in the model. */
  public int getIndex() {
    return index;
  }

  /** Returns the constraint builder. */
  public ModelBuilderHelper getHelper() {
    return helper;
  }

  /** Returns the lower bound of the constraint. */
  public double getLowerBound() {
    return helper.getEnforcedConstraintLowerBound(index);
  }

  /** Sets the lower bound of the constraint. */
  public void setLowerBound(double lb) {
    helper.setEnforcedConstraintLowerBound(index, lb);
  }

  /** Returns the upper bound of the constraint. */
  public double getUpperBound() {
    return helper.getEnforcedConstraintUpperBound(index);
  }

  /** Sets the upper bound of the constraint. */
  public void setUpperBound(double ub) {
    helper.setEnforcedConstraintUpperBound(index, ub);
  }

  /** Returns the name of the constraint given upon creation. */
  public String getName() {
    return helper.getEnforcedConstraintName(index);
  }

  // Sets the name of the constraint. */
  public void setName(String name) {
    helper.setEnforcedConstraintName(index, name);
  }

  /** Adds var * coeff to the constraint. */
  public void addTerm(Variable v, double coeff) {
    helper.safeAddEnforcedConstraintTerm(index, v.getIndex(), coeff);
  }

  /** Sets the coefficient of v to coeff, adding or removing a term if needed. */
  public void setCoefficient(Variable v, double coeff) {
    helper.setEnforcedConstraintCoefficient(index, v.getIndex(), coeff);
  }

  /** Clear all terms. */
  public void clearTerms() {
    helper.clearEnforcedConstraintTerms(index);
  }

  /** Returns the indicator variable of the constraint/ */
  public Variable getIndicatorVariable() {
    return new Variable(helper, helper.getEnforcedIndicatorVariableIndex(index));
  }

  /** Sets the indicator variable of the constraint. */
  public void setIndicatorVariable(Variable v) {
    helper.setEnforcedIndicatorVariableIndex(index, v.index);
  }

  /** Returns the indicator value of the constraint. */
  public boolean getIndicatorValue() {
    return helper.getEnforcedIndicatorValue(index);
  }

  /** Sets the indicator value of the constraint. */
  public void setIndicatorValue(boolean b) {
    helper.setEnforcedIndicatorValue(index, b);
  }

  /** Inline setter */
  public EnforcedLinearConstraint withName(String name) {
    setName(name);
    return this;
  }

  private final ModelBuilderHelper helper;
  private final int index;
}
