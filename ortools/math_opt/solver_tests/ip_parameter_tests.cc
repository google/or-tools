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

// How each parameter is tested:
//  Parameter          | IpParameterTest | generic_test.h  | LargeInstanceTest
//  -------------------|-----------------|-----------------|-------------------
//  time_limit         |                 | x               | x
//  iteration_limit    | x               |                 | x
//  node_limit         | x               |                 | x
//  cutoff_limit       | x               |                 | x
//  objective_limit    | x               |                 | x
//  best_bound_limit   | x               |                 | x
//  solution_limit     | x               |                 | x
//  enable_output      |                 | x               |
//  threads            |                 |                 |
//  random_seed        | x               |                 |
//  absolute_gap_tol   | x               |                 | x
//  relative_gap_tol   | x               |                 | x
//  solution_pool_size |                 |                 |
//  lp_algorithm       | x (bad)         |                 |
//  presolve           | x               |                 |
//  cuts               | x               |                 | x
//  heuristics         |                 |                 |
//  scaling            |                 |                 |
//
// solution_pool_size is tested in ip_multiple_solutions_tests.cc.
//
// Testing some parameters requires that other parameters/stats are supported:
//  * presolve (IpParameterTest) requires message callbacks
//  * cuts (IpParameterTest) must disable presolve
//  * cuts (LargeInstanceTest) needs node_limit=1.
//  * lp_algorithm (IpParameterTest) test is just a no-crash test without
//    supporting iteration counts in SolveStats.
//  * solution_limit (IpParameterTest) requires a hint.
//
// TODO(b/180024054): add tests for:
//  * threads
//  * heuristics
//  * scaling
//  * lp_algorithm, differentiate between primal and dual simplex. E.g. find a
//    problem with LP relaxation that is both infeasible and dual infeasible,
//    disable presolve, and solve. When using primal simplex, we should get
//    termination reason kInfeasible, but dual simplex should give
//    kInfeasibleOrUnbounded.
//  * TODO(b/272268188): test the interaction between cutoff and primal + dual
//    infeasibility.
//
#include "ortools/math_opt/solver_tests/ip_parameter_tests.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/io/mps_converter.h"
#include "ortools/math_opt/solver_tests/test_models.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/testing_utils.h"

namespace operations_research {
namespace math_opt {

std::ostream& operator<<(std::ostream& ostr,
                         const SolveResultSupport& solve_result_support) {
  std::vector<std::string> supported;
  if (solve_result_support.termination_limit) {
    supported.push_back("termination_limit");
  }
  if (solve_result_support.iteration_stats) {
    supported.push_back("iteration_stats");
  }
  if (solve_result_support.node_count) {
    supported.push_back("node_count");
  }
  ostr << "{ " << absl::StrJoin(supported, ", ") << " }";
  return ostr;
}

std::ostream& operator<<(std::ostream& ostr,
                         const ParameterSupport& param_support) {
  std::vector<std::string> supported;
  if (param_support.supports_iteration_limit) {
    supported.push_back("supports_iteration_limit");
  }
  if (param_support.supports_node_limit) {
    supported.push_back("supports_node_limit");
  }
  if (param_support.supports_cutoff) {
    supported.push_back("supports_cutoff");
  }
  if (param_support.supports_objective_limit) {
    supported.push_back("supports_objective_limit");
  }
  if (param_support.supports_bound_limit) {
    supported.push_back("supports_bound_limit");
  }
  if (param_support.supports_solution_limit_one) {
    supported.push_back("supports_solution_limit_one");
  }
  if (param_support.supports_one_thread) {
    supported.push_back("supports_one_thread");
  }
  if (param_support.supports_n_threads) {
    supported.push_back("supports_n_threads");
  }
  if (param_support.supports_random_seed) {
    supported.push_back("supports_random_seed");
  }
  if (param_support.supports_absolute_gap_tolerance) {
    supported.push_back("supports_absolute_gap_tolerance");
  }
  if (param_support.supports_lp_algorithm_simplex) {
    supported.push_back("supports_lp_algorithm_simplex");
  }
  if (param_support.supports_lp_algorithm_barrier) {
    supported.push_back("supports_lp_algorithm_barrier");
  }
  if (param_support.supports_presolve) {
    supported.push_back("supports_presolve");
  }
  if (param_support.supports_cuts) {
    supported.push_back("supports_cuts");
  }
  if (param_support.supports_heuristics) {
    supported.push_back("supports_heuristics");
  }
  if (param_support.supports_scaling) {
    supported.push_back("supports_scaling");
  }
  ostr << "{ " << absl::StrJoin(supported, ", ") << " }";
  return ostr;
}

std::ostream& operator<<(std::ostream& ostr,
                         const IpParameterTestParameters& params) {
  ostr << "{ name: " << params.name;
  ostr << ", solver_type: " << EnumToString(params.solver_type);
  ostr << ", parameter_support: " << params.parameter_support;
  ostr << ", hint_supported: " << params.hint_supported;
  ostr << ", solve_result_support: " << params.solve_result_support;
  ostr << ", presolved_regexp: \"" << absl::CEscape(params.presolved_regexp);
  ostr << "\", stop_before_optimal: "
       << ProtobufShortDebugString(params.stop_before_optimal.Proto()) << " }";
  return ostr;
}

std::ostream& operator<<(std::ostream& out,
                         const LargeInstanceTestParams& params) {
  out << "{ name: " << params.name;
  out << ", solver_type: " << EnumToString(params.solver_type);
  out << ", base_parameters: "
      << ProtobufShortDebugString(params.base_parameters.Proto());
  out << ", parameter_support: " << params.parameter_support;
  out << ", allow_limit_undetermined: " << params.allow_limit_undetermined
      << " }";
  return out;
}

namespace {

using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

// Adds the trio of constraints:
//   x + y >= 1
//   y + z >= 1
//   x + z >= 1
// In the vertex cover problem, you have a graph and must pick a subset of the
// nodes so that every edge has at least one endpoint selected. If x, y, and z
// are nodes in this graph, adding Triangle(x, y, z) ensures that two of x, y,
// and z must be selected in any vertex cover.
void AddTriangle(Model& model, Variable x, Variable y, Variable z) {
  model.AddLinearConstraint(x + y >= 1);
  model.AddLinearConstraint(y + z >= 1);
  model.AddLinearConstraint(z + x >= 1);
}

// xs, ys, and zs must have all size 3. Adds the constraints:
//   Triangle(x1, x2, x3),
//   Triangle(y1, y2, y3),
//   Triangle(z1, z2, z3),
//   Triangle(x1, y1, z1),
//   Triangle(x2, y2, z2),
//   Triangle(x3, y3, z3),
//
// Adding this constraint ensures that the minimum vertex cover (pick a subset
// of the 9 nodes such that for every edge, at least one node is selected) has
// size at least 6. There are many possible symmetric solutions, any solution
// has two xs, two ys, two z2, two ones, two twos, and two threes, e.g.
//   x1, y1,
//   z2, z3,
//   x2, y3.
void AddSixTriangles(Model& model, absl::Span<const Variable> xs,
                     absl::Span<const Variable> ys,
                     absl::Span<const Variable> zs) {
  AddTriangle(model, xs[0], xs[1], xs[2]);
  AddTriangle(model, ys[0], ys[1], ys[2]);
  AddTriangle(model, zs[0], zs[1], zs[2]);
  AddTriangle(model, xs[0], ys[0], zs[0]);
  AddTriangle(model, xs[1], ys[1], zs[1]);
  AddTriangle(model, xs[2], ys[2], zs[2]);
}

// A MIP with an LP relaxation of 13.5 and a best integer solution of 18. The
// MIP has a large number of symmetric solutions. It is given by:
//
// min sum_{i=1}^3 sum_{j=1}^3 x_ij + y_ij + z_ij
//
// SixTriangles(x11, x12, x13, y11, y12, y13, z11, z12, z13)
// SixTriangles(x21, x22, x23, y21, y22, y23, z21, z22, z23)
// SixTriangles(x31, x32, x33, y31, y32, y33, z31, z32, z33)
//
// for j = 1..3:
//   Triangle(x1j, x2j, x3j)
//   Triangle(y1j, y2j, y3j)
//   Triangle(z1j, z2j, z3j)
//
// It can be interpreted as a large instance of vertex cover on 27 nodes.
class VertexCover {
 public:
  VertexCover() {
    xs_ = MakeVars3By3("x");
    ys_ = MakeVars3By3("y");
    zs_ = MakeVars3By3("z");
    LinearExpression objective;
    for (int i = 0; i < 3; ++i) {
      objective += Sum(xs_[i]) + Sum(ys_[i]) + Sum(zs_[i]);
    }
    model_.Minimize(objective);
    for (int i = 0; i < 3; ++i) {
      AddSixTriangles(model_, xs_[i], ys_[i], zs_[i]);
    }
    for (int j = 0; j < 3; ++j) {
      AddTriangle(model_, xs_[0][j], xs_[1][j], xs_[2][j]);
      AddTriangle(model_, ys_[0][j], ys_[1][j], ys_[2][j]);
      AddTriangle(model_, zs_[0][j], zs_[1][j], zs_[2][j]);
    }
  }

