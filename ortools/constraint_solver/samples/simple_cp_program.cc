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
#include "ortools/constraint_solver/constraint_solver.h"
// [END import]

namespace operations_research {

void SimpleCpProgram() {
  // Instantiate the solver.
  // [START solver]
  Solver solver("CpSimple");
  // [END solver]

  // Create the variables.
  // [START variables]
  const int64_t num_vals = 3;
  IntVar* const x = solver.MakeIntVar(0, num_vals - 1, "x");
  IntVar* const y = solver.MakeIntVar(0, num_vals - 1, "y");
  IntVar* const z = solver.MakeIntVar(0, num_vals - 1, "z");
  // [END variables]

  // Constraint 0: x != y..
  // [START constraints]
  solver.AddConstraint(solver.MakeAllDifferent({x, y}));
  LOG(INFO) << "Number of constraints: "
            << std::to_string(solver.constraints());
  // [END constraints]

  // Solve the problem.
  // [START solve]
  DecisionBuilder* const db = solver.MakePhase(
      {x, y, z}, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  // [END solve]

  // Print solution on console.
  // [START print_solution]
  int count = 0;
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    ++count;
    LOG(INFO) << "Solution " << count << ":" << std::endl
              << " x=" << x->Value() << " y=" << y->Value()
              << " z=" << z->Value();
  }
  solver.EndSearch();
  LOG(INFO) << "Number of solutions found: " << solver.solutions();
  // [END print_solution]

  // [START advanced]
  LOG(INFO) << "Advanced usage:" << std::endl
            << "Problem solved in " << std::to_string(solver.wall_time())
            << "ms" << std::endl
            << "Memory usage: " << std::to_string(Solver::MemoryUsage())
            << "bytes";
  // [END advanced]
}

}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::SimpleCpProgram();
  return EXIT_SUCCESS;
}
// [END program]
