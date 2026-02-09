// Copyright 2010-2025 Google LLC
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

// Finds a rectangle with maximum available area for given perimeter
// using AddMultiplicationEquality

// [START program]
#include <stdlib.h>

#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "ortools/base/init_google.h"
#include "ortools/base/log_severity.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"

namespace operations_research {
namespace sat {

void NonLinearSatProgram() {
  CpModelBuilder cp_model;

  const int perimeter = 20;
  const Domain sides_domain(0, perimeter);

  const IntVar x = cp_model.NewIntVar(sides_domain);
  const IntVar y = cp_model.NewIntVar(sides_domain);

  cp_model.AddEquality(2 * (x + y), perimeter);

  const Domain area_domain(0, perimeter * perimeter);
  const IntVar area = cp_model.NewIntVar(area_domain);

  cp_model.AddMultiplicationEquality(area, x, y);

  cp_model.Maximize(area);

  const CpSolverResponse response = Solve(cp_model.Build());

  if (response.status() == CpSolverStatus::OPTIMAL ||
      response.status() == CpSolverStatus::FEASIBLE) {
    // Get the value of x in the solution.
    LOG(INFO) << "x = " << SolutionIntegerValue(response, x);
    LOG(INFO) << "y = " << SolutionIntegerValue(response, y);
    LOG(INFO) << "s = " << SolutionIntegerValue(response, area);
  } else {
    LOG(INFO) << "No solution found.";
  }
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::NonLinearSatProgram();
  return EXIT_SUCCESS;
}
// [END program]