  Model& model() { return model_; }

  std::vector<std::string> SolutionFingerprint(const SolveResult& result) {
    std::vector<std::string> var_names;
    for (const Variable v : model_.Variables()) {
      double val = result.variable_values().at(v);
      if (val > 0.5) {
        var_names.push_back(std::string(v.name()));
      }
    }
    std::sort(var_names.begin(), var_names.end());
    return var_names;
  }

  static absl::StatusOr<std::vector<std::string>> SolveAndFingerprint(
      const SolverType solver_type, const SolveParameters& params) {
    VertexCover vertex_cover;
    ASSIGN_OR_RETURN(
        SolveResult result,
        Solve(vertex_cover.model(), solver_type, {.parameters = params}));
    RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
    if (std::abs(result.objective_value() - 18.0) > 1e-4) {
      return util::InternalErrorBuilder()
             << "expected objective value of 18, found: "
             << result.objective_value();
    }
    return result.has_primal_feasible_solution()
               ? vertex_cover.SolutionFingerprint(result)
               : std::vector<std::string>();
  }

 private:
  // Adds 9 binary variables to the model and returns them in a 3x3 array.
  std::vector<std::vector<Variable>> MakeVars3By3(absl::string_view prefix) {
    std::vector<std::vector<Variable>> result(3);
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        result[i].push_back(
            model_.AddBinaryVariable(absl::StrCat(prefix, "_", i, "_", j)));
      }
    }
    return result;
  }

