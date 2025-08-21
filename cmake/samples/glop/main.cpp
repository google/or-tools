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

#include <ortools/glop/lp_solver.h>
#include <ortools/lp_data/lp_data.h>
#include <ortools/lp_data/lp_types.h>

#include <iostream>

namespace operations_research::glop {
int RunLinearExample() {
  LinearProgram linear_program;
  // Create the variables x and y.
  ColIndex col_x = linear_program.FindOrCreateVariable("x");
  linear_program.SetVariableBounds(col_x, 0.0, 1.0);
  ColIndex col_y = linear_program.FindOrCreateVariable("y");
  linear_program.SetVariableBounds(col_y, 0.0, 2.0);

  // Create linear constraint: 0 <= x + y <= 2.
  RowIndex row_r1 = linear_program.FindOrCreateConstraint("r1");
  linear_program.SetConstraintBounds(row_r1, 0.0, 2.0);
  linear_program.SetCoefficient(row_r1, col_x, 1);
  linear_program.SetCoefficient(row_r1, col_y, 1);

  // Create objective function: 3 * x + y.
  linear_program.SetObjectiveCoefficient(col_x, 3);
  linear_program.SetObjectiveCoefficient(col_y, 1);
  linear_program.SetMaximizationProblem(true);

  linear_program.CleanUp();

  std::cout << "Number of variables = " << linear_program.num_variables()
            << std::endl;
  std::cout << "Number of constraints = " << linear_program.num_constraints()
            << std::endl;

  LPSolver solver;
  GlopParameters parameters;
  parameters.set_provide_strong_optimal_guarantee(true);
  solver.SetParameters(parameters);

  ProblemStatus status = solver.Solve(linear_program);
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
    return 0;
  } else {
    return 1;
  }
}
}  // namespace operations_research::glop

int main(int /*argc*/, char** /*argv*/) {
  return operations_research::glop::RunLinearExample();
}
