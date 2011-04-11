// Copyright 2010-2011 Google
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

#include "linear_solver/linear_solver.h"

namespace operations_research {
void BuildLinearProgrammingMaxExample(MPSolver::OptimizationProblemType type) {
  MPSolver solver("Max_Example", type);
  const double infinity = solver.infinity();
  MPVariable* const x1 = solver.MakeNumVar(0.0, infinity, "x1");
  MPVariable* const x2 = solver.MakeNumVar(0.0, infinity, "x2");
  MPVariable* const x3 = solver.MakeNumVar(0.0, infinity, "x3");

  solver.AddObjectiveTerm(x1, 10);
  solver.AddObjectiveTerm(x2, 6);
  solver.AddObjectiveTerm(x3, 4);
  solver.SetMaximization();

  MPConstraint* const c0 = solver.MakeRowConstraint(-infinity, 100.0);
  c0->AddTerm(x1, 1);
  c0->AddTerm(x2, 1);
  c0->AddTerm(x3, 1);
  MPConstraint* const c1 = solver.MakeRowConstraint(-infinity, 600.0);
  c1->AddTerm(x1, 10);
  c1->AddTerm(x2, 4);
  c1->AddTerm(x3, 5);
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 300.0);
  c2->AddTerm(x1, 2);
  c2->AddTerm(x2, 2);
  c2->AddTerm(x3, 6);

  // The problem has an optimal solution.
  CHECK_EQ(MPSolver::OPTIMAL, solver.Solve());

  LOG(INFO) << "objective = " <<  solver.objective_value();
  LOG(INFO) << "x1 = " << x1->solution_value()
            << ", reduced cost = " << x1->reduced_cost();
  LOG(INFO) << "x2 = " << x2->solution_value()
            << ", reduced cost = " << x2->reduced_cost();
  LOG(INFO) << "x3 = " << x3->solution_value()
            << ", reduced cost = " << x3->reduced_cost();
  LOG(INFO) << "c0 dual value = " << c0->dual_value();
  LOG(INFO) << "c1 dual value = " << c1->dual_value();
  LOG(INFO) << "c2 dual value = " << c2->dual_value();
}

void RunAllExamples() {
  LOG(INFO) << "----- Running Max Example with GLPK -----";
  BuildLinearProgrammingMaxExample(MPSolver::GLPK_LINEAR_PROGRAMMING);
  LOG(INFO) << "----- Running Max Example with Coin LP -----";
  BuildLinearProgrammingMaxExample(MPSolver::CLP_LINEAR_PROGRAMMING);
}
}  // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::RunAllExamples();
  return 0;
}