  Model model_;
  std::vector<std::vector<Variable>> xs_;
  std::vector<std::vector<Variable>> ys_;
  std::vector<std::vector<Variable>> zs_;
};

// On a symmetric MIP with multiple optimal solutions, test that:
//  * If we use the same seed, we get the same result.
//  * If we use different seeds, we at least sometimes get different results.
//
// Warning: this test is quite fragile. I don't understand why, but if presolve
// is disabled, the test fails for gSCIP and Gurobi, where both solvers always
// find the same solution regardless of the seed.
//
// WARNING: the solve must be deterministic for this test to work. We set
// threads=1 if supported, as usually multi-threaded solves are not
// deterministic. HiGHS does not yet support this, but appears to still be
// deterministic.
TEST_P(IpParameterTest, RandomSeedIP) {
  absl::flat_hash_set<std::vector<std::string>> solutions_seen;
  for (int seed = 10; seed < 200; seed += 10) {
    SCOPED_TRACE(absl::StrCat("seed ", seed));
    std::vector<std::string> result_for_seed;
    for (int trial = 0; trial < 10; ++trial) {
      SCOPED_TRACE(absl::StrCat("trial ", trial));
      SolveParameters params = {.random_seed = seed};
      if (GetParam().parameter_support.supports_one_thread) {
        params.threads = 1;
      }
      absl::StatusOr<std::vector<std::string>> fingerprint =
          VertexCover::SolveAndFingerprint(TestedSolver(), params);
      if (!GetParam().parameter_support.supports_random_seed) {
        EXPECT_THAT(fingerprint, StatusIs(absl::StatusCode::kInvalidArgument,
                                          HasSubstr("random_seed")));
        return;
      }
      if (trial == 0) {
        result_for_seed = *fingerprint;
        solutions_seen.insert(result_for_seed);
      } else {
        ASSERT_EQ(result_for_seed, *fingerprint);
      }
    }
  }
  if (GetParam().solver_type != SolverType::kCpSat) {
    // Drawing 20 items from a very large number with replacement, the
    // probability of getting at least 3 unique is very high.
    EXPECT_GE(solutions_seen.size(), 3);
  }
}

// Solves the problem
//   max sum_i y_i
//       x_i >= y_i
//       x_i + y_1 == 1
//       x_i, y_i \in {0, 1}
//
// This is linearly separable in i, and we must have x_i = 1, y_i = 0 for all i.
// Presolve can typically detect this.
absl::StatusOr<std::pair<SolveStats, std::string>> SolveForIPPresolve(
    SolverType solver_type, bool do_presolve) {
  std::ostringstream oss;
  Model model("easy_presolve_model");
  model.set_maximize();
  for (int i = 0; i < 4; ++i) {
    Variable x = model.AddVariable(0.0, 1.0, true);
    Variable y = model.AddVariable(0.0, 1.0, true);
    model.AddToObjective(y);
    model.AddLinearConstraint(x + y == 1.0);
    model.AddLinearConstraint(x >= y);
  }

  SolveArguments args;
  args.parameters.presolve = do_presolve ? Emphasis::kMedium : Emphasis::kOff;
  args.message_callback = PrinterMessageCallback(oss);
  ASSIGN_OR_RETURN(const SolveResult result, Solve(model, solver_type, args));
  RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
  return std::make_pair(result.solve_stats, oss.str());
}

// On asserting that presolve is working:
//   We have created a problem and can be entirely solved in presolve (all
// variables and constraints removed) using basic reductions that should be
// supported by most solvers. It would be easiest to simply look at the size of
// the problem after presolve, but this not universally supported (with Gurobi,
// it is only visible in a callback). Instead, we check the logs for a solver
// specific string that occurs only if presolve solves the model. Further, for
// solvers that return simplex/barrier iterations, we check that there are no
// such iterations iff presolve is on.
TEST_P(IpParameterTest, PresolveOff) {
  if (!GetParam().parameter_support.supports_presolve) {
    // We test presolve being unsupported in IpParameterTest.PresolveOn
    return;
  }
  absl::StatusOr<std::pair<SolveStats, std::string>> stats_and_logs =
      SolveForIPPresolve(TestedSolver(), /*do_presolve=*/false);
  ASSERT_OK(stats_and_logs);
  const auto [solve_stats, logs] = stats_and_logs.value();
  if (GetParam().solve_result_support.iteration_stats) {
    EXPECT_GE(solve_stats.barrier_iterations + solve_stats.simplex_iterations +
                  solve_stats.first_order_iterations,
              1);
  }
#if !defined(_MSC_VER)
  EXPECT_THAT(logs, Not(testing::ContainsRegex(GetParam().presolved_regexp)));
#endif
}

TEST_P(IpParameterTest, PresolveOn) {
  absl::StatusOr<std::pair<SolveStats, std::string>> stats_and_logs =
      SolveForIPPresolve(TestedSolver(), /*do_presolve=*/true);
  if (!GetParam().parameter_support.supports_presolve) {
    EXPECT_THAT(stats_and_logs, StatusIs(absl::StatusCode::kInvalidArgument,
                                         HasSubstr("presolve")));
    return;
  }
  ASSERT_OK(stats_and_logs);
  const auto [solve_stats, logs] = stats_and_logs.value();
  if (!GetParam().solve_result_support.iteration_stats) {
    EXPECT_EQ(solve_stats.barrier_iterations, 0);
    EXPECT_EQ(solve_stats.simplex_iterations, 0);
    EXPECT_EQ(solve_stats.first_order_iterations, 0);
  }
#if !defined(_MSC_VER)
  EXPECT_THAT(logs, testing::ContainsRegex(GetParam().presolved_regexp));
#endif
}

// Requires disabling presolve and cuts is supported (or status errors).
absl::StatusOr<SolveStats> SolveForCuts(SolverType solver_type, bool use_cuts) {
  Model model;
  model.set_maximize();
  for (int i = 0; i < 10; ++i) {
    const Variable x = model.AddVariable(0.0, 1.0, true);
    const Variable y = model.AddVariable(0.0, 1.0, true);
    model.AddToObjective(2 * x + y);
    // With the addition of the knapsack cut:
    //   x + y <= 1
    // the problem becomes integral.
    model.AddLinearConstraint(3 * x + 2 * y <= 4);
  }

  SolveArguments args;
  args.parameters.presolve = Emphasis::kOff;
  args.parameters.cuts = use_cuts ? Emphasis::kMedium : Emphasis::kOff;
  ASSIGN_OR_RETURN(const SolveResult result, Solve(model, solver_type, args));
  RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
  return result.solve_stats;
}

TEST_P(IpParameterTest, CutsOff) {
  if (!GetParam().parameter_support.supports_presolve) {
    GTEST_SKIP() << "Skipping test, this test requires disabling presolve "
                    "which is not supported.";
  }
  if (!GetParam().parameter_support.supports_cuts) {
    // If cuts are not supported we test this in IpParameterTest.CutsOn
    return;
  }
  ASSERT_OK_AND_ASSIGN(const SolveStats solve_stats,
                       SolveForCuts(TestedSolver(), /*use_cuts=*/false));
  if (GetParam().solve_result_support.node_count) {
    EXPECT_GT(solve_stats.node_count, 1);
  }
}
TEST_P(IpParameterTest, CutsOn) {
  if (!GetParam().parameter_support.supports_presolve) {
    GTEST_SKIP() << "Skipping test, this test requires disabling presolve "
                    "which is not supported.";
  }
  absl::StatusOr<SolveStats> solve_stats =
      SolveForCuts(TestedSolver(), /*use_cuts=*/true);
  if (!GetParam().parameter_support.supports_cuts) {
    EXPECT_THAT(solve_stats, StatusIs(absl::StatusCode::kInvalidArgument,
                                      HasSubstr("cuts")));
    return;
  }
  ASSERT_OK(solve_stats);
  if (GetParam().solve_result_support.node_count) {
    EXPECT_EQ(solve_stats->node_count, 1);
  }
}

// This method doesn't give any way to distinguish between primal and dual
// simplex, a better test is needed, see comment at top of file for an idea.
absl::StatusOr<SolveStats> SolveForRootLp(
    const SolverType solver_type, const LPAlgorithm algorithm,
    const ParameterSupport& parameter_support) {
  VertexCover vertex_cover;
  SolveParameters params = {.lp_algorithm = algorithm};
  // Try to make sure that only one algorithm is used
  if (parameter_support.supports_one_thread) {
    params.threads = 1;
  }
  // Avoid making too much progress before the LP, we are testing based on use
  // of the LP solver.
  if (parameter_support.supports_presolve) {
    params.presolve = Emphasis::kOff;
  }

  ASSIGN_OR_RETURN(
      const SolveResult result,
      Solve(vertex_cover.model(), solver_type, {.parameters = params}));
  RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
  return result.solve_stats;
}

TEST_P(IpParameterTest, RootLPAlgorithmPrimal) {
  const absl::StatusOr<SolveStats> stats =
      SolveForRootLp(TestedSolver(), LPAlgorithm::kPrimalSimplex,
                     GetParam().parameter_support);
  if (!GetParam().parameter_support.supports_lp_algorithm_simplex) {
    EXPECT_THAT(stats, StatusIs(absl::StatusCode::kInvalidArgument,
                                HasSubstr("lp_algorithm")));
    return;
  }
  ASSERT_OK(stats);
  if (GetParam().solve_result_support.iteration_stats) {
    EXPECT_GT(stats->simplex_iterations, 0);
    EXPECT_EQ(stats->barrier_iterations, 0);
    EXPECT_EQ(stats->first_order_iterations, 0);
  }
}

TEST_P(IpParameterTest, RootLPAlgorithmDual) {
  const absl::StatusOr<SolveStats> stats = SolveForRootLp(
      TestedSolver(), LPAlgorithm::kDualSimplex, GetParam().parameter_support);
  if (!GetParam().parameter_support.supports_lp_algorithm_simplex) {
    EXPECT_THAT(stats, StatusIs(absl::StatusCode::kInvalidArgument,
                                HasSubstr("lp_algorithm")));
    return;
  }
  ASSERT_OK(stats);
  if (GetParam().solve_result_support.iteration_stats) {
    EXPECT_GT(stats->simplex_iterations, 0);
    EXPECT_EQ(stats->barrier_iterations, 0);
    EXPECT_EQ(stats->first_order_iterations, 0);
  }
}

TEST_P(IpParameterTest, RootLPAlgorithmBarrier) {
  const absl::StatusOr<SolveStats> stats = SolveForRootLp(
      TestedSolver(), LPAlgorithm::kBarrier, GetParam().parameter_support);
  if (!GetParam().parameter_support.supports_lp_algorithm_barrier) {
    EXPECT_THAT(stats, StatusIs(absl::StatusCode::kInvalidArgument,
                                HasSubstr("lp_algorithm")));
    return;
  }
  ASSERT_OK(stats);
  if (GetParam().solve_result_support.iteration_stats) {
    EXPECT_GT(stats->barrier_iterations, 0);
    // We make no assertions on simplex iterations, we do not specify if
    // crossover takes place.
  }
}

// No IP solver supports FOM for solving the root LP yet, update this test test
// once supported.
TEST_P(IpParameterTest, RootLPAlgorithmFirstOrder) {
  EXPECT_THAT(
      SolveForRootLp(TestedSolver(), LPAlgorithm::kFirstOrder,
                     GetParam().parameter_support),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("lp_algorithm")));
}

