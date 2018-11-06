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

// [START program]
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include <iostream>

int main() {
  using namespace operations_research;
  using namespace std;

  MPSolver solver("SimpleProgram", MPSolver::GLOP_LINEAR_PROGRAMMING);
  // Create the variables x and y.
  MPVariable* const x = solver.MakeNumVar(0.0, 1, "x");
  MPVariable* const y = solver.MakeNumVar(0.0, 2, "y");
  // Create the objective function, x + y.
  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(x, 1);
  objective->SetCoefficient(y, 1);
  objective->SetMaximization();
  // Call the solver and display the results.
  solver.Solve();
  cout << "Solution:" << endl;
  cout << "x = " << x->solution_value() << endl;
  cout << "y = " << y->solution_value() << endl;

  return 0;
}
// [END program]
