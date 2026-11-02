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

#include <atomic>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/solve_mp_model.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research {
namespace {

// Adds a "market split" 0-1 program to the solver. These instances are known to
// be very hard for branch-and-bound (see Cornuejols and Dawande, "A class of
// hard small 0-1 programs"), so CBC will not solve one quickly and the solves
// below are guaranteed to be interrupted rather than to complete.
//
// Each equality row has, as right-hand side, the activity of a random solution
// that sets about half the variables to one, so the rows are balanced and the
// instance is guaranteed to have a feasible solution. If `known_solution` is
// not null, it is filled with that solution, which can be passed to CBC as an
// incumbent via SetHint().
void AddMarketSplit(MPSolver& solver, int num_vars, int num_constraints,
                    std::vector<double>* known_solution = nullptr) {
  absl::BitGen gen;
  std::vector<MPVariable*> vars;
  vars.reserve(num_vars);
  std::vector<double> solution;
  solution.reserve(num_vars);
  for (int j = 0; j < num_vars; ++j) {
    vars.push_back(solver.MakeBoolVar(absl::StrCat("x", j)));
    solution.push_back(absl::Bernoulli(gen, 0.5) ? 1.0 : 0.0);
  }
  for (int i = 0; i < num_constraints; ++i) {
    MPConstraint* const ct = solver.MakeRowConstraint(0.0, 0.0);
    double rhs = 0.0;
    for (int j = 0; j < num_vars; ++j) {
      const double coeff = static_cast<double>(absl::Uniform<int>(gen, 1, 101));
      ct->SetCoefficient(vars[j], coeff);
      rhs += coeff * solution[j];
    }
    ct->SetBounds(rhs, rhs);
  }
  // A non-trivial objective: with an all-zero objective, any feasible solution
  // would be optimal and CBC would stop before it could be interrupted.
  MPObjective* const objective = solver.MutableObjective();
  for (MPVariable* const var : vars) {
    objective->SetCoefficient(
        var, static_cast<double>(absl::Uniform<int>(gen, 1, 101)));
  }
  objective->SetMaximization();
  if (known_solution != nullptr) *known_solution = std::move(solution);
}

TEST(CbcInterfaceTest, InterruptSolveIsSupported) {
  EXPECT_TRUE(
      MPSolver::SupportsProblemType(MPSolver::CBC_MIXED_INTEGER_PROGRAMMING));
  MPSolver solver("cbc_interrupt", MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
  EXPECT_TRUE(solver.InterruptSolve());
}

// Interrupting before Solve() must not prevent the next solve from succeeding:
// Solve() resets the interrupt flag.
TEST(CbcInterfaceTest, InterruptBeforeSolveIsIgnored) {
  ASSERT_TRUE(
      MPSolver::SupportsProblemType(MPSolver::CBC_MIXED_INTEGER_PROGRAMMING));
  MPSolver solver("cbc_interrupt", MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
  MPVariable* const x = solver.MakeIntVar(0.0, 10.0, "x");
  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(x, 1.0);
  objective->SetMaximization();
  solver.MakeRowConstraint(-solver.infinity(), 5.0)->SetCoefficient(x, 1.0);
  EXPECT_TRUE(solver.InterruptSolve());
  EXPECT_EQ(solver.Solve(), MPSolver::OPTIMAL);
  EXPECT_DOUBLE_EQ(x->solution_value(), 5.0);
}

TEST(CbcInterfaceTest, InterruptSolveStopsBranchAndBound) {
  ASSERT_TRUE(
      MPSolver::SupportsProblemType(MPSolver::CBC_MIXED_INTEGER_PROGRAMMING));
  MPSolver solver("cbc_interrupt", MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
  AddMarketSplit(solver, /*num_vars=*/60, /*num_constraints=*/5);
  // Safety net: if the interruption does not work, do not hang the test suite.
  solver.SetTimeLimit(absl::Seconds(60));

  std::atomic<bool> solve_finished{false};
  MPSolver::ResultStatus status = MPSolver::NOT_SOLVED;
  const absl::Time start = absl::Now();
  std::thread solve_thread([&] {
    status = solver.Solve();
    solve_finished.store(true);
  });
  // Call InterruptSolve() repeatedly until the solve returns. An interruption
  // requested before Solve() resets its flag would otherwise be lost.
  while (!solve_finished.load()) {
    EXPECT_TRUE(solver.InterruptSolve());
    absl::SleepFor(absl::Milliseconds(10));
  }
  solve_thread.join();
  const absl::Duration elapsed = absl::Now() - start;
  // The instance is far too hard for CBC to solve quickly, so if the solve
  // returned quickly it can only be because it was interrupted. Interrupting a
  // solve that has no incumbent must give NOT_SOLVED (it must not be reported
  // as INFEASIBLE or ABNORMAL, which would mean the solve misbehaved).
  EXPECT_LT(elapsed, absl::Seconds(30));
  EXPECT_TRUE(status == MPSolver::NOT_SOLVED || status == MPSolver::FEASIBLE)
      << status;
}

// Interrupting a solve that already has an incumbent must report FEASIBLE with
// a usable solution (the `case 5` branch of CBCInterface::Solve()); reporting
// NOT_SOLVED would silently discard a solution the user can work with.
TEST(CbcInterfaceTest, InterruptAfterIncumbentReturnsFeasible) {
  ASSERT_TRUE(
      MPSolver::SupportsProblemType(MPSolver::CBC_MIXED_INTEGER_PROGRAMMING));
  MPSolver solver("cbc_interrupt", MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
  std::vector<double> known_solution;
  AddMarketSplit(solver, /*num_vars=*/60, /*num_constraints=*/15,
                 &known_solution);
  std::vector<std::pair<const MPVariable*, double>> hint;
  const std::vector<MPVariable*>& vars = solver.variables();
  hint.reserve(vars.size());
  for (int j = 0; j < vars.size(); ++j) {
    hint.push_back({vars[j], known_solution[j]});
  }
  solver.SetHint(hint);
  // Safety net: if the interruption does not work, do not hang the test suite.
  solver.SetTimeLimit(absl::Seconds(60));

  std::atomic<bool> solve_finished{false};
  MPSolver::ResultStatus status = MPSolver::NOT_SOLVED;
  const absl::Time start = absl::Now();
  std::thread solve_thread([&] {
    status = solver.Solve();
    solve_finished.store(true);
  });
  while (!solve_finished.load()) {
    EXPECT_TRUE(solver.InterruptSolve());
    absl::SleepFor(absl::Milliseconds(10));
  }
  solve_thread.join();
  const absl::Duration elapsed = absl::Now() - start;

  // The hint is an incumbent from the start, so the interruption must return
  // it as a feasible solution instead of discarding it.
  EXPECT_LT(elapsed, absl::Seconds(30));
  EXPECT_EQ(status, MPSolver::FEASIBLE);
  if (status == MPSolver::FEASIBLE) {
    EXPECT_TRUE(solver.VerifySolution(/*tolerance=*/1e-6, /*log_errors=*/true));
    for (const MPConstraint* const ct : solver.constraints()) {
      double activity = 0.0;
      for (const auto& [var, coeff] : ct->terms()) {
        activity += coeff * var->solution_value();
      }
      // Verify the returned solution explicitly too: VerifySolution() only
      // reports a boolean, this shows which constraint is violated.
      EXPECT_GE(activity, ct->lb() - 1e-6) << ct->index();
      EXPECT_LE(activity, ct->ub() + 1e-6) << ct->index();
    }
  }
}

// Interrupting a solve must not poison the solver: a later solve on the same
// solver object must run normally (i.e. the interrupt request is consumed by
// the solve it was meant for).
TEST(CbcInterfaceTest, SolveAfterInterruptSucceeds) {
  ASSERT_TRUE(
      MPSolver::SupportsProblemType(MPSolver::CBC_MIXED_INTEGER_PROGRAMMING));
  MPSolver solver("cbc_interrupt", MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
  AddMarketSplit(solver, /*num_vars=*/60, /*num_constraints=*/5);
  // Safety net: if the interruption does not work, do not hang the test suite.
  solver.SetTimeLimit(absl::Seconds(60));

  std::atomic<bool> solve_finished{false};
  std::thread solve_thread([&] {
    solver.Solve();
    solve_finished.store(true);
  });
  while (!solve_finished.load()) {
    EXPECT_TRUE(solver.InterruptSolve());
    absl::SleepFor(absl::Milliseconds(10));
  }
  solve_thread.join();

  // Replace the hard model by a trivial one and solve it.
  solver.Clear();
  MPVariable* const x = solver.MakeIntVar(0.0, 10.0, "x");
  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(x, 1.0);
  objective->SetMaximization();
  solver.MakeRowConstraint(-solver.infinity(), 5.0)->SetCoefficient(x, 1.0);
  EXPECT_EQ(solver.Solve(), MPSolver::OPTIMAL);
  EXPECT_DOUBLE_EQ(x->solution_value(), 5.0);
}

// Regression test: SolveMPModel() and SolveWithProto() reject an interrupter
// with MPSOLVER_INCOMPATIBLE_OPTIONS unless the solver type is registered as
// interruptible. Without that registration, the CBC interruption support would
// be silently unreachable from those APIs.
TEST(CbcInterfaceTest, SolveMpModelAcceptsInterrupter) {
  ASSERT_TRUE(
      MPSolver::SupportsProblemType(MPSolver::CBC_MIXED_INTEGER_PROGRAMMING));
  EXPECT_TRUE(SolverTypeSupportsInterruption(
      MPModelRequest::CBC_MIXED_INTEGER_PROGRAMMING));

  MPSolver solver("cbc_interrupt", MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
  MPVariable* const x = solver.MakeIntVar(0.0, 10.0, "x");
  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(x, 1.0);
  objective->SetMaximization();
  solver.MakeRowConstraint(-solver.infinity(), 5.0)->SetCoefficient(x, 1.0);

  MPModelRequest request;
  request.set_solver_type(MPModelRequest::CBC_MIXED_INTEGER_PROGRAMMING);
  solver.ExportModelToProto(request.mutable_model());
  // The interrupter is never triggered: the solve must succeed, i.e. the
  // request must not be rejected for lacking interruption support.
  SolveInterrupter interrupter;
  const MPSolutionResponse response = SolveMPModel(request, &interrupter);
  EXPECT_EQ(response.status(), MPSOLVER_OPTIMAL);
}

// End-to-end check that an interrupter passed to SolveMPModel() really stops a
// running CBC solve: the test above only proves that the request is not
// rejected with MPSOLVER_INCOMPATIBLE_OPTIONS, not that the interruption is
// observed by the CBC interface.
TEST(CbcInterfaceTest, SolveMpModelInterruptionStopsBranchAndBound) {
  ASSERT_TRUE(
      MPSolver::SupportsProblemType(MPSolver::CBC_MIXED_INTEGER_PROGRAMMING));
  MPSolver solver("cbc_interrupt", MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
  AddMarketSplit(solver, /*num_vars=*/60, /*num_constraints=*/5);

  MPModelRequest request;
  request.set_solver_type(MPModelRequest::CBC_MIXED_INTEGER_PROGRAMMING);
  // Safety net: if the interruption is not honored, do not hang the test suite.
  request.set_solver_time_limit_seconds(60);
  solver.ExportModelToProto(request.mutable_model());

  SolveInterrupter interrupter;
  MPSolutionResponse response;
  const absl::Time start = absl::Now();
  std::thread solve_thread(
      [&] { response = SolveMPModel(std::move(request), &interrupter); });
  absl::SleepFor(absl::Milliseconds(100));
  interrupter.Interrupt();
  solve_thread.join();
  const absl::Duration elapsed = absl::Now() - start;

  // The instance is far too hard for CBC to solve quickly, so if the solve
  // returned quickly it can only be because it was interrupted. The wrapper
  // turns the interruption into CANCELLED_BY_USER when CBC has no solution
  // (NOT_SOLVED), and keeps FEASIBLE if CBC managed to return an incumbent.
  EXPECT_LT(elapsed, absl::Seconds(30));
  EXPECT_TRUE(response.status() == MPSOLVER_CANCELLED_BY_USER ||
              response.status() == MPSOLVER_NOT_SOLVED ||
              response.status() == MPSOLVER_FEASIBLE)
      << response.status();
  // The wrapper notes the interruption, either when it observes it ("User
  // interrupted ...") or if it arrives before the solve could start.
  const std::string& status_str = response.status_str();
  EXPECT_TRUE(status_str.find("User interrupted") != std::string::npos ||
              status_str.find("Solve not started") != std::string::npos)
      << status_str;
}

}  // namespace
}  // namespace operations_research