TEST_P(IpParameterTest, IterationLimitIP) {
  const int n = 10;
  Model model("Iteration limit IP");
  std::vector<Variable> x;
  x.reserve(n);
  for (int i = 0; i < n; ++i) {
    x.push_back(model.AddIntegerVariable(0.0, 1.0));
  }
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      model.AddLinearConstraint(3 * x[i] + 5 * x[j] <= 7);
    }
  }
  model.Maximize(Sum(x));
  SolveParameters params = {.iteration_limit = 1};
  // Weaken the solver as much as possible to make sure we make it to solving
  // the root LP.
  if (GetParam().parameter_support.supports_presolve) {
    params.presolve = Emphasis::kOff;
  }
  if (GetParam().parameter_support.supports_heuristics) {
    params.heuristics = Emphasis::kOff;
  }
  if (GetParam().parameter_support.supports_one_thread) {
    params.threads = 1;
  }
  const absl::StatusOr<SolveResult> result =
      Solve(model, TestedSolver(), {.parameters = params});
  if (!GetParam().parameter_support.supports_iteration_limit) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("iteration_limit")));
    return;
  }
  EXPECT_THAT(result, IsOkAndHolds(TerminatesWithLimit(
                          Limit::kIteration,
                          /*allow_limit_undetermined=*/!GetParam()
                              .solve_result_support.termination_limit)));
}

TEST_P(IpParameterTest, NodeLimit) {
  if (GetParam().solver_type == SolverType::kHighs) {
    GTEST_SKIP()
        << "This test does not work for HiGHS, which cannot disable cuts and "
           "where disabling primal heuristics seems to have little effect, see "
           "https://paste.googleplex.com/5694421105377280";
  }
  if (GetParam().solver_type == SolverType::kGscip) {
    GTEST_SKIP() << "This test does not work with SCIP v900";
  }
  const std::unique_ptr<const Model> model = DenseIndependentSet(true);
  SolveParameters params = {.node_limit = 1};
  // Weaken the solver as much as possible so it does not solve the problem to
  // optimality at the root.
  if (GetParam().parameter_support.supports_presolve) {
    params.presolve = Emphasis::kOff;
  }
  if (GetParam().parameter_support.supports_heuristics) {
    params.heuristics = Emphasis::kOff;
  }
  if (GetParam().parameter_support.supports_cuts) {
    params.cuts = Emphasis::kOff;
  }
  const absl::StatusOr<SolveResult> result =
      Solve(*model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().parameter_support.supports_node_limit) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("node_limit")));
    return;
  }
  EXPECT_THAT(result,
              IsOkAndHolds(TerminatesWithLimit(
                  Limit::kNode, /*allow_limit_undetermined=*/!GetParam()
                                    .solve_result_support.termination_limit)));
}

// Problem statement:
//   max   y
//   s.t.  x[i] + x[j] <= 1         for i = 1..n, j = 1..n.
//         k * Sum(x) + y <= k + 1
//         k * Sum(x) - y >= -1
//                      y >= 1
//         x[i, j] binary          for i = 1..n, j = 1..n.
//
// Note that:
//         k * z + y <= k + 1
//         k * z - y >= -1
//                 y >= 1
//
// describes the convex hull of (z, y) = (0, 1), (z, y) = (1, 1), and
// (z, y) = (1/2, k/2 + 1). Then for the problem in the (x, y) variables we
// have:
//   * The optimal objective value for the LP relaxation is k/2 + 1 and the set
//     of optimal solutions is any (x, y) such that
//        1. Sum(x) = 1/2
//        2. 0 <= x <= 1
//        3. x[i] + x[j] <= 1, for i = 1..n, j = 1..n.
//        3. y = k/2 + 1
//     (e.g. one solution is x[1] = 1/2, x[i] = 0 for i != 1 and y = k/2 + 1)
//   * The optimal objective value of the MIP is 1 and any integer feasible
//     solution is optimal. (e.g. one solution is x[1] = 1, x[i] = 0 for i != 1
//     and y = k/2 + 1)
//
// Then the LP relaxation is enough to validate any integer feasible solution to
// a relative or absolute gap of k/2.
absl::StatusOr<SolveResult> SolveForGapLimit(const int k, const int n,
                                             const SolverType solver_type,
                                             const SolveParameters params) {
  Model model("Absolute gap limit IP");
  std::vector<Variable> x;
  for (int i = 0; i < n; ++i) {
    x.push_back(model.AddBinaryVariable());
  }

  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      model.AddLinearConstraint(x[i] + x[j] <= 1);
    }
  }
  const Variable y =
      model.AddContinuousVariable(1, std::numeric_limits<double>::infinity());
  model.AddLinearConstraint(k * Sum(x) + y <= k + 1);
  model.AddLinearConstraint(k * Sum(x) - y >= -1);
  model.Maximize(y);
  return Solve(model, solver_type, {.parameters = params});
}

