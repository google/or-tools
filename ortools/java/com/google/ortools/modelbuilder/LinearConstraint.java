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
public class LinearConstraint {
  public LinearConstraint(ModelBuilderHelper helper) {
    this.helper = helper;
    this.index = helper.addLinearConstraint();
  }

  LinearConstraint(ModelBuilderHelper helper, int index) {
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
    return helper.getConstraintLowerBound(index);
  }

  /** Returns the lower bound of the constraint. */
  public void setLowerBound(double lb) {
    helper.setConstraintLowerBound(index, lb);
  }

  /** Returns the upper bound of the constraint. */
  public double getUpperBound() {
    return helper.getConstraintUpperBound(index);
  }

  /** Returns the upper bound of the constraint. */
  public void setUpperBound(double ub) {
    helper.setConstraintUpperBound(index, ub);
  }

  /** Returns the name of the constraint given upon creation. */
  public String getName() {
    return helper.getConstraintName(index);
  }

  /** Sets the name of the constraint. */
  public void setName(String name) {
    helper.setConstraintName(index, name);
  }

  /** Adds var * coeff to the constraint. */
  public void addTerm(Variable v, double coeff) {
    helper.addConstraintTerm(index, v.getIndex(), coeff);
  }

  /** Sets the coefficient of v to coeff, adding or removing a term if needed. */
  public void setCoefficient(Variable v, double coeff) {
    helper.setConstraintCoefficient(index, v.getIndex(), coeff);
  }

  /** Clear all terms. */
  public void clearTerms() {
    helper.clearConstraintTerms(index);
  }

  /** Inline setter */
  public LinearConstraint withName(String name) {
    setName(name);
    return this;
  }

  private final ModelBuilderHelper helper;
  private final int index;
}
