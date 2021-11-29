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

// Minimal example to call the GLOP solver.
// [START program]
// [START import]
#include "ortools/linear_solver/linear_solver.h"
// [END import]

namespace operations_research {
void BasicExample() {
  // [START solver]
  // Create the linear solver with the GLOP backend.
  std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver("GLOP"));
  // [END solver]

  // [START variables]
  // Create the variables x and y.
  MPVariable* const x = solver->MakeNumVar(0.0, 1, "x");
  MPVariable* const y = solver->MakeNumVar(0.0, 2, "y");

  LOG(INFO) << "Number of variables = " << solver->NumVariables();
  // [END variables]

  // [START constraints]
  // Create a linear constraint, 0 <= x + y <= 2.
  MPConstraint* const ct = solver->MakeRowConstraint(0.0, 2.0, "ct");
  ct->SetCoefficient(x, 1);
  ct->SetCoefficient(y, 1);

  LOG(INFO) << "Number of constraints = " << solver->NumConstraints();
  // [END constraints]

  // [START objective]
  // Create the objective function, 3 * x + y.
  MPObjective* const objective = solver->MutableObjective();
  objective->SetCoefficient(x, 3);
  objective->SetCoefficient(y, 1);
  objective->SetMaximization();
  // [END objective]

  // [START solve]
  solver->Solve();
  // [END solve]

  // [START print_solution]
  LOG(INFO) << "Solution:" << std::endl;
  LOG(INFO) << "Objective value = " << objective->Value();
  LOG(INFO) << "x = " << x->solution_value();
  LOG(INFO) << "y = " << y->solution_value();
  // [END print_solution]
}
}  // namespace operations_research

int main() {
  operations_research::BasicExample();
  return EXIT_SUCCESS;
}
// [END program]