TEST_P(IpParameterTest, AbsoluteGapLimit) {
  if (GetParam().solver_type == SolverType::kHighs) {
    GTEST_SKIP()
        << "This test does not work for HiGHS, we cannot weaken the root node "
           "bound enough, see https://paste.googleplex.com/6416319233654784";
  }
  const int k = 10;
  const int n = 2;
  const double lp_opt = k / 2 + 1;
  const double ip_opt = 1;
  const double abs_lp_gap = lp_opt - ip_opt;
  // We will set a target gap that can be validated by lp_opt, but best_bound
  // may end up being slightly better for some solvers.
  const double best_bound_differentiator = lp_opt - abs_lp_gap / 2.0;

  // Check that best bound on default solve is close to ip_opt and hence
  // strictly smaller than best_bound_differentiator.
  ASSERT_OK_AND_ASSIGN(
      const SolveResult default_result,
      SolveForGapLimit(k, n, TestedSolver(), SolveParameters{}));
  EXPECT_EQ(default_result.termination.reason, TerminationReason::kOptimal);
  EXPECT_LT(default_result.termination.objective_bounds.dual_bound,
            ip_opt + 0.1);
  EXPECT_LT(default_result.termination.objective_bounds.dual_bound,
            best_bound_differentiator);

  // Set target gap slightly larger than abs_lp_gap and check that best bound
  // is strictly larger than best_bound_differentiator.
  SolveParameters params = {.absolute_gap_tolerance = abs_lp_gap + 0.1};
  // Weaken the solver so that we actually have a chance to run the root LP
  // and terminate early (if we solve the problem in presolve there will be no
  // gap).
  if (GetParam().parameter_support.supports_presolve) {
    params.presolve = Emphasis::kOff;
  }
  if (GetParam().parameter_support.supports_one_thread) {
    params.threads = 1;
  }
  if (GetParam().parameter_support.supports_cuts) {
    params.cuts = Emphasis::kOff;
  }
  const absl::StatusOr<SolveResult> gap_tolerance_result =
      SolveForGapLimit(k, n, TestedSolver(), params);
  if (!GetParam().parameter_support.supports_absolute_gap_tolerance) {
    EXPECT_THAT(gap_tolerance_result,
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr("absolute_gap_tolerance")));
    return;
  }
  EXPECT_EQ(gap_tolerance_result->termination.reason,
            TerminationReason::kOptimal);

  // This test is too brittle for CP-SAT, as we cannot get a bound that is just
  // the LP relaxation and nothing more. This test is already brittle and may
  // break on solver upgrades.
  if (TestedSolver() != SolverType::kCpSat) {
    EXPECT_GT(gap_tolerance_result->termination.objective_bounds.dual_bound,
              best_bound_differentiator);
  }
}

TEST_P(IpParameterTest, RelativeGapLimit) {
  if (GetParam().solver_type == SolverType::kHighs) {
    GTEST_SKIP()
        << "This test does not work for HiGHS, we cannot weaken the root node "
           "bound enough, see https://paste.googleplex.com/6416319233654784";
  }
  if (GetParam().solver_type == SolverType::kGlpk) {
    GTEST_SKIP()
        << "This test does not work for GLPK, not clear why this isn't "
           "working, logs here: https://paste.googleplex.com/6080056622317568";
  }
  const int k = 10;
  const int n = 2;
  const double lp_opt = k / 2.0 + 1;
  const double ip_opt = 1;
  const double abs_lp_gap = lp_opt - ip_opt;
  const double rel_lp_gap = (lp_opt - ip_opt) / ip_opt;
  // We will set a target gap that can be validated by lp_opt, but best_bound
  // may end up being slightly better for some solvers.
  const double best_bound_differentiator = lp_opt - abs_lp_gap / 2.0;

  // Check that best bound on default solve is close to ip_opt and hence
  // strictly smaller than best_bound_differentiator.
  ASSERT_OK_AND_ASSIGN(
      const SolveResult default_result,
      SolveForGapLimit(k, n, TestedSolver(), SolveParameters()));
  EXPECT_THAT(default_result, IsOptimal());
  EXPECT_LT(default_result.termination.objective_bounds.dual_bound,
            ip_opt + 0.1);
  EXPECT_LT(default_result.termination.objective_bounds.dual_bound,
            best_bound_differentiator);

  // Set target gap slightly larger than rel_lp_gap and check that best bound
  // is strictly larger than best_bound_differentiator.
  SolveParameters params = {.relative_gap_tolerance = rel_lp_gap + 0.1};
  // Weaken the solver so that we actually have a chance to run the root LP
  // and terminate early (if we solve the problem in presolve there will be no
  // gap).
  if (GetParam().parameter_support.supports_presolve) {
    params.presolve = Emphasis::kOff;
  }
  if (GetParam().parameter_support.supports_one_thread) {
    params.threads = 1;
  }
  if (GetParam().parameter_support.supports_cuts) {
    params.cuts = Emphasis::kOff;
  }
  ASSERT_OK_AND_ASSIGN(const SolveResult gap_tolerance_result,
                       SolveForGapLimit(k, n, TestedSolver(), params));
  EXPECT_EQ(gap_tolerance_result.termination.reason,
            TerminationReason::kOptimal);

  // This test is too brittle for CP-SAT, as we cannot get a bound that is just
  // the LP relaxation and nothing more. This test is already brittle and may
  // break on solver upgrades.
  if (TestedSolver() != SolverType::kCpSat) {
    EXPECT_GT(gap_tolerance_result.termination.objective_bounds.dual_bound,
              best_bound_differentiator);
  }
}

TEST_P(IpParameterTest, CutoffLimit) {
  Model model;
  const Variable x = model.AddBinaryVariable();
  model.Minimize(x);
  // When the optimal solution is worse than cutoff, no solution information is
  // returned and we return Limit::kCutoff.
  const absl::StatusOr<SolveResult> result = Solve(
      model, GetParam().solver_type, {.parameters = {.cutoff_limit = -1.0}});
  if (!GetParam().parameter_support.supports_cutoff) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("cutoff")));
    return;
  }
  EXPECT_THAT(result, IsOkAndHolds(
                          TerminatesWithReasonNoSolutionFound(Limit::kCutoff)));
  // When the optimal solution is better than cutoff, the parameter has no
  // effect on the returned SolveResult (at least for problems with a unique
  // solution, it may change the nodes visited still) and we return the optimal
  // solution.
  EXPECT_THAT(Solve(model, GetParam().solver_type,
                    {.parameters = {.cutoff_limit = 0.5}}),
              IsOkAndHolds(IsOptimal(0.0)));
}

