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
#include "ortools/linear_solver/linear_solver.h"
// [END import]

// [START program_part1]
namespace operations_research {
// [START data_model]
struct DataModel {
  const std::vector<std::vector<double>> constraint_coeffs{
      {5, 7, 9, 2, 1},
      {18, 4, -9, 10, 12},
      {4, 7, 3, 8, 5},
      {5, 13, 16, 3, -7},
  };
  const std::vector<double> bounds{250, 285, 211, 315};
  const std::vector<double> obj_coeffs{7, 8, 2, 9, 6};
  const int num_vars = 5;
  const int num_constraints = 4;
};
// [END data_model]

void MipVarArray() {
  // [START data]
  DataModel data;
  // [END data]
  // [END program_part1]

  // [START solver]
  // Create the mip solver with the SCIP backend.
  std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver("SCIP"));
  if (!solver) {
    LOG(WARNING) << "SCIP solver unavailable.";
    return;
  }
  // [END solver]

  // [START program_part2]
  // [START variables]
  const double infinity = solver->infinity();
  // x[j] is an array of non-negative, integer variables.
  std::vector<const MPVariable*> x(data.num_vars);
  for (int j = 0; j < data.num_vars; ++j) {
    x[j] = solver->MakeIntVar(0.0, infinity, "");
  }
  LOG(INFO) << "Number of variables = " << solver->NumVariables();
  // [END variables]

  // [START constraints]
  // Create the constraints.
  for (int i = 0; i < data.num_constraints; ++i) {
    MPConstraint* constraint = solver->MakeRowConstraint(0, data.bounds[i], "");
    for (int j = 0; j < data.num_vars; ++j) {
      constraint->SetCoefficient(x[j], data.constraint_coeffs[i][j]);
    }
  }
  LOG(INFO) << "Number of constraints = " << solver->NumConstraints();
  // [END constraints]

  // [START objective]
  // Create the objective function.
  MPObjective* const objective = solver->MutableObjective();
  for (int j = 0; j < data.num_vars; ++j) {
    objective->SetCoefficient(x[j], data.obj_coeffs[j]);
  }
  objective->SetMaximization();
  // [END objective]

  // [START solve]
  const MPSolver::ResultStatus result_status = solver->Solve();
  // [END solve]

  // [START print_solution]
  // Check that the problem has an optimal solution.
  if (result_status != MPSolver::OPTIMAL) {
    LOG(FATAL) << "The problem does not have an optimal solution.";
  }
  LOG(INFO) << "Solution:";
  LOG(INFO) << "Optimal objective value = " << objective->Value();

  for (int j = 0; j < data.num_vars; ++j) {
    LOG(INFO) << "x[" << j << "] = " << x[j]->solution_value();
  }
  // [END print_solution]
}
}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::MipVarArray();
  return EXIT_SUCCESS;
}
// [END program_part2]
// [END program]
