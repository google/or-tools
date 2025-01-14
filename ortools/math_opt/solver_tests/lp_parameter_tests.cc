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

// TODO(b/180024054): the following parameters are not tested:
//  * time_limit
//  * threads
//  * scaling
//
// The following parameters are under-tested:
//  * lp_algorithm
//
// Note that cuts and heuristics do not apply for LP. enable_output is tested
// in generic_tests.h.
#include "ortools/math_opt/solver_tests/lp_parameter_tests.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace math_opt {

using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

std::ostream& operator<<(std::ostream& out,
                         const LpParameterTestParams& params) {
  out << "{ solver_type: " << params.solver_type
      << " supports_simplex: " << params.supports_simplex
      << " supports_barrier: " << params.supports_barrier
      << " supports_random_seed: " << params.supports_random_seed
      << " supports_presolve: " << params.supports_presolve
      << " supports_cutoff: " << params.supports_cutoff << " }";
  return out;
}

namespace {

class AssignmentProblem {
 public:
  explicit AssignmentProblem(const int n) : n_(n) {
    for (int i = 0; i < n_; ++i) {
      vars_.emplace_back();
      for (int j = 0; j < n_; ++j) {
        vars_.back().push_back(
            model_.AddVariable(0.0, 1.0, false, absl::StrCat("x_", i, "_", j)));
      }
    }
    LinearExpression obj;
    for (int i = 0; i < n_; ++i) {
      obj += Sum(vars_[i]);
    }
    model_.Maximize(obj);
    for (int i = 0; i < n_; ++i) {
      model_.AddLinearConstraint(Sum(vars_[i]) <= 1.0);
    }
    for (int j = 0; j < n_; ++j) {
      LinearExpression j_vars;
      for (int i = 0; i < n_; ++i) {
        j_vars += vars_[i][j];
      }
      model_.AddLinearConstraint(j_vars <= 1.0);
    }
  }

  void MakePresolveOptimal() {
    for (int i = 0; i < n_; ++i) {
      LinearExpression terms;
      for (int j = 0; j < n_; ++j) {
        if (i != j) {
          terms += vars_[i][j];
        }
      }
      model_.AddLinearConstraint(terms == 0.0);
    }
  }

  const Model& model() { return model_; }

  std::vector<std::string> SolutionFingerprint(
      const SolveResult& result) const {
    std::vector<std::string> fingerprint;
    for (int i = 0; i < n_; ++i) {
      for (int j = 0; j < n_; ++j) {
        const double val = result.variable_values().at(vars_[i][j]);
        CHECK(val <= 0.01 || val >= 0.99)
            << "i: " << i << " j: " << j << " val: " << val;
        if (val > 0.5) {
          fingerprint.push_back(std::string(vars_[i][j].name()));
        }
      }
    }
    std::sort(fingerprint.begin(), fingerprint.end());
    return fingerprint;
  }

