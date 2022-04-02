// Copyright 2010-2021 Google LLC
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

/** Model solver class */
public final class ModelSolver {
  static class ModelSolverException extends RuntimeException {
    public ModelSolverException(String methodName, String msg) {
      // Call constructor of parent Exception
      super(methodName + ": " + msg);
    }
  }

  public ModelSolver(String solverName) {
    this.helper = new ModelSolverHelper(solverName);
  }

  public SolveStatus solve(ModelBuilder model) {
    helper.solve(model.getHelper());
    if (!helper.hasResponse()) {
      return SolveStatus.UNKNOWN_STATUS;
    }
    return helper.getStatus();
  }

  public double objectiveValue() {
    if (!helper.hasSolution()) {
      throw new ModelSolverException(
          "ModelSolver.objectiveValue()", "solve() was not called or no solution was found");
    }
    return helper.getObjectiveValue();
  }

  public double value(Variable var) {
    if (!helper.hasSolution()) {
      throw new ModelSolverException(
          "ModelSolver.value())", "solve() was not called or no solution was found");
    }
    return helper.getVariableValue(var.getIndex());
  }

  public double wallTime() {
    return helper.getWallTime();
  }

  ModelSolverHelper helper;
}
