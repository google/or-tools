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

//
// Linear programming example that shows how to use the API.

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {
void RunLinearProgrammingExample(
    MPSolver::OptimizationProblemType optimization_problem_type) {
  MPSolver solver("LinearProgrammingExample", optimization_problem_type);
  const double infinity = solver.infinity();
  // x1, x2 and x3 are continuous non-negative variables.
  MPVariable* const x1 = solver.MakeNumVar(0.0, infinity, "x1");
  MPVariable* const x2 = solver.MakeNumVar(0.0, infinity, "x2");
  MPVariable* const x3 = solver.MakeNumVar(0.0, infinity, "x3");

  // Maximize 10 * x1 + 6 * x2 + 4 * x3.
  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(x1, 10);
  objective->SetCoefficient(x2, 6);
  objective->SetCoefficient(x3, 4);
  objective->SetMaximization();

  // x1 + x2 + x3 <= 100.
  MPConstraint* const c0 = solver.MakeRowConstraint(-infinity, 100.0);
  c0->SetCoefficient(x1, 1);
  c0->SetCoefficient(x2, 1);
  c0->SetCoefficient(x3, 1);

  // 10 * x1 + 4 * x2 + 5 * x3 <= 600.
  MPConstraint* const c1 = solver.MakeRowConstraint(-infinity, 600.0);
  c1->SetCoefficient(x1, 10);
  c1->SetCoefficient(x2, 4);
  c1->SetCoefficient(x3, 5);

  // 2 * x1 + 2 * x2 + 6 * x3 <= 300.
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 300.0);
  c2->SetCoefficient(x1, 2);
  c2->SetCoefficient(x2, 2);
  c2->SetCoefficient(x3, 6);

  // TODO(user): Change example to show = and >= constraints.

  LOG(INFO) << "Number of variables = " << solver.NumVariables();
  LOG(INFO) << "Number of constraints = " << solver.NumConstraints();

  const MPSolver::ResultStatus result_status = solver.Solve();

  // Check that the problem has an optimal solution.
  if (result_status != MPSolver::OPTIMAL) {
    LOG(FATAL) << "The problem does not have an optimal solution!";
  }

  LOG(INFO) << "Problem solved in " << solver.wall_time() << " milliseconds";

  // The objective value of the solution.
  LOG(INFO) << "Optimal objective value = " << objective->Value();

  // The value of each variable in the solution.
  LOG(INFO) << "x1 = " << x1->solution_value();
  LOG(INFO) << "x2 = " << x2->solution_value();
  LOG(INFO) << "x3 = " << x3->solution_value();

  LOG(INFO) << "Advanced usage:";
  LOG(INFO) << "Problem solved in " << solver.iterations() << " iterations";
  LOG(INFO) << "x1: reduced cost = " << x1->reduced_cost();
  LOG(INFO) << "x2: reduced cost = " << x2->reduced_cost();
  LOG(INFO) << "x3: reduced cost = " << x3->reduced_cost();
  const std::vector<double> activities = solver.ComputeConstraintActivities();
  LOG(INFO) << "c0: dual value = " << c0->dual_value()
            << " activity = " << activities[c0->index()];
  LOG(INFO) << "c1: dual value = " << c1->dual_value()
            << " activity = " << activities[c1->index()];
  LOG(INFO) << "c2: dual value = " << c2->dual_value()
            << " activity = " << activities[c2->index()];
}

void RunAllExamples() {
  #if defined(USE_GLOP)
  LOG(INFO) << "---- Linear programming example with GLOP ----";
  RunLinearProgrammingExample(MPSolver::GLOP_LINEAR_PROGRAMMING);
  #endif  // USE_GLOP
  #if defined(USE_GLPK)
  LOG(INFO) << "---- Linear programming example with GLPK ----";
  RunLinearProgrammingExample(MPSolver::GLPK_LINEAR_PROGRAMMING);
  #endif  // USE_GLPK
  #if defined(USE_CLP)
  LOG(INFO) << "---- Linear programming example with CLP ----";
  RunLinearProgrammingExample(MPSolver::CLP_LINEAR_PROGRAMMING);
  #endif  // USE_CLP
  #if defined(USE_SLM)
  LOG(INFO) << "---- Linear programming example with Sulum ----";
  RunLinearProgrammingExample(MPSolver::SULUM_LINEAR_PROGRAMMING);
  #endif  // USE_SLM
  #if defined(USE_GUROBI)
  LOG(INFO) << "---- Linear programming example with Gurobi ----";
  RunLinearProgrammingExample(MPSolver::GUROBI_LINEAR_PROGRAMMING);
  #endif  // USE_GUROBI
  #if defined(USE_CPLEX)
  LOG(INFO) << "---- Linear programming example with CPLEX ----";
  RunLinearProgrammingExample(MPSolver::CPLEX_LINEAR_PROGRAMMING);
  #endif  // USE_CPLEX
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  operations_research::RunAllExamples();
  return 0;
}