 private:
  const int n_;
  Model model_;
  std::vector<std::vector<Variable>> vars_;
};

std::vector<std::string> LPAssignment(const SolverType solver_type,
                                      const SolveArguments& args) {
  constexpr int n = 5;
  AssignmentProblem assignment(n);
  const SolveResult result =
      Solve(assignment.model(), solver_type, args).value();
  CHECK_OK(result.termination.EnsureIsOptimal());
  CHECK_LE(std::abs(result.objective_value() - n), 1e-4)
      << result.objective_value();
  return assignment.SolutionFingerprint(result);
}

TEST_P(LpParameterTest, RandomSeedLp) {
  if (!SupportsRandomSeed()) {
    GTEST_SKIP() << "Random seed not supported. Ignoring this test.";
  }
  absl::flat_hash_set<std::vector<std::string>> solutions_seen;
  for (int seed = 10; seed < 200; seed += 10) {
    std::vector<std::string> result_for_seed;
    for (int trial = 0; trial < 10; ++trial) {
      SolveArguments args;
      args.parameters.random_seed = seed;
      // When the problem is solved in presolve, solvers typically give the same
      // solution every time, regardless of the seed.
      args.parameters.presolve = Emphasis::kOff;
      std::vector<std::string> result = LPAssignment(TestedSolver(), args);
      if (trial == 0) {
        result_for_seed = result;
        solutions_seen.insert(result_for_seed);
      } else {
        ASSERT_EQ(result_for_seed, result)
            << "seed: " << seed << " trail: " << trial;
      }
    }
  }
  // Drawing 20 items from a very large number with replacement, the probability
  // of getting at least 3 unique is very high.
  EXPECT_GE(solutions_seen.size(), 3);
}

SolveStats LPForPresolve(SolverType solver_type, Emphasis presolve_emphasis) {
  AssignmentProblem assignment_problem(6);
  assignment_problem.MakePresolveOptimal();
  SolveArguments args;
  args.parameters.presolve = presolve_emphasis;
  const SolveResult result =
      Solve(assignment_problem.model(), solver_type, args).value();
  CHECK_OK(result.termination.EnsureIsOptimal());
  return result.solve_stats;
}

TEST_P(LpParameterTest, PresolveOff) {
  if (!SupportsPresolve()) {
    GTEST_SKIP() << "Presolve emphasis not supported. Ignoring this test.";
  }
  const SolveStats stats = LPForPresolve(TestedSolver(), Emphasis::kOff);
  EXPECT_GT(stats.simplex_iterations + stats.barrier_iterations +
                stats.first_order_iterations,
            0);
}

TEST_P(LpParameterTest, PresolveOn) {
  if (!SupportsPresolve()) {
    GTEST_SKIP() << "Presolve emphasis not supported. Ignoring this test.";
  }
  const SolveStats stats = LPForPresolve(TestedSolver(), Emphasis::kMedium);
  EXPECT_EQ(stats.simplex_iterations + stats.barrier_iterations +
                stats.first_order_iterations,
            0);
}

// This test doesn't really distinguish between primal and dual simplex, a
// better test is possible.
absl::StatusOr<SolveStats> SolveForLpAlgorithm(SolverType solver_type,
                                               LPAlgorithm algorithm) {
  AssignmentProblem assignment_problem(6);
  SolveArguments args;
  args.parameters.lp_algorithm = algorithm;
  // Make sure that the underlying solver doesn't use an ensemble of LP
  // algorithms.
  // TODO(b/271098533): use solver capabilities instead. Note that HiGHS only
  // lets you control the number of threads by setting a global that is not
  // synchronized, so we disable it here.
  if (solver_type != SolverType::kHighs) {
    args.parameters.threads = 1;
  }
  ASSIGN_OR_RETURN(const SolveResult result,
                   Solve(assignment_problem.model(), solver_type, args));
  RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
  return result.solve_stats;
}

TEST_P(LpParameterTest, LPAlgorithmPrimal) {
  if (!SupportsSimplex()) {
    EXPECT_THAT(
        SolveForLpAlgorithm(TestedSolver(), LPAlgorithm::kPrimalSimplex),
        StatusIs(absl::StatusCode::kInvalidArgument,
                 AnyOf(HasSubstr("LP_ALGORITHM_PRIMAL_SIMPLEX"),
                       HasSubstr("lp_algorithm"))));
    return;
  }
  ASSERT_OK_AND_ASSIGN(
      const SolveStats stats,
      SolveForLpAlgorithm(TestedSolver(), LPAlgorithm::kPrimalSimplex));

  EXPECT_GT(stats.simplex_iterations, 0);
  EXPECT_EQ(stats.barrier_iterations, 0);
  EXPECT_EQ(stats.first_order_iterations, 0);
}

TEST_P(LpParameterTest, LPAlgorithmDual) {
  if (!SupportsSimplex()) {
    EXPECT_THAT(SolveForLpAlgorithm(TestedSolver(), LPAlgorithm::kDualSimplex),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         AnyOf(HasSubstr("LP_ALGORITHM_DUAL_SIMPLEX"),
                               HasSubstr("lp_algorithm"))));
    return;
  }
  ASSERT_OK_AND_ASSIGN(
      const SolveStats stats,
      SolveForLpAlgorithm(TestedSolver(), LPAlgorithm::kDualSimplex));

  EXPECT_GT(stats.simplex_iterations, 0);
  EXPECT_EQ(stats.barrier_iterations, 0);
  EXPECT_EQ(stats.first_order_iterations, 0);
}

TEST_P(LpParameterTest, LPAlgorithmBarrier) {
  if (!SupportsBarrier()) {
    EXPECT_THAT(SolveForLpAlgorithm(TestedSolver(), LPAlgorithm::kBarrier),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         AnyOf(HasSubstr("LP_ALGORITHM_BARRIER"),
                               HasSubstr("lp_algorithm"))));
    return;
  }
  ASSERT_OK_AND_ASSIGN(
      const SolveStats stats,
      SolveForLpAlgorithm(TestedSolver(), LPAlgorithm::kBarrier));
  // As of 2023-11-30 ecos_solver does not set the iteration count.
  if (GetParam().solver_type != SolverType::kEcos) {
    EXPECT_GT(stats.barrier_iterations, 0);
  }
  // We make no assertions on simplex iterations, we do not specify if
  // crossover takes place.
}

