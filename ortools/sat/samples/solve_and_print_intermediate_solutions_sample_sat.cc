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

// [START program]
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

void SolveAndPrintIntermediateSolutionsSampleSat() {
  // [START model]
  CpModelBuilder cp_model;
  // [END model]

  // [START variables]
  const Domain domain(0, 2);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");
  // [END variables]

  // [START constraints]
  cp_model.AddNotEqual(x, y);
  // [END constraints]

  // [START objective]
  cp_model.Maximize(LinearExpr::ScalProd({x, y, z}, {1, 2, 3}));
  // [END objective]

  // [START print_solution]
  Model model;
  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << "Solution " << num_solutions;
    LOG(INFO) << "  objective value = " << r.objective_value();
    LOG(INFO) << "  x = " << SolutionIntegerValue(r, x);
    LOG(INFO) << "  y = " << SolutionIntegerValue(r, y);
    LOG(INFO) << "  z = " << SolutionIntegerValue(r, z);
    num_solutions++;
  }));
  // [END print_solution]

  // [START solve]
  const CpSolverResponse response = SolveWithModel(cp_model.Build(), &model);
  // [END solve]

  LOG(INFO) << "Number of solutions found: " << num_solutions;
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::SolveAndPrintIntermediateSolutionsSampleSat();

  return EXIT_SUCCESS;
}
// [END program]
