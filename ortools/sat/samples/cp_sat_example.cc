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

// [START program]
// [START import]
#include <algorithm>

#include "ortools/sat/cp_model.h"
// [END import]

namespace operations_research {
namespace sat {

void CpSatExample() {
  // [START model]
  CpModelBuilder cp_model;
  // [END model]

  // [START variables]
  int64_t var_upper_bound = std::max({50, 45, 37});
  const Domain domain(0, var_upper_bound);
  const IntVar x = cp_model.NewIntVar(domain).WithName("x");
  const IntVar y = cp_model.NewIntVar(domain).WithName("y");
  const IntVar z = cp_model.NewIntVar(domain).WithName("z");
  // [END variables]

  // [START constraints]
  cp_model.AddLessOrEqual(2 * x + 7 * y + 3 * z, 50);
  cp_model.AddLessOrEqual(3 * x - 5 * y + 7 * z, 45);
  cp_model.AddLessOrEqual(5 * x + 2 * y - 6 * z, 37);
  // [END constraints]

  // [START objective]
  cp_model.Maximize(2 * x + 2 * y + 3 * z);
  // [END objective]

  // Solving part.
  // [START solve]
  const CpSolverResponse response = Solve(cp_model.Build());
  // [END solve]

  // [START print_solution]
  if (response.status() == CpSolverStatus::OPTIMAL ||
      response.status() == CpSolverStatus::FEASIBLE) {
    // Get the value of x in the solution.
    LOG(INFO) << "Maximum of objective function: "
              << response.objective_value();
    LOG(INFO) << "x = " << SolutionIntegerValue(response, x);
    LOG(INFO) << "y = " << SolutionIntegerValue(response, y);
    LOG(INFO) << "z = " << SolutionIntegerValue(response, z);
  } else {
    LOG(INFO) << "No solution found.";
  }
  // [END print_solution]

  // Statistics.
  // [START statistics]
  LOG(INFO) << "Statistics";
  LOG(INFO) << CpSolverResponseStats(response);
  // [END statistics]
}

}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::CpSatExample();
  return EXIT_SUCCESS;
}
// [END program]
