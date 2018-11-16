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

namespace operations_research {
namespace sat {

void SimpleSatProgram() {
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

  // Solving part.
  // [START solve]
  const CpSolverResponse response = Solve(cp_model);
  LOG(INFO) << CpSolverResponseStats(response);
  // [END solve]

  if (response.status() == CpSolverStatus::FEASIBLE) {
    // Get the value of x in the solution.
    LOG(INFO) << "x = " << SolutionIntegerValue(response, x);
    LOG(INFO) << "y = " << SolutionIntegerValue(response, y);
    LOG(INFO) << "z = " << SolutionIntegerValue(response, z);
  }
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::SimpleSatProgram();

  return EXIT_SUCCESS;
}
// [END program]