TEST_P(IpParameterTest, ObjectiveLimit) {
  const std::unique_ptr<Model> model = DenseIndependentSet(/*integer=*/true);
  SolveParameters params = {.objective_limit = 3.5};
  // If we solve in presolve we don't get a chance to stop early with the
  // limit.
  if (GetParam().parameter_support.supports_presolve) {
    params.presolve = Emphasis::kOff;
  }
  {
    // The model has an optimal solution of 7 which is hard to find, and many
    // easy to find solutions with objective value 5. Solve with permission to
    // stop early once an easy solution is found, and verify that we terminate
    // from the objective limit.

    const absl::StatusOr<SolveResult> result =
        Solve(*model, GetParam().solver_type, {.parameters = params});
    if (!GetParam().parameter_support.supports_objective_limit) {
      EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                   HasSubstr("objective_limit")));
      return;
    }
    EXPECT_THAT(result, IsOkAndHolds(TerminatesWithLimit(Limit::kObjective)));
    ASSERT_TRUE(result->has_primal_feasible_solution());
    EXPECT_LE(result->objective_value(), 5.0 + 1.0e-5);
  }
  // Resolve the same model with objective limit 20. Since the true objective
  // is 7, we will just solve to optimality.
  params.objective_limit = 20.0;
  EXPECT_THAT(Solve(*model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(7)));
}

TEST_P(IpParameterTest, BestBoundLimit) {
  const std::unique_ptr<Model> model = DenseIndependentSet(/*integer=*/true);
  // The model has an LP relaxation of 60 and an optimal solution of 7.
  // Solve with permission to stop early, when the best bound is equal to 60.
  // Check the termination reason, and that the bound is indeed worse than
  // optimal.
  SolveParameters params = {.best_bound_limit = 60.0};
  // If we solve in presolve we don't get a chance to stop early with the
  // limit.
  if (GetParam().parameter_support.supports_presolve) {
    params.presolve = Emphasis::kOff;
  }
  {
    const absl::StatusOr<SolveResult> result =
        Solve(*model, GetParam().solver_type, {.parameters = params});
    if (!GetParam().parameter_support.supports_bound_limit) {
      EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                   HasSubstr("best_bound_limit")));
      return;
    }
    ASSERT_THAT(result, IsOkAndHolds(TerminatesWithLimit(Limit::kObjective)));
    EXPECT_LE(result->termination.objective_bounds.dual_bound, 60.0);
    EXPECT_GE(result->termination.objective_bounds.dual_bound, 8.0);
  }
  // Solve again but now with permission to stop only when the bound is 4 or
  // smaller. Since the optimal solution is 7, we will just solve to optimality.
  params.best_bound_limit = 4.0;
  EXPECT_THAT(Solve(*model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(7.0)));
}

TEST_P(IpParameterTest, SolutionLimitOneWithHint) {
  if (!GetParam().hint_supported) {
    GTEST_SKIP() << "Test requires a hint";
  }
  Model model;
  const Variable x = model.AddBinaryVariable();
  model.Minimize(x);
  ModelSolveParameters model_params;
  model_params.solution_hints.push_back({.variable_values = {{x, 1.0}}});
  SolveParameters params = {.solution_limit = 1};
  if (GetParam().parameter_support.supports_presolve) {
    params.presolve = Emphasis::kOff;
  }
  // SCIP fails to stop based on the hinted solution, runs the "trivial
  // heuristic" and finds a better solution, then returns limit feasible with
  // the wrong solution, unless heuristics are disabled.
  if (GetParam().parameter_support.supports_heuristics) {
    params.heuristics = Emphasis::kOff;
  }
  const absl::StatusOr<SolveResult> result =
      Solve(model, GetParam().solver_type,
            {.parameters = params, .model_parameters = model_params});
  if (!GetParam().parameter_support.supports_solution_limit_one) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("solution_limit")));
    return;
  }
  EXPECT_THAT(result, IsOkAndHolds(TerminatesWithReasonFeasible(
                          Limit::kSolution,
                          /*allow_limit_undetermined=*/!GetParam()
                              .solve_result_support.termination_limit)));
  EXPECT_EQ(result->solutions.size(), 1);
  EXPECT_THAT(*result, HasSolution(PrimalSolution{
                           .variable_values = {{x, 1.0}},
                           .objective_value = 1.0,
                           .feasibility_status = SolutionStatus::kFeasible}));
}

TEST_P(IpParameterTest, SolutionLimitOneAndCutoff) {
  if (!(GetParam().parameter_support.supports_cutoff &&
        GetParam().parameter_support.supports_solution_limit_one)) {
    // We have already tested when these parameters are unsupported.
    return;
  }
  if (!GetParam().hint_supported) {
    GTEST_SKIP() << "Test requires a hint";
  }
  Model model;
  const Variable x = model.AddBinaryVariable();
  const Variable y = model.AddBinaryVariable();
  const Variable z = model.AddBinaryVariable();
  model.Maximize(x + 2 * y + 3 * z);
  model.AddLinearConstraint(x + y + z <= 1);

  // Exclude (0, 0, 0) and (1, 0, 0) with cutoff = 1.5.
  // Hint (1, 0, 0) and (0, 1, 0).
  // Set a solution limit of 1. The first hint should be ignored, and the second
  // suboptimal hint should be returned.
  //
  // NOTE: CP-SAT only allows one hint (the first one suggested). We put hint
  // (0, 1, 0) first so the test still passes, but we are not testing as much.
  SolveParameters params = {.cutoff_limit = 1.5, .solution_limit = 1};
  if (GetParam().parameter_support.supports_presolve) {
    params.presolve = Emphasis::kOff;
  }
  // Not 100% clear why this is needed, but CP-SAT will sometimes return
  // a solution better than the hint without this.
  if (GetParam().parameter_support.supports_one_thread) {
    params.threads = 1;
  }
  ModelSolveParameters model_params;
  model_params.solution_hints.push_back(ModelSolveParameters::SolutionHint{
      .variable_values = {{x, 0.0}, {y, 1.0}, {z, 0.0}}});
  model_params.solution_hints.push_back(ModelSolveParameters::SolutionHint{
      .variable_values = {{x, 1.0}, {y, 0.0}, {z, 0.0}}});
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(model, GetParam().solver_type,
            {.parameters = params, .model_parameters = model_params}));
  EXPECT_THAT(result, TerminatesWithReasonFeasible(
                          Limit::kSolution,
                          /*allow_limit_undetermined=*/!GetParam()
                              .solve_result_support.termination_limit));
  ASSERT_TRUE(result.has_primal_feasible_solution());
  EXPECT_NEAR(result.objective_value(), 2.0, 1e-5);
  EXPECT_EQ(result.solutions.size(), 1);
}

