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

#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

void ChannelingSampleSat() {
  // Create the CP-SAT model.
  CpModelBuilder cp_model;

  // Declare our two primary variables.
  const IntVar x = cp_model.NewIntVar({0, 10});
  const IntVar y = cp_model.NewIntVar({0, 10});

  // Declare our intermediate boolean variable.
  const BoolVar b = cp_model.NewBoolVar();

  // Implement b == (x >= 5).
  cp_model.AddGreaterOrEqual(x, 5).OnlyEnforceIf(b);
  cp_model.AddLessThan(x, 5).OnlyEnforceIf(Not(b));

  // Create our two half-reified constraints.
  // First, b implies (y == 10 - x).
  cp_model.AddEquality(LinearExpr::Sum({x, y}), 10).OnlyEnforceIf(b);
  // Second, not(b) implies y == 0.
  cp_model.AddEquality(y, 0).OnlyEnforceIf(Not(b));

  // Search for x values in increasing order.
  cp_model.AddDecisionStrategy({x}, DecisionStrategyProto::CHOOSE_FIRST,
                               DecisionStrategyProto::SELECT_MIN_VALUE);

  // Create a solver and solve with a fixed search.
  Model model;
  SatParameters parameters;
  parameters.set_search_branching(SatParameters::FIXED_SEARCH);
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "x=" << SolutionIntegerValue(r, x)
              << " y=" << SolutionIntegerValue(r, y)
              << " b=" << SolutionBooleanValue(r, b);
  }));
  SolveWithModel(cp_model, &model);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::ChannelingSampleSat();

  return EXIT_SUCCESS;
}
