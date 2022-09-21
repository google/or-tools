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

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Main modeling class.
 *
 * <p>Proposes a factory to create all modeling objects understood by the SAT solver.
 */
public final class ModelBuilder {
  static class ModelBuilderException extends RuntimeException {
    public ModelBuilderException(String methodName, String msg) {
      // Call constructor of parent Exception
      super(methodName + ": " + msg);
    }
  }

  /** Exception thrown when parallel arrays have mismatched lengths. */
  public static class MismatchedArrayLengths extends ModelBuilderException {
    public MismatchedArrayLengths(String methodName, String array1Name, String array2Name) {
      super(methodName, array1Name + " and " + array2Name + " have mismatched lengths");
    }
  }

  /** Exception thrown when an array has a wrong length. */
  public static class WrongLength extends ModelBuilderException {
    public WrongLength(String methodName, String msg) {
      super(methodName, msg);
    }
  }

  public ModelBuilder() {
    helper = new ModelBuilderHelper();
    constantMap = new LinkedHashMap<>();
  }

  // Integer variables.

  /** Creates a variable with domain [lb, ub]. */
  public Variable newVar(double lb, double ub, boolean isIntegral, String name) {
    return new Variable(helper, lb, ub, isIntegral, name);
  }

  /** Creates an continuous variable with domain [lb, ub]. */
  public Variable newNumVar(double lb, double ub, String name) {
    return new Variable(helper, lb, ub, false, name);
  }

  /** Creates an integer variable with domain [lb, ub]. */
  public Variable newIntVar(double lb, double ub, String name) {
    return new Variable(helper, lb, ub, true, name);
  }

  /** Creates a Boolean variable with the given name. */
  public Variable newBoolVar(String name) {
    return new Variable(helper, 0, 1, true, name);
  }

  /** Creates a constant variable. */
  public Variable newConstant(double value) {
    if (constantMap.containsKey(value)) {
      return new Variable(helper, constantMap.get(value));
    }
    Variable cste = new Variable(helper, value, value, false, ""); // bounds and name.
    constantMap.put(value, cste.getIndex());
    return cste;
  }

  /** Rebuilds a variable from its index. */
  public Variable varFromIndex(int index) {
    return new Variable(helper, index);
  }

  // Linear constraints.

  /** Adds {@code lb <= expr <= ub}. */
  public LinearConstraint addLinearConstraint(LinearArgument expr, double lb, double ub) {
    LinearConstraint lin = new LinearConstraint(helper);
    final LinearExpr e = expr.build();
    for (int i = 0; i < e.numElements(); ++i) {
      helper.addConstraintTerm(lin.getIndex(), e.getVariableIndex(i), e.getCoefficient(i));
    }
    double offset = e.getOffset();
    if (lb == Double.NEGATIVE_INFINITY || lb == Double.POSITIVE_INFINITY) {
      lin.setLowerBound(lb);
    } else {
      lin.setLowerBound(lb - offset);
    }
    if (ub == Double.NEGATIVE_INFINITY || ub == Double.POSITIVE_INFINITY) {
      lin.setUpperBound(ub);
    } else {
      lin.setUpperBound(ub - offset);
    }
    return lin;
  }

  /** Returns the number of variables in the model. */
  public int numVariables() {
    return helper.numVariables();
  }

  /** Adds {@code expr == value}. */
  public LinearConstraint addEquality(LinearArgument expr, double value) {
    return addLinearConstraint(expr, value, value);
  }

  /** Adds {@code left == right}. */
  public LinearConstraint addEquality(LinearArgument left, LinearArgument right) {
    LinearExprBuilder difference = LinearExpr.newBuilder();
    difference.addTerm(left, 1);
    difference.addTerm(right, -1);
    return addLinearConstraint(difference, 0.0, 0.0);
  }

  /** Adds {@code expr <= value}. */
  public LinearConstraint addLessOrEqual(LinearArgument expr, double value) {
    return addLinearConstraint(expr, Double.NEGATIVE_INFINITY, value);
  }

  /** Adds {@code left <= right}. */
  public LinearConstraint addLessOrEqual(LinearArgument left, LinearArgument right) {
    LinearExprBuilder difference = LinearExpr.newBuilder();
    difference.addTerm(left, 1);
    difference.addTerm(right, -1);
    return addLinearConstraint(difference, Double.NEGATIVE_INFINITY, 0.0);
  }

  /** Adds {@code expr >= value}. */
  public LinearConstraint addGreaterOrEqual(LinearArgument expr, double value) {
    return addLinearConstraint(expr, value, Double.POSITIVE_INFINITY);
  }

  /** Adds {@code left >= right}. */
  public LinearConstraint addGreaterOrEqual(LinearArgument left, LinearArgument right) {
    LinearExprBuilder difference = LinearExpr.newBuilder();
    difference.addTerm(left, 1);
    difference.addTerm(right, -1);
    return addLinearConstraint(difference, 0.0, Double.POSITIVE_INFINITY);
  }

  /** Returns the number of constraints in the model. */
  public int numConstraints() {
    return helper.numConstraints();
  }

  /** Minimize expression */
  public void minimize(LinearArgument obj) {
    optimize(obj, false);
  }

  /** Minimize expression */
  public void maximize(LinearArgument obj) {
    optimize(obj, true);
  }

  /** Sets the objective expression. */
  public void optimize(LinearArgument obj, boolean maximize) {
    helper.clearObjective();
    LinearExpr e = obj.build();
    LinkedHashMap<Integer, Double> coeffMap = new LinkedHashMap<>();
    for (int i = 0; i < e.numElements(); ++i) {
      coeffMap.merge(e.getVariableIndex(i), e.getCoefficient(i), Double::sum);
    }
    for (Map.Entry<Integer, Double> entry : coeffMap.entrySet()) {
      if (entry.getValue() != 0) {
        helper.setVarObjectiveCoefficient(entry.getKey(), entry.getValue());
      }
    }
    helper.setObjectiveOffset(e.getOffset());
    helper.setMaximize(maximize);
  }

  /** returns the objective offset. */
  double getObjectiveOffset() {
    return helper.getObjectiveOffset();
  }

  /** Sets the objective offset. */
  void setObjectiveOffset(double offset) {
    helper.setObjectiveOffset(offset);
  }

  // Model getters, import, export.

  /** Returns the name of the model. */
  public String getName() {
    return helper.getName();
  }

  /** Sets the name of the model. */
  public void setName(String name) {
    helper.setName(name);
  }

  /**
   * Write the model as a protocol buffer to 'file'.
   *
   * @param file file to write the model to. If the filename ends with 'txt', the model will be
   *     written as a text file, otherwise, the binary format will be used.
   * @return true if the model was correctly written.
   */
  public boolean exportToFile(String file) {
    return helper.writeModelToFile(file);
  }

  public String exportToMpsString(boolean obfuscate) {
    return helper.exportToMpsString(obfuscate);
  }

  public String exportToLpString(boolean obfuscate) {
    return helper.exportToLpString(obfuscate);
  }

  public boolean importFromMpsString(String mpsString) {
    return helper.importFromMpsString(mpsString);
  }

  public boolean importFromMpsFile(String mpsFile) {
    return helper.importFromMpsString(mpsFile);
  }

  public boolean importFromLpString(String lpString) {
    return helper.importFromLpString(lpString);
  }

  public boolean importFromLpFile(String lpFile) {
    return helper.importFromMpsString(lpFile);
  }

  // Getters.
  /** Returns the model builder helper. */
  public ModelBuilderHelper getHelper() {
    return helper;
  }

  private final ModelBuilderHelper helper;
  private final Map<Double, Integer> constantMap;
}
