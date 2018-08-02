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
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.CpSolverResponse;
import com.google.ortools.sat.Constraint;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.SatParameters;
import com.google.ortools.sat.SatHelper;

public class CpSolver {
  public CpSolver() {
    this.parameters_ = SatParameters.newBuilder();
  }

  public CpSolverStatus solve(CpModel model) {
    response_ = SatHelper.SolveWithParameters(model.model(),
                                              parameters_.build());
    return response_.getStatus();
  }

  public CpSolverStatus solveWithSolutionCallback(CpModel model,
                                                  SolutionCallback cb) {
    response_ = SatHelper.SolveWithParametersAndSolutionCallback(
        model.model(), parameters_.build(), cb);
    return response_.getStatus();
  }

  public CpSolverStatus searchAllSolutions(CpModel model,
                                           SolutionCallback cb) {
    parameters_.setEnumerateAllSolutions(true);
    response_ = SatHelper.SolveWithParametersAndSolutionCallback(
        model.model(), parameters_.build(), cb);
    parameters_.setEnumerateAllSolutions(true);
    return response_.getStatus();
  }

  public double objectiveValue() {
    return response_.getObjectiveValue();
  }

  public long value(IntVar var) {
    return response_.getSolution(var.getIndex());
  }

  public CpSolverResponse response() {
    return response_;
  }

  public Boolean booleanValue(ILiteral var) {
    int index = var.getIndex();
    if (index >= 0) {
      return response_.getSolution(index) != 0;
    } else {
      return response_.getSolution(-index - 1) == 0;
    }
  }

  public long numBranches()
  {
    return response_.getNumBranches();
  }

  public long numConflicts()
  {
    return response_.getNumConflicts();
  }

  public double wallTime()
  {
    return response_.getWallTime();
  }

  // parameters.
  public SatParameters.Builder getParameters() {
    return parameters_;
  }

  private CpModelProto model_;
  private CpSolverResponse response_;
  private SatParameters.Builder parameters_;
}
