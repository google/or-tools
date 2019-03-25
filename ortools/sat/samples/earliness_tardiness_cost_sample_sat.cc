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

void EarlinessTardinessCostSampleSat() {
  const int64 kEarlinessDate = 5;
  const int64 kEarlinessCost = 8;
  const int64 kLatenessDate = 15;
  const int64 kLatenessCost = 12;

  // Create the CP-SAT model.
  CpModelBuilder cp_model;

  // Declare our primary variable.
  const IntVar x = cp_model.NewIntVar({0, 20});

  // Create the expression variable and implement the piecewise linear function.
  //
  //  \        /
  //   \______/
  //   ed    ld
  //
  const int64 kLargeConstant = 1000;
  const IntVar expr = cp_model.NewIntVar({0, kLargeConstant});

  // First segment.
  const IntVar s1 = cp_model.NewIntVar({-kLargeConstant, kLargeConstant});
  cp_model.AddEquality(s1, LinearExpr::ScalProd({x}, {-kEarlinessCost})
                               .AddConstant(kEarlinessCost * kEarlinessDate));

  // Second segment.
  const IntVar s2 = cp_model.NewConstant(0);

  // Third segment.
  const IntVar s3 = cp_model.NewIntVar({-kLargeConstant, kLargeConstant});
  cp_model.AddEquality(s3, LinearExpr::ScalProd({x}, {kLatenessCost})
                               .AddConstant(-kLatenessCost * kLatenessDate));

  // Link together expr and x through s1, s2, and s3.
  cp_model.AddMaxEquality(expr, {s1, s2, s3});

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
    LOG(INFO) << "x=" << SolutionIntegerValue(r, x) << " expr"
              << SolutionIntegerValue(r, expr);
  }));
  SolveWithModel(cp_model, &model);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::EarlinessTardinessCostSampleSat();

  return EXIT_SUCCESS;
}
