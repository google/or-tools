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
#include <iostream>

#include "ortools/linear_solver/linear_solver.h"
// [END import]

namespace operations_research {
void IntegerProgrammingExample() {
  // [START solver]
  // Create the mip solver with the SCIP backend.
  std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver("SCIP"));
  if (!solver) {
    LOG(WARNING) << "SCIP solver unavailable.";
    return;
  }
  // [END solver]

  // [START variables]
  // x, y, and z are non-negative integer variables.
  MPVariable* const x = solver->MakeIntVar(0.0, solver->infinity(), "x");
  MPVariable* const y = solver->MakeIntVar(0.0, solver->infinity(), "y");
  MPVariable* const z = solver->MakeIntVar(0.0, solver->infinity(), "z");
  LOG(INFO) << "Number of variables = " << solver->NumVariables();
  // [END variables]

  // [START constraints]
  // 2*x + 7*y + 3*z <= 50
  MPConstraint* const constraint0 =
      solver->MakeRowConstraint(-solver->infinity(), 50);
  constraint0->SetCoefficient(x, 2);
  constraint0->SetCoefficient(y, 7);
  constraint0->SetCoefficient(z, 3);

  // 3*x - 5*y + 7*z <= 45
  MPConstraint* const constraint1 =
      solver->MakeRowConstraint(-solver->infinity(), 45);
  constraint1->SetCoefficient(x, 3);
  constraint1->SetCoefficient(y, -5);
  constraint1->SetCoefficient(z, 7);

  // 5*x + 2*y - 6*z <= 37
  MPConstraint* const constraint2 =
      solver->MakeRowConstraint(-solver->infinity(), 37);
  constraint2->SetCoefficient(x, 5);
  constraint2->SetCoefficient(y, 2);
  constraint2->SetCoefficient(z, -6);
  LOG(INFO) << "Number of constraints = " << solver->NumConstraints();
  // [END constraints]

  // [START objective]
  // Maximize 2*x + 2*y + 3*z
  MPObjective* const objective = solver->MutableObjective();
  objective->SetCoefficient(x, 2);
  objective->SetCoefficient(y, 2);
  objective->SetCoefficient(z, 3);
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
  LOG(INFO) << "Optimal objective value = " << objective->Value();
  LOG(INFO) << x->name() << " = " << x->solution_value();
  LOG(INFO) << y->name() << " = " << y->solution_value();
  LOG(INFO) << z->name() << " = " << z->solution_value();
  // [END print_solution]
}
}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::IntegerProgrammingExample();
  return EXIT_SUCCESS;
}
// [END program]
