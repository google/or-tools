// Copyright 2010-2024 Google LLC
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
// Minimal example to call the GLOP solver.
// [START import]
#include <cstdlib>
#include <memory>

#include "absl/flags/flag.h"
#include "absl/log/flags.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/init/init.h"
#include "ortools/linear_solver/linear_solver.h"
// [END import]

namespace operations_research {
void BasicExample() {
  LOG(INFO) << "Google OR-Tools version : " << OrToolsVersion::VersionString();

  // [START solver]
  // Create the linear solver with the GLOP backend.
  std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver("GLOP"));
  if (!solver) {
    LOG(WARNING) << "Could not create solver GLOP";
    return;
  }
  // [END solver]

  // [START variables]
  // Create the variables x and y.
  MPVariable* const x = solver->MakeNumVar(0.0, 1, "x");
  MPVariable* const y = solver->MakeNumVar(0.0, 2, "y");

  LOG(INFO) << "Number of variables = " << solver->NumVariables();
  // [END variables]

  // [START constraints]
  // Create a linear constraint, x + y <= 2.
  const double infinity = solver->infinity();
  MPConstraint* const ct = solver->MakeRowConstraint(-infinity, 2.0, "ct");
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
  LOG(INFO) << "Solving with " << solver->SolverVersion();
  const MPSolver::ResultStatus result_status = solver->Solve();
  // [END solve]

  // [START print_solution]
  // Check that the problem has an optimal solution.
  LOG(INFO) << "Status: " << result_status;
  if (result_status != MPSolver::OPTIMAL) {
    LOG(INFO) << "The problem does not have an optimal solution!";
    if (result_status == MPSolver::FEASIBLE) {
      LOG(INFO) << "A potentially suboptimal solution was found";
    } else {
      LOG(WARNING) << "The solver could not solve the problem.";
      return;
    }
  }

  LOG(INFO) << "Solution:";
  LOG(INFO) << "Objective value = " << objective->Value();
  LOG(INFO) << "x = " << x->solution_value();
  LOG(INFO) << "y = " << y->solution_value();
  // [END print_solution]

  // [START advanced]
  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Problem solved in " << solver->wall_time() << " milliseconds";
  LOG(INFO) << "Problem solved in " << solver->iterations() << " iterations";
  // [END advanced]
}
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetFlag(&FLAGS_stderrthreshold, 0);
  operations_research::BasicExample();
  return EXIT_SUCCESS;
}
// [END program]
