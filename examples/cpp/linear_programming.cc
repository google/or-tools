// Copyright 2010-2017 Google
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

// Linear programming example that shows how to use the API.

#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {
void RunLinearProgrammingExample() {
  MPSolver solver("LinearProgrammingExample",
                  MPSolver::GLOP_LINEAR_PROGRAMMING);
  const double infinity = solver.infinity();
  // x and y are continuous non-negative variables.
  MPVariable* const x = solver.MakeNumVar(0.0, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(0.0, infinity, "y");

  // Objectif function: Maximize 3x + 4y.
  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(x, 3);
  objective->SetCoefficient(y, 4);
  objective->SetMaximization();

  // x + 2y <= 14.
  MPConstraint* const c0 = solver.MakeRowConstraint(-infinity, 14.0);
  c0->SetCoefficient(x, 1);
  c0->SetCoefficient(y, 2);

  // 3x - y >= 0.
  MPConstraint* const c1 = solver.MakeRowConstraint(0.0, infinity);
  c1->SetCoefficient(x, 3);
  c1->SetCoefficient(y, -1);

  // x - y <= 2.
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 2.0);
  c2->SetCoefficient(x, 1);
  c2->SetCoefficient(y, -1);

  LOG(INFO) << "Number of variables = " << solver.NumVariables();
  LOG(INFO) << "Number of constraints = " << solver.NumConstraints();

  const MPSolver::ResultStatus result_status = solver.Solve();
  // Check that the problem has an optimal solution.
  if (result_status != MPSolver::OPTIMAL) {
    LOG(FATAL) << "The problem does not have an optimal solution!";
  }
  LOG(INFO) << "Solution:";
  LOG(INFO) << "x = " << x->solution_value();
  LOG(INFO) << "y = " << y->solution_value();
  LOG(INFO) << "Optimal objective value = " << objective->Value();
  LOG(INFO) << "";
  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Problem solved in " << solver.wall_time() << " milliseconds";
  LOG(INFO) << "Problem solved in " << solver.iterations() << " iterations";
  LOG(INFO) << "x: reduced cost = " << x->reduced_cost();
  LOG(INFO) << "y: reduced cost = " << y->reduced_cost();
  const std::vector<double> activities = solver.ComputeConstraintActivities();
  LOG(INFO) << "c0: dual value = " << c0->dual_value()
            << " activity = " << activities[c0->index()];
  LOG(INFO) << "c1: dual value = " << c1->dual_value()
            << " activity = " << activities[c1->index()];
  LOG(INFO) << "c2: dual value = " << c2->dual_value()
            << " activity = " << activities[c2->index()];
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;
  operations_research::RunLinearProgrammingExample();
  return EXIT_SUCCESS;
}