// Tests the interaction between cutoff and an additional limit.
TEST_P(IpParameterTest, NoSolutionsBelowCutoffEarlyTermination) {
  if (GetParam().solver_type == SolverType::kGscip) {
    GTEST_SKIP() << "This test does not work with SCIP v900";
  }
  if (!(GetParam().parameter_support.supports_cutoff)) {
    // We have already tested that the right error message is returned.
    return;
  }
  if (!GetParam().hint_supported) {
    GTEST_SKIP() << "Test requires a hint";
  }
  const std::unique_ptr<const Model> model = DenseIndependentSet(true);
  ModelSolveParameters model_params;
  model_params.solution_hints.push_back(DenseIndependentSetHint5(*model));
  SolveParameters params = GetParam().stop_before_optimal;
  params.cutoff_limit = 6.5;
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(*model, GetParam().solver_type,
            {.parameters = params, .model_parameters = model_params}));
  // There is a solution with objective 7, but it is hard to find.
  // NOTE: if this becomes flaky, we can increase to cutoff to 7.5.
  EXPECT_THAT(result, TerminatesWith(TerminationReason::kNoSolutionFound));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// LargeInstanceIpParameterTest
////////////////////////////////////////////////////////////////////////////////

namespace {

absl::StatusOr<std::unique_ptr<Model>> LoadMiplibInstance(
    absl::string_view name) {
  ASSIGN_OR_RETURN(
      const ModelProto model_proto,
      ReadMpsFile(absl::StrCat("ortools/math_opt/solver_tests/testdata/", name,
                               ".mps")));
  return Model::FromModelProto(model_proto);
}

};  // namespace

absl::StatusOr<std::unique_ptr<Model>>
LargeInstanceIpParameterTest::Load23588() {
  return LoadMiplibInstance("23588");
}

absl::StatusOr<std::unique_ptr<Model>>
LargeInstanceIpParameterTest::LoadBeavma() {
  return LoadMiplibInstance("beavma");
}

namespace {

TEST_P(LargeInstanceIpParameterTest, SolvesInstanceNoLimits) {
  if (DEBUG_MODE || kAnyXsanEnabled) {
    GTEST_SKIP() << "Test skipped, too slow unless compiled with -c opt.";
  }
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model, Load23588());
  const SolveParameters params = GetParam().base_parameters;
  EXPECT_THAT(Solve(*model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(kOptimalObjective)));
}

TEST_P(LargeInstanceIpParameterTest, TimeLimit) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model, Load23588());
  SolveParameters params = GetParam().base_parameters;
  params.time_limit = absl::Milliseconds(1);
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(*model, GetParam().solver_type, {.parameters = params}));
  EXPECT_THAT(result, TerminatesWithLimit(Limit::kTime,
                                          GetParam().allow_limit_undetermined));
  // Solvers do not stop very precisely, use a large number to avoid flaky
  // tests. Do NOT try to fine tune this to be small, it is hard to get right
  // for all compilation modes (e.g. debug, asan).
  EXPECT_LE(result.solve_stats.solve_time, absl::Seconds(1));
}

TEST_P(LargeInstanceIpParameterTest, IterationLimit) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model, Load23588());
  SolveParameters params = GetParam().base_parameters;
  params.iteration_limit = 1;
  const absl::StatusOr<SolveResult> result =
      Solve(*model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().parameter_support.supports_iteration_limit) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("iteration_limit")));
    return;
  }
  ASSERT_THAT(result,
              IsOkAndHolds(TerminatesWithLimit(
                  Limit::kIteration, GetParam().allow_limit_undetermined)));
  EXPECT_LE(result->solve_stats.simplex_iterations, 1);
  EXPECT_LE(result->solve_stats.barrier_iterations, 1);
}

TEST_P(LargeInstanceIpParameterTest, NodeLimit) {
  if (GetParam().solver_type == SolverType::kHighs) {
    GTEST_SKIP() << "Ignoring this test as Highs 1.7+ returns unimplemented";
  }
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model, Load23588());
  SolveParameters params = GetParam().base_parameters;
  params.node_limit = 1;
  const absl::StatusOr<SolveResult> result =
      Solve(*model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().parameter_support.supports_node_limit) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("node_limit")));
    return;
  }
  ASSERT_THAT(result, IsOkAndHolds(TerminatesWithLimit(
                          Limit::kNode, GetParam().allow_limit_undetermined)));
  EXPECT_LE(result->solve_stats.node_count, 1);
}

TEST_P(LargeInstanceIpParameterTest, CutoffLimit) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model, Load23588());
  SolveParameters params = GetParam().base_parameters;
  // 23588.mps is minimization, set the cutoff below the optimal solution so
  // that no solutions are found.
  params.cutoff_limit = kOptimalObjective - 10.0;
  const absl::StatusOr<SolveResult> result =
      Solve(*model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().parameter_support.supports_cutoff) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("cutoff_limit")));
    return;
  }
  ASSERT_THAT(result,
              IsOkAndHolds(TerminatesWithLimit(
                  Limit::kCutoff, GetParam().allow_limit_undetermined)));
  // All solutions are worse than the cutoff value
  EXPECT_FALSE(result->has_primal_feasible_solution());

  // Solve again with a cutoff above the optimal solution, make sure we get the
  // optimal solution back.
  //
  // This requires a full solve, which is slow in debug/asan.
  if (DEBUG_MODE || kAnyXsanEnabled) {
    return;
  }
}

TEST_P(LargeInstanceIpParameterTest, ObjectiveLimit) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model, Load23588());
  SolveParameters params = GetParam().base_parameters;
  // 23588.mps is minimization, set the objective limit above the optimal
  // solution so we terminate early.
  params.objective_limit = 1.5 * kOptimalObjective;
  const absl::StatusOr<SolveResult> result =
      Solve(*model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().parameter_support.supports_objective_limit) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("objective_limit")));
    return;
  }

  // This assertion is a bit fragile, the solver could prove optimality.
  ASSERT_THAT(result,
              IsOkAndHolds(TerminatesWithReasonFeasible(
                  Limit::kObjective, GetParam().allow_limit_undetermined)));
  // The objective value should be in the interval:
  //   [kOptimalObjective, params.objective_limit].
  EXPECT_LE(result->objective_value(), params.objective_limit);
  // This assertion is fragile, the solver could find an optimal solution, but
  // we want to ensure that the objective limit is actually making us stop
  // early.
  EXPECT_GE(result->objective_value(), kOptimalObjective + 1.0);

  // Solve again with an objective limit below the optimal solution, make sure
  // we get the optimal solution back.
  //
  // This requires a full solve, which is slow in debug/asan.
  if (DEBUG_MODE || kAnyXsanEnabled) {
    return;
  }
  params.objective_limit = kOptimalObjective - 10.0;
  EXPECT_THAT(Solve(*model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(kOptimalObjective)));
}

