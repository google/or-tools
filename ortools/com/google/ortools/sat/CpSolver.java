// Copyright 2010-2017 Google
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

import com.google.ortools.sat.CpModelProto;
import com.google.ortools.sat.CpSolverResponse;
import com.google.ortools.sat.CpSolverStatus;
import com.google.ortools.sat.SatParameters;

/**
 * Wrapper around the SAT solver.
 *
 * <p>This class proposes different solve() methods, as well as accessors to get the values of
 * variables in the best solution, as well as general statistics of the search.
 */
public class CpSolver {
  /** Main constructionof the CpSolver class. */
  public CpSolver() {
    this.parameters_ = SatParameters.newBuilder();
  }

  /** Solves the given module, and returns the solve status. */
  public CpSolverStatus solve(CpModel model) {
    response_ = SatHelper.solveWithParameters(model.model(), parameters_.build());
    return response_.getStatus();
  }

  /** Solves a problem and pass each solution found to the callback. */
  public CpSolverStatus solveWithSolutionCallback(CpModel model, CpSolverSolutionCallback cb) {
    response_ =
        SatHelper.solveWithParametersAndSolutionCallback(model.model(), parameters_.build(), cb);
    return response_.getStatus();
  }

  /**
   * Search for all solutions of a satisfiability problem.
   *
   * <p>This method searches for all feasible solution of a given model. Then it feeds the solution
   * to the callback.
   *
   * @param model the model to solve.
   * @param cb the callback that will be called at each solution.
   * @return the status of the solve (FEASIBLE, INFEASIBLE...).
   */
  public CpSolverStatus searchAllSolutions(CpModel model, CpSolverSolutionCallback cb) {
    parameters_.setEnumerateAllSolutions(true);
    response_ =
        SatHelper.solveWithParametersAndSolutionCallback(model.model(), parameters_.build(), cb);
    parameters_.setEnumerateAllSolutions(true);
    return response_.getStatus();
  }

  /** The best objective value found during search. */
  public double objectiveValue() {
    return response_.getObjectiveValue();
  }

  /** The value of a variable in the last solution found. */
  public long value(IntVar var) {
    return response_.getSolution(var.getIndex());
  }

  /** The Boolean value of a literal in the last solution found. */
  public Boolean booleanValue(ILiteral var) {
    int index = var.getIndex();
    if (index >= 0) {
      return response_.getSolution(index) != 0;
    } else {
      return response_.getSolution(-index - 1) == 0;
    }
  }

  /** The internal response protobuf that is returned internally by the SAT solver. */
  public CpSolverResponse response() {
    return response_;
  }

  /** The number of branches explored during search. */
  public long numBranches() {
    return response_.getNumBranches();
  }

  /** The number of conflicts created during search. */
  public long numConflicts() {
    return response_.getNumConflicts();
  }

  /** The wall time of the search. */
  public double wallTime() {
    return response_.getWallTime();
  }

  /** The user time of the search. */
  public double userTime() {
    return response_.getUserTime();
  }

  /** Returns the builder of the parameters of the SAT solver for modification. */
  public SatParameters.Builder getParameters() {
    return parameters_;
  }

  private CpModelProto model_;
  private CpSolverResponse response_;
  private SatParameters.Builder parameters_;
}