TEST_P(LpParameterTest, LPAlgorithmFirstOrder) {
  if (!SupportsFirstOrder()) {
    EXPECT_THAT(SolveForLpAlgorithm(TestedSolver(), LPAlgorithm::kFirstOrder),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         AnyOf(HasSubstr("LP_ALGORITHM_FIRST_ORDER"),
                               HasSubstr("lp_algorithm"))));
    return;
  }
  ASSERT_OK_AND_ASSIGN(
      const SolveStats stats,
      SolveForLpAlgorithm(TestedSolver(), LPAlgorithm::kFirstOrder));
  EXPECT_EQ(stats.simplex_iterations, 0);
  EXPECT_EQ(stats.barrier_iterations, 0);
  EXPECT_GT(stats.first_order_iterations, 0);
}

absl::StatusOr<SolveResult> LPForIterationLimit(
    const SolverType solver_type, const std::optional<LPAlgorithm> algorithm,
    const int n, const bool supports_presolve) {
  // The unique optimal solution to this problem is x[i] = 1/2 for all i, with
  // an objective value of n/2.
  Model model("Iteration limit LP");
  std::vector<Variable> x;
  for (int i = 0; i < n; ++i) {
    x.push_back(model.AddContinuousVariable(0.0, 1.0));
  }
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      model.AddLinearConstraint(x[i] + x[j] <= 1);
    }
  }
  model.Maximize(Sum(x));
  SolveArguments args;
  args.parameters.lp_algorithm = algorithm;
  if (supports_presolve) {
    args.parameters.presolve = Emphasis::kOff;
  }
  args.parameters.iteration_limit = 1;
  return Solve(model, solver_type, args);
}

TEST_P(LpParameterTest, IterationLimitPrimalSimplex) {
  if (!SupportsSimplex()) {
    GTEST_SKIP() << "Simplex not supported. Ignoring this test.";
  }
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      LPForIterationLimit(TestedSolver(), LPAlgorithm::kPrimalSimplex, 3,
                          SupportsPresolve()));
  EXPECT_THAT(result,
              TerminatesWithLimit(
                  Limit::kIteration,
                  /*allow_limit_undetermined=*/!GetParam().reports_limits));
}

TEST_P(LpParameterTest, IterationLimitDualSimplex) {
  if (!SupportsSimplex()) {
    GTEST_SKIP() << "Simplex not supported. Ignoring this test.";
  }
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      LPForIterationLimit(TestedSolver(), LPAlgorithm::kDualSimplex, 3,
                          SupportsPresolve()));
  EXPECT_THAT(result,
              TerminatesWithLimit(
                  Limit::kIteration,
                  /*allow_limit_undetermined=*/!GetParam().reports_limits));
}

TEST_P(LpParameterTest, IterationLimitBarrier) {
  if (!SupportsBarrier()) {
    GTEST_SKIP() << "Barrier not supported. Ignoring this test.";
  }
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      LPForIterationLimit(TestedSolver(), LPAlgorithm::kBarrier, 3,
                          SupportsPresolve()));
  EXPECT_THAT(result,
              TerminatesWithLimit(
                  Limit::kIteration,
                  /*allow_limit_undetermined=*/!GetParam().reports_limits));
}

TEST_P(LpParameterTest, IterationLimitFirstOrder) {
  if (!SupportsFirstOrder()) {
    GTEST_SKIP() << "First order methods not supported. Ignoring this test.";
  }
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      LPForIterationLimit(TestedSolver(), LPAlgorithm::kFirstOrder, 3,
                          SupportsPresolve()));
  EXPECT_THAT(result,
              TerminatesWithLimit(
                  Limit::kIteration,
                  /*allow_limit_undetermined=*/!GetParam().reports_limits));
}

TEST_P(LpParameterTest, IterationLimitUnspecified) {
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      LPForIterationLimit(TestedSolver(), std::nullopt, 3, SupportsPresolve()));
  EXPECT_THAT(result,
              TerminatesWithLimit(
                  Limit::kIteration,
                  /*allow_limit_undetermined=*/!GetParam().reports_limits));
}

