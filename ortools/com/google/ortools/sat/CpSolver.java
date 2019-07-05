// Copyright 2010-2018 Google LLC
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

import com.google.ortools.sat.CpSolverResponse;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.SatParameters;

/**
 * Wrapper around the SAT solver.
 *
 * <p>This class proposes different solve() methods, as well as accessors to get the values of
 * variables in the best solution, as well as general statistics of the search.
 */
public final class CpSolver {
  /** Main construction of the CpSolver class. */
  public CpSolver() {
    this.solveParameters = SatParameters.newBuilder();
  }

  /** Solves the given module, and returns the solve status. */
  public CpSolverStatus solve(CpModel model) {
    solveResponse = SatHelper.solveWithParameters(model.model(), solveParameters.build());
    return solveResponse.getStatus();
  }

  /** Solves a problem and passes each solution found to the callback. */
  public CpSolverStatus solveWithSolutionCallback(CpModel model, CpSolverSolutionCallback cb) {
    solveResponse = SatHelper.solveWithParametersAndSolutionCallback(
        model.model(), solveParameters.build(), cb);
    return solveResponse.getStatus();
  }

  /**
   * Searches for all solutions of a satisfiability problem.
   *
   * <p>This method searches for all feasible solutions of a given model. Then it feeds the
   * solutions to the callback.
   *
   * <p>Note that the model cannot have an objective.
   *
   * @param model the model to solve
   * @param cb the callback that will be called at each solution
   * @return the status of the solve (FEASIBLE, INFEASIBLE...)
   */
  public CpSolverStatus searchAllSolutions(CpModel model, CpSolverSolutionCallback cb) {
    solveParameters.setEnumerateAllSolutions(true);
    solveResponse = SatHelper.solveWithParametersAndSolutionCallback(
        model.model(), solveParameters.build(), cb);
    solveParameters.setEnumerateAllSolutions(true);
    return solveResponse.getStatus();
  }

  /** Returns the best objective value found during search. */
  public double objectiveValue() {
    return solveResponse.getObjectiveValue();
  }

  /**
   * Returns the best lower bound found when minimizing, of the best upper bound found when
   * maximizing.
   */
  public double bestObjectiveBound() {
    return solveResponse.getBestObjectiveBound();
  }

  /** Returns the value of a variable in the last solution found. */
  public long value(IntVar var) {
    return solveResponse.getSolution(var.getIndex());
  }

  /** Returns the Boolean value of a literal in the last solution found. */
  public Boolean booleanValue(Literal var) {
    int index = var.getIndex();
    if (index >= 0) {
      return solveResponse.getSolution(index) != 0;
    } else {
      return solveResponse.getSolution(-index - 1) == 0;
    }
  }

  /** Returns the internal response protobuf that is returned internally by the SAT solver. */
  public CpSolverResponse response() {
    return solveResponse;
  }

  /** Returns the number of branches explored during search. */
  public long numBranches() {
    return solveResponse.getNumBranches();
  }

  /** Returns the number of conflicts created during search. */
  public long numConflicts() {
    return solveResponse.getNumConflicts();
  }

  /** Returns the wall time of the search. */
  public double wallTime() {
    return solveResponse.getWallTime();
  }

  /** Returns the user time of the search. */
  public double userTime() {
    return solveResponse.getUserTime();
  }

  /** Returns the builder of the parameters of the SAT solver for modification. */
  public SatParameters.Builder getParameters() {
    return solveParameters;
  }

  /** Returns some statistics on the solution found as a string. */
  public String responseStats() {
    return SatHelper.solverResponseStats(solveResponse);
  }

  private CpSolverResponse solveResponse;
  private final SatParameters.Builder solveParameters;
}
