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

/** An integer variable. */
public class Variable implements LinearArgument {
  Variable(ModelBuilderHelper helper, double lb, double ub, boolean isIntegral, String name) {
    this.helper = helper;
    this.index = helper.addVar();
    this.helper.setVarName(index, name);
    this.helper.setVarIntegrality(index, isIntegral);
    this.helper.setVarLowerBound(index, lb);
    this.helper.setVarUpperBound(index, ub);
  }

  Variable(ModelBuilderHelper helper, int index) {
    this.helper = helper;
    this.index = index;
  }

  /** Returns the index of the variable in the underlying ModelBuilderHelper. */
  public int getIndex() {
    return index;
  }

  // LinearArgument interface
  @Override
  public LinearExpr build() {
    return new AffineExpression(index, 1.0, 0.0);
  }

  /** Returns the lower bound of the variable. */
  public double getLowerBound() {
    return helper.getVarLowerBound(index);
  }

  /** Sets the lower bound of the variable. */
  public void setLowerBound(double lowerBound) {
    helper.setVarLowerBound(index, lowerBound);
  }

  /** Returns the upper bound of the variable. */
  public double getUpperBound() {
    return helper.getVarUpperBound(index);
  }

  /** Sets the upper bound of the variable. */
  public void setUpperBound(double upperBound) {
    helper.setVarUpperBound(index, upperBound);
  }

  /** Returns whether the variable is integral. */
  public boolean getIntegrality() {
    return helper.getVarIntegrality(index);
  }

  /** Sets the integrality of the variable. */
  public void setIntegrality(boolean isIntegral) {
    helper.setVarIntegrality(index, isIntegral);
  }

  /** Returns the objective coefficient of the variable. */
  public double getObjectiveCoefficient() {
    return helper.getVarObjectiveCoefficient(index);
  }

  /** Sets the objective coefficient of the variable in the objective. */
  public void setObjectiveCoefficient(double objectiveCoefficient) {
    helper.setVarObjectiveCoefficient(index, objectiveCoefficient);
  }

  /** Returns the name of the variable given upon creation. */
  public String getName() {
    return helper.getVarName(index);
  }

  public void setName(String name) {
    helper.setVarName(index, name);
  }

  @Override
  public String toString() {
    if (getName().isEmpty()) {
      if (getLowerBound() == getUpperBound()) {
        return String.format("%f", getLowerBound());
      } else {
        return String.format("var_%d(%f, %f)", getIndex(), getLowerBound(), getUpperBound());
      }
    } else {
      return String.format("%s(%f, %f)", getName(), getLowerBound(), getUpperBound());
    }
  }

  protected final ModelBuilderHelper helper;
  protected final int index;
}
