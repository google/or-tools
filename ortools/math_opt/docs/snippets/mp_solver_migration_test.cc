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

#include <cmath>
#include <iostream>
#include <memory>
#include <ostream>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_expr.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/solve_mp_model.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace {

constexpr double kTolerance = 1e-5;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::status::IsOkAndHolds;

// [START_solve_with_mpsolver]
absl::StatusOr<double> SolveWithMPSolver() {
  using namespace operations_research;  // NOLINT
  // Build the model.
  MPSolver solver("mip", MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  const MPVariable* const x_var = solver.MakeBoolVar("x");
  const MPVariable* const y_var = solver.MakeNumVar(0.0, 3.0, "y");
  const LinearExpr x(x_var);
  const LinearExpr y(y_var);
  solver.MutableObjective()->MaximizeLinearExpr(2 * x + y);
  solver.MakeRowConstraint(x + y <= 1.3);

  // Solve the model.
  solver.SetTimeLimit(absl::Seconds(10));
  solver.EnableOutput();
  MPSolverParameters parameters;
  parameters.SetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP, 0.05);
  const MPSolver::ResultStatus result_status = solver.Solve(parameters);
  if (result_status != MPSolver::OPTIMAL &&
      result_status != MPSolver::FEASIBLE) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Could not find solution, result status: " << result_status;
  }

  // Get the solution.
  std::cout << "objective: " << solver.Objective().Value() << "\n"
            << "x: " << x_var->solution_value() << "\n"
            << "y: " << y_var->solution_value() << std::endl;
  return solver.Objective().Value();
}
// [END_solve_with_mpsolver]

TEST(MPSolverMigrationTest, SolveWithMPSolver) {
  ASSERT_OK_AND_ASSIGN(const double obj, SolveWithMPSolver());
  EXPECT_GE(obj, 0.0 - kTolerance);
  EXPECT_LE(obj, 2.3 + kTolerance);
}

// [START_solve_with_mp_model_proto]
absl::StatusOr<double> SolveWithMPModelProto() {
  using namespace operations_research;  // NOLINT
  MPModelRequest request;

  // Build the model.
  MPModelProto& model = *request.mutable_model();
  model.set_maximize(true);
  const int x_index = model.variable_size();
  MPVariableProto* const x = model.add_variable();
  x->set_lower_bound(0.0);
  x->set_upper_bound(1.0);
  x->set_is_integer(true);
  x->set_name("x");
  x->set_objective_coefficient(2.0);
  const int y_index = model.variable_size();
  MPVariableProto* const y = model.add_variable();
  y->set_lower_bound(0.0);
  y->set_upper_bound(3.0);
  y->set_is_integer(false);
  y->set_name("y");
  y->set_objective_coefficient(1.0);
  MPConstraintProto* const c = model.add_constraint();
  c->set_upper_bound(1.3);
  c->add_var_index(x_index);
  c->add_coefficient(1.0);
  c->add_var_index(y_index);
  c->add_coefficient(1.0);

  // Solve the model.
  request.set_solver_time_limit_seconds(10.0);
  request.set_enable_internal_solver_output(true);
  request.set_solver_type(MPModelRequest::SCIP_MIXED_INTEGER_PROGRAMMING);
  request.set_solver_specific_parameters("limits/gap=0.05");
  const MPSolutionResponse result = SolveMPModel(request);
  if (result.status() != MPSOLVER_OPTIMAL &&
      result.status() != MPSOLVER_FEASIBLE) {
    return ortools::InvalidArgumentErrorBuilder()
           << "Could not find solution, result status: "
           << MPSolverResponseStatus_Name(result.status());
  }

  // Get the solution.
  std::cout << "objective: " << result.objective_value() << "\n"
            << "x: " << result.variable_value(x_index) << "\n"
            << "y: " << result.variable_value(y_index) << std::endl;
  return result.objective_value();
}
// [END_solve_with_mp_model_proto]

TEST(MPSolverMigrationTest, SolveWithMPModelProto) {
  ASSERT_OK_AND_ASSIGN(const double obj, SolveWithMPModelProto());
  EXPECT_GE(obj, 0.0 - kTolerance);
  EXPECT_LE(obj, 2.3 + kTolerance);
}

// [START_solve_with_math_opt]
absl::StatusOr<double> SolveWithMathOpt() {
  using namespace operations_research::math_opt;  // NOLINT
  // Build the model.
  Model model("mip");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddContinuousVariable(0.0, 3.0, "y");
  model.Maximize(2 * x + y);
  model.AddLinearConstraint(x + y <= 1.3);

  // Solve the model.
  const SolveParameters params = {.enable_output = true,
                                  .time_limit = absl::Seconds(10),
                                  .relative_gap_tolerance = 0.05};
  OR_ASSIGN_OR_RETURN(const SolveResult result,
                      Solve(model, SolverType::kGscip, {.parameters = params}));
  OR_RETURN_IF_ERROR(result.termination.EnsureIsOptimalOrFeasible());

  // Get the solution.
  std::cout << "objective: " << result.objective_value() << "\n"
            << "x: " << result.variable_values().at(x) << "\n"
            << "y: " << result.variable_values().at(y) << std::endl;
  return result.objective_value();
}
// [END_solve_with_math_opt]

