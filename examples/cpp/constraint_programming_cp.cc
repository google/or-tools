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

// Constraint programming example that shows how to use the API.

#include "ortools/base/logging.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {
void RunConstraintProgrammingExample() {
  // Instantiate the solver.
  Solver solver("ConstraintProgrammingExample");
  const int64_t numVals = 3;

  // Define decision variables.
  IntVar* const x = solver.MakeIntVar(0, numVals - 1, "x");
  IntVar* const y = solver.MakeIntVar(0, numVals - 1, "y");
  IntVar* const z = solver.MakeIntVar(0, numVals - 1, "z");

  // Define constraints.
  std::vector<IntVar*> xyvars = {x, y};
  solver.AddConstraint(solver.MakeAllDifferent(xyvars));

  LOG(INFO) << "Number of constraints: " << solver.constraints();

  // Create decision builder to search for solutions.
  std::vector<IntVar*> allvars = {x, y, z};
  DecisionBuilder* const db = solver.MakePhase(
      allvars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);

  solver.NewSearch(db);
  while (solver.NextSolution()) {
    LOG(INFO) << "Solution"
              << ": x = " << x->Value() << "; y = " << y->Value()
              << "; z = " << z->Value();
  }
  solver.EndSearch();
  LOG(INFO) << "Number of solutions: " << solver.solutions();
  LOG(INFO) << "";
  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Problem solved in " << solver.wall_time() << "ms";
  LOG(INFO) << "Memory usage: " << Solver::MemoryUsage() << " bytes";
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetFlag(&FLAGS_logtostderr, 1);
  operations_research::RunConstraintProgrammingExample();
  return EXIT_SUCCESS;
}
