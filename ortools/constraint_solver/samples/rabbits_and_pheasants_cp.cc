// Copyright 2018 Google LLC
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

// Knowing that we see 20 heads and 56 legs,
// how many pheasants and rabbits are we looking at ?

#include "ortools/base/logging.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {
void RunConstraintProgrammingExample() {
  // Instantiate the solver.
  Solver solver("RabbitsPheasantsExample");

  // Define decision variables.
  IntVar* const rabbits = solver.MakeIntVar(0, 20, "rabbits");
  IntVar* const pheasants = solver.MakeIntVar(0, 20, "pheasants");

  // Define constraints.
  IntExpr* const heads = solver.MakeSum(rabbits, pheasants);
  Constraint* const c0 = solver.MakeEquality(heads, 20);
  solver.AddConstraint(c0);

  IntExpr* const legs = solver.MakeSum(solver.MakeProd(rabbits, 4),
                                       solver.MakeProd(pheasants, 2));
  Constraint* const c1 = solver.MakeEquality(legs, 56);
  solver.AddConstraint(c1);

  DecisionBuilder* const db =
      solver.MakePhase(rabbits, pheasants, Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);

  int count = 0;
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    count++;
    LOG(INFO) << "Solution " << count << ":";
    LOG(INFO) << "rabbits = " << rabbits->Value();
    LOG(INFO) << "pheasants = " << rabbits->Value();
  }
  solver.EndSearch();
  LOG(INFO) << "Number of solutions: " << count;
  LOG(INFO) << "";
  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Problem solved in " << solver.wall_time() << " milliseconds";
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  absl::SetFlag(&FLAGS_logtostderr, 1);
  operations_research::RunConstraintProgrammingExample();
  return EXIT_SUCCESS;
}