// This test is a little fragile as we do not set an initial basis, perhaps
// worth reconsidering if it becomes an issue.
TEST_P(LpParameterTest, ObjectiveLimitMaximization) {
  // We only expect this to work for primal simplex.
  if (!GetParam().supports_simplex) {
    return;
  }
  // max 10x + 9y + 8z
  // s.t. x + y <= 1
  //      x + z <= 1
  //      x, y, z in [0, 1].
  //
  // The optimal solution is (0, 1, 1), objective value 17.
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0);
  const Variable y = model.AddContinuousVariable(0.0, 1.0);
  const Variable z = model.AddContinuousVariable(0.0, 1.0);
  model.AddLinearConstraint(x + y <= 1.0);
  model.AddLinearConstraint(x + z <= 1.0);
  model.Maximize(10 * x + 9 * y + 8 * z);

  // We can stop as soon as we find a solution with objective at least -0.5,
  // i.e. on any feasible solution.
  SolveParameters params = {.objective_limit = -0.5,
                            .lp_algorithm = LPAlgorithm::kPrimalSimplex,
                            .presolve = Emphasis::kOff};
  const absl::StatusOr<SolveResult> result =
      Solve(model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().supports_objective_limit) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("objective_limit")));
    return;
  }
  EXPECT_THAT(result,
              IsOkAndHolds(TerminatesWithReasonFeasible(
                  Limit::kObjective,
                  /*allow_limit_undetermined=*/!GetParam().reports_limits)));
  // When the optimal solution is worse than objective_limit, the parameter
  // has no effect on the returned SolveResult and we return the optimal
  // solution.
  params.objective_limit = 18.0;
  EXPECT_THAT(Solve(model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(17.0)));
}

// This test is a little fragile as we do not set an initial basis, perhaps
// worth reconsidering if it becomes an issue.
TEST_P(LpParameterTest, ObjectiveLimitMinimization) {
  if (!GetParam().supports_objective_limit) {
    // We have already tested the solver errors in ObjectiveLimitMaximization.
    return;
  }
  // We only expect this to work for primal simplex.
  if (!GetParam().supports_simplex) {
    return;
  }
  // min 10x + 9y + 8z
  // s.t. x + y >= 1
  //      x + z >= 1
  //      x, y, z in [0, 1].
  //
  // The optimal solution is (1, 0, 0), objective value 10.
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0);
  const Variable y = model.AddContinuousVariable(0.0, 1.0);
  const Variable z = model.AddContinuousVariable(0.0, 1.0);
  model.AddLinearConstraint(x + y >= 1.0);
  model.AddLinearConstraint(x + z >= 1.0);
  model.Minimize(10 * x + 9 * y + 8 * z);

  // We can stop as soon as we find a solution with objective at most 30.0,
  // i.e. on any feasible solution.
  SolveParameters params = {.objective_limit = 30.0,
                            .lp_algorithm = LPAlgorithm::kPrimalSimplex,
                            .presolve = Emphasis::kOff};
  EXPECT_THAT(Solve(model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(TerminatesWithReasonFeasible(
                  Limit::kObjective,
                  /*allow_limit_undetermined=*/!GetParam().reports_limits)));
  // When the optimal solution is worse than objective_limit, the parameter
  // has no effect on the returned SolveResult and we return the optimal
  // solution.
  params.objective_limit = 7.0;
  EXPECT_THAT(Solve(model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(10.0)));
}

// This test is a little fragile as we do not set an initial basis, perhaps
// worth reconsidering if it becomes an issue.
TEST_P(LpParameterTest, BestBoundLimitMaximize) {
  // We only expect this to work for dual simplex.
  if (!GetParam().supports_simplex) {
    return;
  }
  if (GetParam().solver_type == SolverType::kHighs) {
    // TODO(b/272312674): bug in HiGHS breaks this test.
    GTEST_SKIP() << "TODO(b/272312674): Highs appears to have a bug where "
                    "best_bound_limit is only supported for minimization.";
  }
  // max  3x + 2y + z
  // s.t. x + y  + z <= 1.5
  //      x, y, in [0, 1]
  //
  // The optimal solution is (1, 0.5, 0) with objective value 4.
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0);
  const Variable y = model.AddContinuousVariable(0.0, 1.0);
  const Variable z = model.AddContinuousVariable(0.0, 1.0);
  model.AddLinearConstraint(x + y + z <= 1.5);
  model.Maximize(3 * x + 2 * y + z);

  // With best bound limit of 6.5, we will find a dual feasible solution with
  // dual objective better (smaller) than 3.5 before finding the optimal
  // solution (e.g. (x, y, z) = (1, 1, 1), objective = 6).
  SolveParameters params = {.best_bound_limit = 6.5,
                            .lp_algorithm = LPAlgorithm::kDualSimplex,
                            .presolve = Emphasis::kOff};
  const absl::StatusOr<SolveResult> result =
      Solve(model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().supports_best_bound_limit) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("best_bound_limit")));
    return;
  }
  EXPECT_THAT(result,
              IsOkAndHolds(TerminatesWithReasonNoSolutionFound(
                  Limit::kObjective,
                  /*allow_limit_undetermined=*/!GetParam().reports_limits)));
  // When the optimal solution is better than best_bound_limit, the parameter
  // has no effect on the returned SolveResult and we return the optimal
  // solution.
  params.best_bound_limit = 3.5;
  EXPECT_THAT(Solve(model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(4.0)));
}

