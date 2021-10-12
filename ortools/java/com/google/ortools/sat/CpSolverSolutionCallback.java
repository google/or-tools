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

package com.google.ortools.sat;

/**
 * Parent class to create a callback called at each solution.
 *
 * <p>From the parent class, it inherits the methods:
 *
 * <p>{@code long numBooleans()} to query the number of boolean variables created.
 *
 * <p>{@code long numBranches()} to query the number of branches explored so far.
 *
 * <p>{@code long numConflicts()} to query the number of conflicts created so far.
 *
 * <p>{@code long numBinaryPropagations()} to query the number of boolean propagations in the SAT
 * solver so far.
 *
 * <p>{@code long numIntegerPropagations()} to query the number of integer propagations in the SAT
 * solver so far.
 *
 * <p>{@code double wallTime()} to query wall time passed in the search so far.
 *
 * <p>{@code double userTime()} to query the user time passed in the search so far.
 *
 * <p>{@code long objectiveValue()} to get the best objective value found so far.
 */
public class CpSolverSolutionCallback extends SolutionCallback {
  /** Returns the value of the linear expression in the current solution. */
  public long value(LinearExpr expr) {
    long result = expr.getOffset();
    for (int i = 0; i < expr.numElements(); ++i) {
      result += solutionIntegerValue(expr.getVariable(i).getIndex()) * expr.getCoefficient(i);
    }
    return result;
  }

  /** Returns the Boolean value of the literal in the current solution. */
  public Boolean booleanValue(Literal literal) {
    return solutionBooleanValue(literal.getIndex());
  }

  /** Callback method to override. It will be called at each new solution. */
  @Override
  public void onSolutionCallback() {}
}