TEST_P(LargeInstanceIpParameterTest, BestBoundLimit) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model, Load23588());
  SolveParameters params = GetParam().base_parameters;
  params.best_bound_limit = kLpRelaxationObjective - 1.0;
  const absl::StatusOr<SolveResult> result =
      Solve(*model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().parameter_support.supports_bound_limit) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("best_bound_limit")));
    return;
  }
  // This assertion is a bit fragile, the solver could prove optimality.
  ASSERT_THAT(result,
              IsOkAndHolds(TerminatesWithLimit(
                  Limit::kObjective, GetParam().allow_limit_undetermined)));
  // Since we should get a bound at least as strong as the LP relaxation at
  // the root node
  EXPECT_LE(result->solve_stats.node_count, 1);
  // The objective value should be in the interval:
  //   [params.best_bound_limit, kOptimalObjective].
  EXPECT_GE(result->best_objective_bound(), params.best_bound_limit);
  // This assertion is fragile, the solver could prove optimality, but
  // we want to ensure that the bound limit is actually making us stop early.
  EXPECT_GE(result->termination.objective_bounds.primal_bound,
            kOptimalObjective - 1.0);

  // Solve again with a bound limit above the optimal solution, make sure we
  // get the optimal solution back.
  //
  // This requires a full solve, which is slow in debug/asan.
  if (DEBUG_MODE || kAnyXsanEnabled) {
    return;
  }
  params.best_bound_limit = kOptimalObjective + 10.0;
  EXPECT_THAT(Solve(*model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(kOptimalObjective)));
}

TEST_P(LargeInstanceIpParameterTest, SolutionLimit) {
  if (GetParam().solver_type == SolverType::kHighs) {
    GTEST_SKIP() << "Ignoring this test as Highs 1.7+ returns unimplemented";
  }
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model, Load23588());
  SolveParameters params = GetParam().base_parameters;
  params.solution_limit = 1;
  const absl::StatusOr<SolveResult> result =
      Solve(*model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().parameter_support.supports_solution_limit_one) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("solution_limit")));
    return;
  }
  // This assertion is a bit fragile, the solver could prove optimality.
  ASSERT_THAT(result,
              IsOkAndHolds(TerminatesWithReasonFeasible(
                  Limit::kSolution, GetParam().allow_limit_undetermined)));
  // This test is a bit fragile, but typically we cannot prove optimality at
  // the time of first feasible solution (note that CP-SATs first primal
  // solution is optimal roughly 1/100 solves).
  EXPECT_GE(result->objective_value() - result->best_objective_bound(), 1.0);
}

// Set the absolute gap to the difference between the optimal objective
// and the root LP (~441), and check that there is at least a gap of ~10
// between the objective and best bound at termination.
//
// The root LP should bring us within the gap. Do NOT assert that there
// is at most one node as:
//  * There may be multiple nodes due to restarts.
//  * There is no guarantee (Without hints) that we find a good primal
//    solution at the root.
TEST_P(LargeInstanceIpParameterTest, AbsoluteGapTolerance) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model, Load23588());
  SolveParameters params = GetParam().base_parameters;
  const double absolute_lp_relax_gap =
      kOptimalObjective - kLpRelaxationObjective;
  params.absolute_gap_tolerance = absolute_lp_relax_gap;
  const absl::StatusOr<SolveResult> result =
      Solve(*model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().parameter_support.supports_absolute_gap_tolerance) {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("absolute_gap_tolerance")));
    return;
  }
  ASSERT_THAT(result, IsOkAndHolds(IsOptimal()));
  // There should be some space between our optimal solution and best bound
  if (GetParam().solver_type != SolverType::kCpSat) {
    // CP-SAT in parallel can find the optimal solution directly.
    EXPECT_GE(result->termination.objective_bounds.primal_bound -
                  result->termination.objective_bounds.dual_bound,
              absolute_lp_relax_gap / 40.0);
  }
}

// Set the relative gap to 2*(8090 - 7649)/8090 ~= 0.1 and check there is
// a gap of at least ~10 between the objective and best bound at termination.
//
// The root LP should bring us within the gap, but not assert on the node count,
// see above.
TEST_P(LargeInstanceIpParameterTest, RelativeGapTolerance) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model, Load23588());
  SolveParameters params = GetParam().base_parameters;
  const double absolute_lp_relax_gap =
      kOptimalObjective - kLpRelaxationObjective;
  params.relative_gap_tolerance = 2 * absolute_lp_relax_gap / kOptimalObjective;
  const absl::StatusOr<SolveResult> result =
      Solve(*model, GetParam().solver_type, {.parameters = params});

  ASSERT_THAT(result, IsOkAndHolds(IsOptimal()));
  // The root LP should bring us within the gap. Do NOT assert that there
  // is at most one node, there may be multiple due to restarts.
  // There should be some space between our optimal solution and best bound
  EXPECT_GE(result->termination.objective_bounds.primal_bound -
                result->termination.objective_bounds.dual_bound,
            absolute_lp_relax_gap / 40.0);
}

TEST_P(LargeInstanceIpParameterTest, Cuts) {
  if (!GetParam().parameter_support.supports_node_limit) {
    GTEST_SKIP() << "Skipping test, requires node_limit but is not supported.";
  }
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model, LoadBeavma());
  SolveParameters params = GetParam().base_parameters;
  // Run only the root node so we can compare the bound quality with and without
  // cuts on by checking the best bound on the SolveResult.
  params.node_limit = 1;
  params.cuts = Emphasis::kOff;
  const absl::StatusOr<SolveResult> result_cuts_off =
      Solve(*model, GetParam().solver_type, {.parameters = params});
  if (!GetParam().parameter_support.supports_cuts) {
    EXPECT_THAT(result_cuts_off, StatusIs(absl::StatusCode::kInvalidArgument,
                                          HasSubstr("cuts")));
    return;
  }
  ASSERT_OK(result_cuts_off);
  const double bound_cuts_off = result_cuts_off->best_objective_bound();

  params.cuts = Emphasis::kMedium;
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result_cuts_on,
      Solve(*model, GetParam().solver_type, {.parameters = params}));
  const double bound_cuts_on = result_cuts_on.best_objective_bound();

  // Problem is minimization, so a larger bound is better. Using cuts should
  // improve the bound.
  EXPECT_GE(bound_cuts_on, bound_cuts_off + 1.0);
}

}  // namespace

}  // namespace math_opt
}  // namespace operations_research
