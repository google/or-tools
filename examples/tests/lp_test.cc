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

// Linear programming example that shows how to use the API.

#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {
  void SolveAndPrint(
      MPSolver& solver,
      std::vector<MPVariable*> variables,
      std::vector<MPConstraint*> constraints) {
    LOG(INFO) << "Number of variables = " << solver.NumVariables();
    LOG(INFO) << "Number of constraints = " << solver.NumConstraints();

    const MPSolver::ResultStatus result_status = solver.Solve();
    // Check that the problem has an optimal solution.
    if (result_status != MPSolver::OPTIMAL) {
      LOG(FATAL) << "The problem does not have an optimal solution!";
    }
    LOG(INFO) << "Solution:";
    for(const auto& i : variables) {
      LOG(INFO) << i->name() << " = " << i->solution_value();
    }
    LOG(INFO) << "Optimal objective value = " << solver.Objective().Value();
    LOG(INFO) << "";
    LOG(INFO) << "Advanced usage:";
    LOG(INFO) << "Problem solved in " << solver.wall_time() << " milliseconds";
    LOG(INFO) << "Problem solved in " << solver.iterations() << " iterations";
    for(const auto& i : variables) {
      LOG(INFO) << i->name() << ": reduced cost " << i->reduced_cost();
    }

    const std::vector<double> activities = solver.ComputeConstraintActivities();
    for(const auto& i : constraints) {
      LOG(INFO) << i->name() << ": dual value = " << i->dual_value()
        << " activity = " << activities[i->index()];
    }
  }

  void RunLinearProgrammingExample(
      MPSolver::OptimizationProblemType optimization_problem_type) {
    MPSolver solver("LinearProgrammingExample", optimization_problem_type);
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
    MPConstraint* const c0 = solver.MakeRowConstraint(-infinity, 14.0, "c0");
    c0->SetCoefficient(x, 1);
    c0->SetCoefficient(y, 2);

    // 3x - y >= 0.
    MPConstraint* const c1 = solver.MakeRowConstraint(0.0, infinity, "c1");
    c1->SetCoefficient(x, 3);
    c1->SetCoefficient(y, -1);

    // x - y <= 2.
    MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 2.0, "c2");
    c2->SetCoefficient(x, 1);
    c2->SetCoefficient(y, -1);

    SolveAndPrint(solver, {x, y}, {c0, c1, c2});
  }

  void RunMixedIntegerProgrammingExample(
      MPSolver::OptimizationProblemType optimization_problem_type) {
    MPSolver solver("MixedIntegerProgrammingExample", optimization_problem_type);
    const double infinity = solver.infinity();
    // x and y are integers non-negative variables.
    MPVariable* const x = solver.MakeIntVar(0.0, infinity, "x");
    MPVariable* const y = solver.MakeIntVar(0.0, infinity, "y");

    // Objective function: Maximize x + 10 * y.
    MPObjective* const objective = solver.MutableObjective();
    objective->SetCoefficient(x, 1);
    objective->SetCoefficient(y, 10);
    objective->SetMaximization();

    // x + 7 * y <= 17.5
    MPConstraint* const c0 = solver.MakeRowConstraint(-infinity, 17.5, "c0");
    c0->SetCoefficient(x, 1);
    c0->SetCoefficient(y, 7);

    // x <= 3.5
    MPConstraint* const c1 = solver.MakeRowConstraint(-infinity, 3.5, "c1");
    c1->SetCoefficient(x, 1);
    c1->SetCoefficient(y, 0);

    SolveAndPrint(solver, {x, y}, {c0, c1});
  }

  void RunBooleanProgrammingExample(
      MPSolver::OptimizationProblemType optimization_problem_type) {
    MPSolver solver("MixedIntegerProgrammingExample", optimization_problem_type);
    const double infinity = solver.infinity();
    // x and y are boolean variables.
    MPVariable* const x = solver.MakeBoolVar("x");
    MPVariable* const y = solver.MakeBoolVar("y");

    // Objective function: Minimize 2 * x + y.
    MPObjective* const objective = solver.MutableObjective();
    objective->SetCoefficient(x, 2);
    objective->SetCoefficient(y, 1);
    objective->SetMinimization();

    // 1 <= x + 2 * y <= 3.
    MPConstraint* const c0 = solver.MakeRowConstraint(1, 3, "c0");
    c0->SetCoefficient(x, 1);
    c0->SetCoefficient(y, 2);

    SolveAndPrint(solver, {x, y}, {c0});
  }

  void RunAllExamples() {
    // Linear programming problems
#if defined(USE_CLP)
    LOG(INFO) << "---- Linear programming example with CLP ----";
    RunLinearProgrammingExample(MPSolver::CLP_LINEAR_PROGRAMMING);
#endif  // USE_CLP
#if defined(USE_GLPK)
    LOG(INFO) << "---- Linear programming example with GLPK ----";
    RunLinearProgrammingExample(MPSolver::GLPK_LINEAR_PROGRAMMING);
#endif  // USE_GLPK
#if defined(USE_GLOP)
    LOG(INFO) << "---- Linear programming example with GLOP ----";
    RunLinearProgrammingExample(MPSolver::GLOP_LINEAR_PROGRAMMING);
#endif  // USE_GLOP
#if defined(USE_GUROBI)
    LOG(INFO) << "---- Linear programming example with Gurobi ----";
    RunLinearProgrammingExample(MPSolver::GUROBI_LINEAR_PROGRAMMING);
#endif  // USE_GUROBI
#if defined(USE_CPLEX)
    LOG(INFO) << "---- Linear programming example with CPLEX ----";
    RunLinearProgrammingExample(MPSolver::CPLEX_LINEAR_PROGRAMMING);
#endif  // USE_CPLEX

    // Integer programming problems
#if defined(USE_SCIP)
    LOG(INFO) << "---- Mixed Integer programming example with SCIP ----";
    RunMixedIntegerProgrammingExample(MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
#endif  // USE_SCIP
#if defined(USE_GLPK)
    LOG(INFO) << "---- Mixed Integer programming example with GLPK ----";
    RunMixedIntegerProgrammingExample(MPSolver::GLPK_MIXED_INTEGER_PROGRAMMING);
#endif  // USE_GLPK
#if defined(USE_CBC)
    LOG(INFO) << "---- Mixed Integer programming example with CBC ----";
    RunMixedIntegerProgrammingExample(MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
#endif  // USE_CBC
#if defined(USE_GUROBI)
    LOG(INFO) << "---- Mixed Integer programming example with GUROBI ----";
    RunMixedIntegerProgrammingExample(MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING);
#endif  // USE_GUROBI
#if defined(USE_CPLEX)
    LOG(INFO) << "---- Mixed Integer programming example with CPLEX ----";
    RunMixedIntegerProgrammingExample(MPSolver::CPLEX_MIXED_INTEGER_PROGRAMMING);
#endif  // USE_CPLEX

    // Boolean integer programming problems
#if defined(USE_BOP)
    LOG(INFO) << "---- Boolean Integer programming example with BOP ----";
    RunBooleanProgrammingExample(MPSolver::BOP_INTEGER_PROGRAMMING);
#endif  // USE_BOP
  }
}  // namespace operations_research

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = 1;
  operations_research::RunAllExamples();
  return 0;
}