// This test is a little fragile as we do not set an initial basis, perhaps
// worth reconsidering if it becomes an issue.
TEST_P(LpParameterTest, BestBoundLimitMinimize) {
  if (!GetParam().supports_objective_limit) {
    // We have already tested the solver errors in BestBoundLimitMaximize.
    return;
  }
  // We only expect this to work for dual simplex.
  if (!GetParam().supports_simplex) {
    return;
  }
  // min  2x + y
  // s.t. x + y >= 1
  //      x, y, in [0, 1]
  //
  // The optimal solution is (0, 1) with objective value 1.
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0);
  const Variable y = model.AddContinuousVariable(0.0, 1.0);
  model.AddLinearConstraint(x + y >= 1.0);
  model.Minimize(2 * x + y);

  // With best bound limit of -0.5, we will find a dual feasible solution with
  // dual objective better (larger) than -0.5 before finding the optimal
  // solution (e.g. (x, y) = (0, 0), objective = 0).
  SolveParameters params = {.best_bound_limit = -0.5,
                            .lp_algorithm = LPAlgorithm::kDualSimplex,
                            .presolve = Emphasis::kOff};
  EXPECT_THAT(Solve(model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(TerminatesWithReasonNoSolutionFound(
                  Limit::kObjective,
                  /*allow_limit_undetermined=*/!GetParam().reports_limits)));
  // When the optimal solution is better than best_bound_limit, the parameter
  // has no effect on the returned SolveResult and we return the optimal
  // solution.
  params.best_bound_limit = 1.5;
  EXPECT_THAT(Solve(model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(1.0)));
}

TEST_P(LpParameterTest, CutoffLimitMaximize) {
  // max  2x + y
  // s.t. x + y <= 1
  //      x, y, in [0, 1]
  //
  // The optimal solution is (1, 0) with objective value 2.
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0);
  const Variable y = model.AddContinuousVariable(0.0, 1.0);
  model.AddLinearConstraint(x + y <= 1.0);
  model.Maximize(2 * x + y);
  // When the optimal solution is worse than cutoff, no solution information is
  // returned and we return Limit::kCutoff.
  SolveParameters params = {.cutoff_limit = 3.5, .presolve = Emphasis::kOff};
  const absl::StatusOr<SolveResult> result =
      Solve(model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().supports_cutoff) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("cutoff_limit")));
    return;
  }
  EXPECT_THAT(result, IsOkAndHolds(
                          TerminatesWithReasonNoSolutionFound(Limit::kCutoff)));
  // When the optimal solution is better than cutoff, the parameter has no
  // effect on the returned SolveResult (at least for problems with a unique
  // solution, it may change the nodes visited still) and we return the optimal
  // solution.
  params.cutoff_limit = 1.5;
  EXPECT_THAT(Solve(model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(2.0)));
}

TEST_P(LpParameterTest, CutoffLimitMinimize) {
  if (!GetParam().supports_cutoff) {
    // We have already tested the solver errors in CutoffLimitMaximize.
    return;
  }
  // min  2x + y
  // s.t. x + y >= 1
  //      x, y, in [0, 1]
  //
  // The optimal solution is (0, 1) with objective value 1.
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0);
  const Variable y = model.AddContinuousVariable(0.0, 1.0);
  model.AddLinearConstraint(x + y >= 1.0);
  model.Minimize(2 * x + y);
  // When the optimal solution is worse than cutoff, no solution information is
  // returned and we return Limit::kCutoff.
  SolveParameters params = {.cutoff_limit = -0.5, .presolve = Emphasis::kOff};
  EXPECT_THAT(
      Solve(model, GetParam().solver_type, {.parameters = params}),
      IsOkAndHolds(TerminatesWithReasonNoSolutionFound(Limit::kCutoff)));
  // When the optimal solution is better than cutoff, the parameter has no
  // effect on the returned SolveResult (at least for problems with a unique
  // solution, it may change the nodes visited still) and we return the optimal
  // solution.
  params.cutoff_limit = 1.5;
  EXPECT_THAT(Solve(model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(1.0)));
}

// TODO(b/272268188): test the interaction between cutoff and primal + dual
// infeasibility.

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
