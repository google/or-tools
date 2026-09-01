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

import java.time.Duration;
import java.util.function.Consumer;

/** Model solver class */
public final class ModelSolver {
  static class ModelSolverException extends RuntimeException {
    public ModelSolverException(String methodName, String msg) {
      // Call constructor of parent Exception
      super(methodName + ": " + msg);
    }
  }

  /** Creates the solver with the supplied solver backend. */
  public ModelSolver(String solverName) {
    this.helper = new ModelSolverHelper(solverName);
    this.logCallback = null;
  }

  /** Solves given model, and returns the status of the response. */
  public SolveStatus solve(ModelBuilder model) {
    if (logCallback == null) {
      helper.clearLogCallback();
    } else {
      helper.setLogCallback(logCallback);
    }
    helper.solve(model.getHelper());
    if (!helper.hasResponse()) {
      return SolveStatus.UNKNOWN_STATUS;
    }
    return helper.getStatus();
  }

  /** Enables or disables the underlying solver output. */
  public void enableOutput(boolean enable) {
    helper.enableOutput(enable);
  }

  /** Sets the time limit for the solve in seconds. */
  public void setTimeLimit(Duration limit) {
    helper.setTimeLimitInSeconds((double) limit.toMillis() / 1000.0);
  }

  /** Sets solver specific parameters as string. */
  public void setSolverSpecificParameters(String parameters) {
    helper.setSolverSpecificParameters(parameters);
  }

  /** Returns whether solver specified during the ctor was found and correctly installed. */
  public boolean solverIsSupported() {
    return helper.solverIsSupported();
  }

  /** Tries to interrupt the solve. Returns true if the feature is supported. */
  public boolean interruptSolve() {
    return helper.interruptSolve();
  }

  /** Returns true if solve() was called, and a response was returned. */
  public boolean hasResponse() {
    return helper.hasResponse();
  }

  /** Returns true if solve() was called, and a solution was returned. */
  public boolean hasSolution() {
    return helper.hasSolution();
  }

  /** Checks that the solver has found a solution, and returns the objective value. */
  public double getObjectiveValue() {
    if (!helper.hasSolution()) {
      throw new ModelSolverException(
          "ModelSolver.getObjectiveValue()", "solve() was not called or no solution was found");
    }
    return helper.getObjectiveValue();
  }

  /** Checks that the solver has found a solution, and returns the objective value. */
  public double getBestObjectiveBound() {
    if (!helper.hasSolution()) {
      throw new ModelSolverException(
          "ModelSolver.getBestObjectiveBound()", "solve() was not called or no solution was found");
    }
    return helper.getBestObjectiveBound();
  }

  /** Checks that the solver has found a solution, and returns the value of the given variable. */
  public double getValue(Variable var) {
    if (!helper.hasSolution()) {
      throw new ModelSolverException(
          "ModelSolver.getValue())", "solve() was not called or no solution was found");
    }
    return helper.getVariableValue(var.getIndex());
  }
  /**
   * Checks that the solver has found a solution, and returns the reduced cost of the given
   * variable.
   */
  public double getReducedCost(Variable var) {
    if (!helper.hasSolution()) {
      throw new ModelSolverException(
          "ModelSolver.getReducedCost())", "solve() was not called or no solution was found");
    }
    return helper.getReducedCost(var.getIndex());
  }

  /**
   * Checks that the solver has found a solution, and returns the dual value of the given
   * constraint.
   */
  public double getDualValue(LinearConstraint ct) {
    if (!helper.hasSolution()) {
      throw new ModelSolverException(
          "ModelSolver.getDualValue())", "solve() was not called or no solution was found");
    }
    return helper.getDualValue(ct.getIndex());
  }

  /**
   * Checks that the solver has found a solution, and returns the activity of the given constraint.
   */
  public double getActivity(LinearConstraint ct) {
    if (!helper.hasSolution()) {
      throw new ModelSolverException(
          "ModelSolver.getActivity())", "solve() was not called or no solution was found");
    }
    return helper.getActivity(ct.getIndex());
  }

  /** Sets the log callback for the solver. */
  public void setLogCallback(Consumer<String> cb) {
    this.logCallback = cb;
  }

  /** Returns the elapsed time since the creation of the solver. */
  public double getWallTime() {
    return helper.getWallTime();
  }

  /** Returns the user time since the creation of the solver. */
  public double getUserTime() {
    return helper.getUserTime();
  }

  private final ModelSolverHelper helper;
  private Consumer<String> logCallback;
}