TEST(MPSolverMigrationTest, SolveWithMathOpt) {
  ASSERT_OK_AND_ASSIGN(const double obj, SolveWithMathOpt());
  EXPECT_GE(obj, 0.0 - kTolerance);
  EXPECT_LE(obj, 2.3 + kTolerance);
}

// [START_solve_incrementally_with_mpsolver]
absl::StatusOr<std::vector<double>> SolveIncrementallyWithMPSolver() {
  using namespace operations_research;  // NOLINT
  // Build the model.
  MPSolver solver("incremental_lp", MPSolver::GLOP_LINEAR_PROGRAMMING);
  MPVariable* const x_var = solver.MakeNumVar(0.0, 1.0, "x");
  solver.MutableObjective()->MaximizeLinearExpr(LinearExpr(x_var));

  // Solve the model.
  solver.EnableOutput();
  MPSolverParameters parameters;
  // GLOP will only hot-start incremental solves when presolve is disabled.
  parameters.SetIntegerParam(MPSolverParameters::PRESOLVE,
                             MPSolverParameters::PRESOLVE_OFF);

  std::vector<double> objective_values;
  for (int i = 0; i < 5; ++i) {
    x_var->SetUB(2 * (i + 1));
    const MPSolver::ResultStatus result_status = solver.Solve(parameters);
    if (result_status != MPSolver::OPTIMAL) {
      return ortools::InvalidArgumentErrorBuilder()
             << "Expected OPTIMAL, but result status was: " << result_status;
    }
    objective_values.push_back(solver.Objective().Value());
  }
  return objective_values;
}
// [END_solve_incrementally_with_mpsolver]

TEST(MPSolverMigrationTest, SolveIncrementallyWithMPSolver) {
  EXPECT_THAT(
      SolveIncrementallyWithMPSolver(),
      IsOkAndHolds(ElementsAre(DoubleNear(2.0, 1e-5), DoubleNear(4.0, 1e-5),
                               DoubleNear(6.0, 1e-5), DoubleNear(8.0, 1e-5),
                               DoubleNear(10.0, 1e-5))));
}
// [START_solve_incrementally_with_math_opt]
absl::StatusOr<std::vector<double>> SolveIncrementallyWithMathOpt() {
  using namespace operations_research::math_opt;  // NOLINT
  // Build the model.
  Model model("incremental_lp");
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  model.Maximize(x);

  // Solve the model.
  // GLOP will only hot-start incremental solves when presolve is disabled.
  const SolveParameters params = {.enable_output = true,
                                  .presolve = Emphasis::kOff};
  OR_ASSIGN_OR_RETURN(const std::unique_ptr<IncrementalSolver> solver,
                      NewIncrementalSolver(&model, SolverType::kGlop));
  std::vector<double> objective_values;
  for (int i = 0; i < 5; ++i) {
    model.set_upper_bound(x, 2 * (i + 1));
    OR_ASSIGN_OR_RETURN(const SolveResult result,
                        solver->Solve({.parameters = params}));
    OR_RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
    objective_values.push_back(result.objective_value());
  }
  return objective_values;
}
// [END_solve_incrementally_with_math_opt]

// [START_solve_incrementally_with_math_opt_test]
TEST(MPSolverMigrationTest, SolveIncrementallyWithMathOpt) {
  EXPECT_THAT(
      SolveIncrementallyWithMathOpt(),
      IsOkAndHolds(ElementsAre(DoubleNear(2.0, 1e-5), DoubleNear(4.0, 1e-5),
                               DoubleNear(6.0, 1e-5), DoubleNear(8.0, 1e-5),
                               DoubleNear(10.0, 1e-5))));
}
// [END_solve_incrementally_with_math_opt_test]

// [START_mpsolver_integer_solution_value]
TEST(MPSolverMigrationTest, MPSolverIntegerSolutionValue) {
  using namespace operations_research;  // NOLINT
  // Build the model.
  MPSolver solver("mip", MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  const MPVariable* const x_var = solver.MakeBoolVar("x");
  solver.MutableObjective()->MaximizeLinearExpr(LinearExpr(x_var));
  const MPSolver::ResultStatus result_status = solver.Solve();
  ASSERT_EQ(result_status, MPSolver::OPTIMAL);
  EXPECT_EQ(x_var->solution_value(), 1.0);
}
// [END_mpsolver_integer_solution_value]

// [START_math_opt_integer_solution_value]
TEST(MPSolverMigrationTest, MathOptIntegerSolutionValue) {
  using namespace operations_research::math_opt;  // NOLINT
  // Build the model.
  Model model("mip");
  const Variable x = model.AddBinaryVariable("x");
  model.Maximize(x);
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, SolverType::kGscip));
  ASSERT_EQ(result.termination.reason, TerminationReason::kOptimal);
  EXPECT_EQ(std::round(result.variable_values().at(x)), 1.0);
}
// [END_math_opt_integer_solution_value]

}  // namespace
