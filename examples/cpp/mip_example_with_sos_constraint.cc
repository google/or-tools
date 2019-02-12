// Copyright 2010-2018 Google LLC
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

// Integer programming example that shows how to use the API.

#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {
void RunMixedIntegerProgrammingExample(
    MPSolver::OptimizationProblemType optimization_problem_type) {
  MPSolver solver("MixedIntegerProgrammingExample", optimization_problem_type);
  const double infinity = solver.infinity();
  // x and y are integer non-negative variables.
  MPVariable* const x = solver.MakeIntVar(0.0, infinity, "x");
  MPVariable* const y = solver.MakeIntVar(0.0, infinity, "y");

  // Maximize x + 10 * y.
  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(x, 1);
  objective->SetCoefficient(y, 10);
  objective->SetMaximization();

  // x + 7 * y <= 17.5.
  MPConstraint* const c0 = solver.MakeRowConstraint(-infinity, 17.5);
  c0->SetCoefficient(x, 1);
  c0->SetCoefficient(y, 7);

  // x <= 3.5
  MPConstraint* const c1 = solver.MakeRowConstraint(-infinity, 3.5);
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 0);

  SOSConstraint* const c2 = solver.MakeSOSConstraint(MPSolver::SOS1);
  c2->SetCoefficient(x, 1);
  c2->SetCoefficient(y, 1);

  LOG(INFO) << "Number of variables = " << solver.NumVariables();
  LOG(INFO) << "Number of constraints = " << solver.NumConstraints();
  LOG(INFO) << "Number of SOS constraints = " << solver.NumSOSConstraints();

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
  LOG(INFO) << "Problem solved in " << solver.nodes()
            << " branch-and-bound nodes";
}

void RunAllExamples() {
#if defined(USE_CBC)
  LOG(INFO) << "---- Mixed integer programming example with CBC ----";
  RunMixedIntegerProgrammingExample(MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
#endif
#if defined(USE_GLPK)
  LOG(INFO) << "---- Mixed integer programming example with GLPK ----";
  RunMixedIntegerProgrammingExample(MPSolver::GLPK_MIXED_INTEGER_PROGRAMMING);
#endif
#if defined(USE_SCIP)
  LOG(INFO) << "---- Mixed integer programming example with SCIP ----";
  RunMixedIntegerProgrammingExample(MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
#endif
#if defined(USE_GUROBI)
  LOG(INFO) << "---- Mixed integer programming example with Gurobi ----";
  RunMixedIntegerProgrammingExample(MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING);
#endif  // USE_GUROBI
#if defined(USE_CPLEX)
  LOG(INFO) << "---- Mixed integer programming example with CPLEX ----";
  RunMixedIntegerProgrammingExample(MPSolver::CPLEX_MIXED_INTEGER_PROGRAMMING);
#endif  // USE_CPLEX
}
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;
  //operations_research::RunAllExamples();
  RunMixedIntegerProgrammingExample(operations_research::MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING);
  return EXIT_SUCCESS;
}
