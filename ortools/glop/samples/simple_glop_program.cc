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
#include <iostream>

#include "ortools/glop/lp_solver.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
// [END import]

namespace operations_research::glop {
int RunLinearExample() {
  LinearProgram lp;
  // Create the variables x and y.
  ColIndex col_x = lp.FindOrCreateVariable("x");
  lp.SetVariableBounds(col_x, 0.0, 1.0);
  ColIndex col_y = lp.FindOrCreateVariable("y");
  lp.SetVariableBounds(col_y, 0.0, 2.0);

  // Create linear constraint: 0 <= x + y <= 2.
  RowIndex row_r1 = lp.FindOrCreateConstraint("r1");
  lp.SetConstraintBounds(row_r1, 0.0, 2.0);
  lp.SetCoefficient(row_r1, col_x, 1);
  lp.SetCoefficient(row_r1, col_y, 1);

  // Create objective function: 3 * x + y.
  lp.SetObjectiveCoefficient(col_x, 3);
  lp.SetObjectiveCoefficient(col_y, 1);
  lp.SetMaximizationProblem(true);

  lp.CleanUp();

  std::cout << "Number of variables = " << lp.num_variables() << std::endl;
  std::cout << "Number of constraints = " << lp.num_constraints() << std::endl;

  LPSolver solver;
  GlopParameters parameters;
  parameters.set_provide_strong_optimal_guarantee(true);
  solver.SetParameters(parameters);

  ProblemStatus status = solver.Solve(lp);
  if (status == ProblemStatus::OPTIMAL) {
    std::cout << "Optimal solution found !" << std::endl;
    // The objective value of the solution.
    std::cout << "Optimal objective value = " << solver.GetObjectiveValue()
              << std::endl;
    // The value of each variable in the solution.
    const DenseRow& values = solver.variable_values();
    std::cout << "Solution:" << std::endl
              << "x = " << values[col_x] << std::endl
              << ", y = " << values[col_y] << std::endl;
    return EXIT_SUCCESS;
  } else {
    return EXIT_FAILURE;
  }
}
}  // namespace operations_research::glop

int main(int argc, char** argv) {
  return operations_research::glop::RunLinearExample();
}
// [END program]
