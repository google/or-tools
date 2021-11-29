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

// Mixed Integer programming example that shows how to use the API.
// [START program]
// [START import]
#include "ortools/linear_solver/linear_solver.h"
// [END import]

namespace operations_research {
void SimpleMipProgram() {
  // [START solver]
  // Create the mip solver with the SCIP backend.
  std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver("SCIP"));
  if (!solver) {
    LOG(WARNING) << "SCIP solver unavailable.";
    return;
  }
  // [END solver]

  // [START variables]
  const double infinity = solver->infinity();
  // x and y are integer non-negative variables.
  MPVariable* const x = solver->MakeIntVar(0.0, infinity, "x");
  MPVariable* const y = solver->MakeIntVar(0.0, infinity, "y");

  LOG(INFO) << "Number of variables = " << solver->NumVariables();
  // [END variables]

  // [START constraints]
  // x + 7 * y <= 17.5.
  MPConstraint* const c0 = solver->MakeRowConstraint(-infinity, 17.5, "c0");
  c0->SetCoefficient(x, 1);
  c0->SetCoefficient(y, 7);

  // x <= 3.5.
  MPConstraint* const c1 = solver->MakeRowConstraint(-infinity, 3.5, "c1");
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 0);

  LOG(INFO) << "Number of constraints = " << solver->NumConstraints();
  // [END constraints]

  // [START objective]
  // Maximize x + 10 * y.
  MPObjective* const objective = solver->MutableObjective();
  objective->SetCoefficient(x, 1);
  objective->SetCoefficient(y, 10);
  objective->SetMaximization();
  // [END objective]

  // [START solve]
  const MPSolver::ResultStatus result_status = solver->Solve();
  // Check that the problem has an optimal solution.
  if (result_status != MPSolver::OPTIMAL) {
    LOG(FATAL) << "The problem does not have an optimal solution!";
  }
  // [END solve]

  // [START print_solution]
  LOG(INFO) << "Solution:";
  LOG(INFO) << "Objective value = " << objective->Value();
  LOG(INFO) << "x = " << x->solution_value();
  LOG(INFO) << "y = " << y->solution_value();
  // [END print_solution]

  // [START advanced]
  LOG(INFO) << "\nAdvanced usage:";
  LOG(INFO) << "Problem solved in " << solver->wall_time() << " milliseconds";
  LOG(INFO) << "Problem solved in " << solver->iterations() << " iterations";
  LOG(INFO) << "Problem solved in " << solver->nodes()
            << " branch-and-bound nodes";
  // [END advanced]
}
}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::SimpleMipProgram();
  return EXIT_SUCCESS;
}
// [END program]
