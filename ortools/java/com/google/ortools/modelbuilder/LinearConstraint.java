// Copyright 2010-2022 Google LLC
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

  /** Returns the index of the constraint in the model. */
  public int getIndex() {
    return index;
  }

  /** Returns the constraint builder. */
  public ModelBuilderHelper getHelper() {
    return helper;
  }

  /** Returns the lower bound of the variable. */
  public double getLowerBound() {
    return helper.getConstraintLowerBound(index);
  }

  /** Returns the lower bound of the variable. */
  public void setLowerBound(double lb) {
    helper.setConstraintLowerBound(index, lb);
  }

  /** Returns the upper bound of the variable. */
  public double getUpperBound() {
    return helper.getConstraintUpperBound(index);
  }

  /** Returns the upper bound of the variable. */
  public void setUpperBound(double ub) {
    helper.setConstraintUpperBound(index, ub);
  }

  /** Returns the name of the variable given upon creation. */
  public String getName() {
    return helper.getConstraintName(index);
  }

  // Sets the name of the variable. */
  public void setName(String name) {
    helper.setConstraintName(index, name);
  }

  // Adds var * coeff to the constraint.
  public void addTerm(Variable var, double coeff) {
    helper.addConstraintTerm(index, var.getIndex(), coeff);
  }

  /** Inline setter */
  public LinearConstraint withName(String name) {
    setName(name);
    return this;
  }

  private final ModelBuilderHelper helper;
  private final int index;
}
