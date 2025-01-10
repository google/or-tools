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

// TODO(user): These tests are incomplete in a few ways; see mip_tests.cc
// TODO(user): Expand tests so they check primal, dual and/or primal-dual
// infeasible cases as appropriate.
#include "ortools/math_opt/solver_tests/lp_tests.h"

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {

using ::testing::AnyOf;
using ::testing::status::IsOkAndHolds;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kTolerance = 1.0e-5;

struct SolvedModel {
  std::unique_ptr<Model> model;
  SolveResult expected_result;
};

std::ostream& operator<<(std::ostream& out,
                         const SimpleLpTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << ", parameters: " << ProtobufShortDebugString(params.parameters.Proto())
      << ", supports_duals: " << params.supports_duals
      << ", supports_basis: " << params.supports_basis
      << ", ensures_primal_ray: " << params.ensures_primal_ray
      << ", ensures_dual_ray: " << params.ensures_dual_ray
      << " disallows_infeasible_or_unbounded: "
      << params.disallows_infeasible_or_unbounded << " }";
  return out;
}

//   max 0.1 + sum_{i=1}^3 (3.0 *x_i + 2.0 * y_i)
//   s.t. x_i + y_i <= 1.5 for all i \in {1,2,3} (c_i)
//       0 <= x_i <= 1
//       0 <= y_i <= 1 for all i \in {1,2,3}
//
// Optimal solution is (x_i,y_i)=(1.0, 0.5) for all i \in {0,1,2}, with
// objective value 3*4+0.1 = 12.1.
IncrementalLpTest::IncrementalLpTest()
    : model_("incremental_solve_test"),
      zero_(model_.AddContinuousVariable(0, 0, "zero")),
      x_1_(model_.AddContinuousVariable(0, 1, "x_1")),
      y_1_(model_.AddContinuousVariable(0, 1, "y_1")),
      c_1_(model_.AddLinearConstraint(x_1_ + y_1_ <= 1.5, "c_1")),
      x_2_(model_.AddContinuousVariable(0, 1, "x_2")),
      y_2_(model_.AddContinuousVariable(0, 1, "y_2")),
      c_2_(model_.AddLinearConstraint(x_2_ + y_2_ <= 1.5, "c_2")),
      x_3_(model_.AddContinuousVariable(0, 1, "x_3")),
      y_3_(model_.AddContinuousVariable(0, 1, "y_3")),
      c_3_(model_.AddLinearConstraint(x_3_ + y_3_ <= 1.5, "c_3")) {
  model_.Maximize(0.1 + 3 * (x_1_ + x_2_ + x_3_) + 2 * (y_1_ + y_2_ + y_3_));
  solver_ = NewIncrementalSolver(&model_, TestedSolver()).value();
  const SolveResult first_solve = solver_->Solve().value();
  CHECK_OK(first_solve.termination.EnsureIsOptimal());
  CHECK_LE(std::abs(first_solve.objective_value() - 12.1), kTolerance);
}

namespace {

TEST_P(SimpleLpTest, ProtoNonIncrementalSolve) {
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(2 * x);
  const ModelProto proto = model.ExportModel();
  ASSERT_OK_AND_ASSIGN(
      const SolveResultProto result,
      Solver::NonIncrementalSolve(
          proto, EnumToProto(TestedSolver()),
          /*init_args=*/{},
          /*solve_args=*/{.parameters = GetParam().parameters.Proto()}));
  ASSERT_EQ(result.termination().reason(), TERMINATION_REASON_OPTIMAL)
      << ProtobufDebugString(result.termination());
  ASSERT_GE(result.solutions_size(), 1);
  ASSERT_TRUE(result.solutions(0).has_primal_solution());
  EXPECT_NEAR(result.solutions(0).primal_solution().objective_value(), 2.0,
              kTolerance);
  EXPECT_EQ(result.solutions(0).primal_solution().feasibility_status(),
            SOLUTION_STATUS_FEASIBLE);
  if (GetParam().supports_duals) {
    ASSERT_TRUE(result.solutions(0).has_dual_solution());
    ASSERT_TRUE(result.solutions(0).dual_solution().has_objective_value());
    EXPECT_NEAR(result.solutions(0).dual_solution().objective_value(), 2.0,
                kTolerance);
    EXPECT_EQ(result.solutions(0).dual_solution().feasibility_status(),
              SOLUTION_STATUS_FEASIBLE);
  }
}

// TODO(b/184447031): change descriptions to avoid d(y, r)/d_max(y,r) and
// go/mathopt-doc-math#dual

// Primal:
//   max 2.0*x
//   s.t.
//       0 <= x <= 4.0
//
// Dual (go/mathopt-doc-math#dual):
//   min d(y, r)
//         r == 2.0
//
// Unique optimal primal solution is x* = 4.0.
// Complementary slackness conditions for x*
// (go/mathopt-dual#primal-dual-optimal-pairs) imply:
//
// r == 2.0,
//
// which has the unique solution r* = 2.0.
TEST_P(SimpleLpTest, OneVarMax) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 4.0, "x");
  model.Maximize(2 * x);
  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(model));

  EXPECT_THAT(result, IsOptimalWithSolution(8.0, {{x, 4.0}}, kTolerance));
  if (GetParam().supports_duals) {
    EXPECT_THAT(result,
                IsOptimalWithDualSolution(8.0, {}, {{x, 2.0}}, kTolerance));
  }
}

// Primal:
//   min 2.0*x
//   s.t.
//       -2.4 <= x <= 4.0
//
// Dual (go/mathopt-doc-math#dual):
//   max d_max(y, r)
//         r == 2.0
//
// Unique optimal primal solution is x* = -2.4.
// Complementary slackness conditions for x*
// (go/mathopt-dual#primal-dual-optimal-pairs) imply:
//
// r == 2.0,
//
// which has the unique solution r* = 2.0.
TEST_P(SimpleLpTest, OneVarMin) {
  Model model;
  const Variable x = model.AddContinuousVariable(-2.4, 4.0, "x");
  model.Minimize(2 * x);
  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(model));
  EXPECT_THAT(result, IsOptimalWithSolution(-4.8, {{x, -2.4}}));
  if (GetParam().supports_duals) {
    EXPECT_THAT(result, IsOptimalWithDualSolution(-4.8, {}, {{x, 2.0}}));
  }
}

// For any parameter p in [-kInf, 0]
// Primal:
//   max 2.0*x_1 + 1.0*x_2
//   s.t. p <= x_1 + x_2 <= 1.5  (y)
//        0.0 <= x_1 <= 1.0
//        0.0 <= x_2 <= 1.0
//
// Dual (go/mathopt-doc-math#dual):
//   min d(y, r)
//        y + r_1 == 2.0
//        y + r_2 == 1.0
//
// Unique optimal primal solution is (x*_1, x*_2) = (1.0. 0.5).
// Complementary slackness conditions for x*
// (go/mathopt-dual#primal-dual-optimal-pairs) imply:
//
//        y + r_1 == 2.0
//        y + r_2 == 1.0
//            r_2 == 0.0
//
// which has the unique solution (y*, r*_1, r*_2) = (1.0, 1.0, 0.0).
SolvedModel SimpleLinearConstraint(double p) {
  auto model = std::make_unique<Model>();
  const Variable x_1 = model->AddVariable(0.0, 1.0, false, "x_1");
  const Variable x_2 = model->AddVariable(0.0, 1.0, false, "x_2");
  model->Maximize(2 * x_1 + x_2);
  const LinearConstraint y =
      model->AddLinearConstraint(p <= x_1 + x_2 <= 1.5, "y");
  SolveResult result{Termination::Optimal(2.5)};
  result.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x_1, 1.0}, {x_2, 0.5}},
                         .objective_value = 2.5,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{y, 1.0}},
                       .reduced_costs = {{x_1, 1.0}, {x_2, 0.0}},
                       .objective_value = 2.5,
                       .feasibility_status = SolutionStatus::kFeasible}});
  return {.model = std::move(model), .expected_result = result};
}

TEST_P(SimpleLpTest, SimpleLinearConstraintRanged) {
  const SolvedModel solved_model = SimpleLinearConstraint(0.0);
  EXPECT_THAT(SimpleSolve(*solved_model.model),
              IsOkAndHolds(
                  IsConsistentWith(solved_model.expected_result,
                                   {.check_dual = GetParam().supports_duals})));
}

TEST_P(SimpleLpTest, SimpleLinearConstraintNonRanged) {
  const SolvedModel solved_model = SimpleLinearConstraint(-kInf);
  EXPECT_THAT(SimpleSolve(*solved_model.model),
              IsOkAndHolds(
                  IsConsistentWith(solved_model.expected_result,
                                   {.check_dual = GetParam().supports_duals})));
}

// First extra check for possible sign issues with duals
// For any parameter p in [1.5, kInf]
// Primal:
//   min 2.0*x_1 + 1.0*x_2
//   s.t. 0.5 <= x_1 + x_2 <= p  (y)
//        0.0 <= x_1 <= 1.0
//        0.0 <= x_2 <= 1.0
//
// Dual (go/mathopt-doc-math#dual):
//   max d_max(y, r)
//        y + r_1 == 2.0
//        y + r_2 == 1.0
//
// Unique optimal primal solution is (x*_1, x*_2) = (0.0. 0.5).
// Complementary slackness conditions for x*
// (go/mathopt-dual#primal-dual-optimal-pairs) imply:
//
//        y + r_1 == 2.0
//        y + r_2 == 1.0
//            r_2 == 0.0
//
// which has the unique solution (y*, r*_1, r*_2) = (1.0, 1.0, 0.0).
SolvedModel SimpleLinearConstraintDualMin(double p) {
  auto model = std::make_unique<Model>();
  const Variable x_1 = model->AddVariable(0.0, 1.0, false, "x_1");
  const Variable x_2 = model->AddVariable(0.0, 1.0, false, "x_2");
  model->Minimize(2 * x_1 + x_2);
  const LinearConstraint y =
      model->AddLinearConstraint(0.5 <= x_1 + x_2 <= p, "y");
  SolveResult result{Termination::Optimal(0.5)};
  result.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x_1, 0.0}, {x_2, 0.5}},
                         .objective_value = 0.5,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{y, 1.0}},
                       .reduced_costs = {{x_1, 1.0}, {x_2, 0.0}},
                       .objective_value = 0.5,
                       .feasibility_status = SolutionStatus::kFeasible}});
  return {std::move(model), result};
}

TEST_P(SimpleLpTest, SimpleLinearConstraintDualMinRanged) {
  const SolvedModel solved_model = SimpleLinearConstraintDualMin(1.5);
  EXPECT_THAT(SimpleSolve(*solved_model.model),
              IsOkAndHolds(
                  IsConsistentWith(solved_model.expected_result,
                                   {.check_dual = GetParam().supports_duals})));
}

TEST_P(SimpleLpTest, SimpleLinearConstraintDualMinNonRanged) {
  const SolvedModel solved_model = SimpleLinearConstraintDualMin(kInf);
  EXPECT_THAT(SimpleSolve(*solved_model.model),
              IsOkAndHolds(
                  IsConsistentWith(solved_model.expected_result,
                                   {.check_dual = GetParam().supports_duals})));
}

// Second extra checks for possible sign issues with duals
// For any parameter p in [1.5, kInf]
// Primal:
//   max -2.0*x_1 - 1.0*x_2
//   s.t. 0.5 <= x_1 + x_2 <= p  (y)
//        0.0 <= x_1 <= 1.0
//        0.0 <= x_2 <= 1.0
//
// Dual (go/mathopt-doc-math#dual):
//   min d(y, r)
//        y + r_1 == -2.0
//        y + r_2 == -1.0
//
// Unique optimal primal solution is (x*_1, x*_2) = (0.0. 0.5).
// Complementary slackness conditions for x*
// (go/mathopt-dual#primal-dual-optimal-pairs) imply:
//
//        y + r_1 == -2.0
//        y + r_2 == -1.0
//            r_2 == 0.0
//
// which has the unique solution (y*, r*_1, r*_2) = (-1.0, -1.0, 0.0).
SolvedModel SimpleLinearConstraintDualLowerBounds(double p) {
  auto model = std::make_unique<Model>();
  const Variable x_1 = model->AddVariable(0.0, 1.0, false, "x_1");
  const Variable x_2 = model->AddVariable(0.0, 1.0, false, "x_2");
  model->Maximize(-2 * x_1 - x_2);
  const LinearConstraint y =
      model->AddLinearConstraint(0.5 <= x_1 + x_2 <= p, "y");
  SolveResult result{Termination::Optimal(-0.5)};
  result.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x_1, 0.0}, {x_2, 0.5}},
                         .objective_value = -0.5,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{y, -1.0}},
                       .reduced_costs = {{x_1, -1.0}, {x_2, 0.0}},
                       .objective_value = -0.5,
                       .feasibility_status = SolutionStatus::kFeasible}});
  return {std::move(model), result};
}

TEST_P(SimpleLpTest, SimpleLinearConstraintDualLowerBoundsRanged) {
  const SolvedModel solved_model = SimpleLinearConstraintDualLowerBounds(1.5);
  EXPECT_THAT(SimpleSolve(*solved_model.model),
              IsOkAndHolds(
                  IsConsistentWith(solved_model.expected_result,
                                   {.check_dual = GetParam().supports_duals})));
}

TEST_P(SimpleLpTest, SimpleLinearConstraintDualLowerBoundsNonRanged) {
  const SolvedModel solved_model = SimpleLinearConstraintDualLowerBounds(kInf);
  EXPECT_THAT(SimpleSolve(*solved_model.model),
              IsOkAndHolds(
                  IsConsistentWith(solved_model.expected_result,
                                   {.check_dual = GetParam().supports_duals})));
}

// Primal:
//   max 2.0*x_1 + 1.0*x_2
//   s.t. -1.0 <= x_1 - x_2 <= 1.0 (y)
//        x_1 >= 0
//        x_2 >= 0
//
// Problem is unbounded: the only ray (up to scaling) is (x_1, x_2) = (1.0, 1.0)
// If ranged = false, separate (y) into two single-sided inequalities.
SolvedModel SimpleUnboundedLP(bool ranged) {
  auto model = std::make_unique<Model>();
  const Variable x_1 = model->AddContinuousVariable(0.0, kInf, "x_1");
  const Variable x_2 = model->AddContinuousVariable(0.0, kInf, "x_2");
  model->Maximize(2 * x_1 + x_2);
  if (ranged) {
    model->AddLinearConstraint(-1 <= x_1 - x_2 <= 1, "y");
  } else {
    model->AddLinearConstraint(-1 <= x_1 - x_2, "y_1");
    model->AddLinearConstraint(x_1 - x_2 <= 1, "y_2");
  }
  SolveResult result{Termination::Unbounded(/*is_maximize=*/true)};
  result.primal_rays.emplace_back().variable_values = {{x_1, 1.0}, {x_2, 1.0}};
  return {std::move(model), result};
}

SolveResultMatcherOptions PrimalRayMatchOptions(
    const SimpleLpTestParameters& test_params, const SolveResult& actual) {
  SolveResultMatcherOptions matcher_options;
  matcher_options.inf_or_unb_soft_match =
      !test_params.disallows_infeasible_or_unbounded;
  matcher_options.check_rays =
      test_params.ensures_primal_ray || actual.has_ray();
  return matcher_options;
}

TEST_P(SimpleLpTest, SimpleRangedRay) {
  const SolvedModel solved_model = SimpleUnboundedLP(true);
  ASSERT_OK_AND_ASSIGN(const SolveResult actual,
                       SimpleSolve(*solved_model.model));
  EXPECT_THAT(actual,
              IsConsistentWith(solved_model.expected_result,
                               PrimalRayMatchOptions(GetParam(), actual)));
}

TEST_P(SimpleLpTest, SimpleNonRangedRay) {
  const SolvedModel solved_model = SimpleUnboundedLP(false);
  ASSERT_OK_AND_ASSIGN(const SolveResult actual,
                       SimpleSolve(*solved_model.model));
  EXPECT_THAT(actual,
              IsConsistentWith(solved_model.expected_result,
                               PrimalRayMatchOptions(GetParam(), actual)));
}

// TODO(b/183600770): add simple version of these tests.
// For any parameter p in [-kInf, -1.0]
// Primal:
//   min 2.0*x_1 + 1.0*x_2
//   s.t. p <= x_1 + x_2 <= -1.0 (y)
//         0.0 <= x_1 <= 3.0
//         0.0 <= x_2 <= 3.0
//
// Dual (go/mathopt-doc-math#dual):
//   max d(y, r)
//        y + r_1 == 2.0
//        y + r_2 == 1.0
//
// The primal is infeasible and the dual is unbounded.
//
// Dual ray / primal infeasibility certificate must satisfy
// (go/mathopt-solutions#primal-inf-cert):
//
//                                          y + r_1 == 0.0
//                                          y + r_2 == 0.0
//                 {p*y : y > 0} + {-1.0*y : y < 0}
//      + {3.0*r_1 : r_1 < 0} + {3.0*r_2 : r_2 < 0}  > 0
//
// Because p <= -1.0, the only solution (up to scaling) is
// (y, r_1, r_2) = (-1.0, 1.0, 1.0).
SolvedModel SimpleInfeasibleLPMin(double p) {
  auto model = std::make_unique<Model>();
  const Variable x_1 = model->AddContinuousVariable(0, 3, "x_1");
  const Variable x_2 = model->AddContinuousVariable(0, 3, "x_2");
  model->Minimize(2 * x_1 + x_2);
  const LinearConstraint y =
      model->AddLinearConstraint(p <= x_1 + x_2 <= -1, "y");
  SolveResult result{Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible)};
  result.dual_rays.push_back(
      {.dual_values = {{y, -1.0}}, .reduced_costs = {{x_1, 1.0}, {x_2, 1.0}}});
  return {std::move(model), result};
}

SolveResultMatcherOptions DualUnboundedMatchOptions(
    const SimpleLpTestParameters& test_params, const SolveResult& actual) {
  SolveResultMatcherOptions matcher_options;
  // NOTE: this assumes that primal is infeasible and the dual is unbounded, see
  // inf_or_unb_soft_match documentation for details.
  matcher_options.inf_or_unb_soft_match = false;
  // TODO(b/211045017): remove this hardcoded edge case
  if (test_params.solver_type == SolverType::kGlpk &&
      test_params.parameters.lp_algorithm == LPAlgorithm::kBarrier) {
    matcher_options.inf_or_unb_soft_match = true;
  }
  matcher_options.check_rays =
      test_params.ensures_dual_ray || actual.has_dual_ray();
  return matcher_options;
}

TEST_P(SimpleLpTest, SimpleRangedInfeasibleMin) {
  const SolvedModel solved_model = SimpleInfeasibleLPMin(-2.0);
  ASSERT_OK_AND_ASSIGN(const SolveResult actual,
                       SimpleSolve(*solved_model.model));
  EXPECT_THAT(actual,
              IsConsistentWith(solved_model.expected_result,
                               DualUnboundedMatchOptions(GetParam(), actual)));
}

TEST_P(SimpleLpTest, SimpleNonRangedInfeasibleMin) {
  const SolvedModel solved_model = SimpleInfeasibleLPMin(-kInf);
  ASSERT_OK_AND_ASSIGN(const SolveResult actual,
                       SimpleSolve(*solved_model.model));
  EXPECT_THAT(actual,
              IsConsistentWith(solved_model.expected_result,
                               DualUnboundedMatchOptions(GetParam(), actual)));
}

// For any parameter p in [-kInf, -1.0]
// Primal:
//   max 2.0*x_1 + 1.0*x_2
//   s.t. p <= x_1 + x_2 <= -1.0 (y)
//         0.0 <= x_1 <= 3.0
//         0.0 <= x_2 <= 3.0
//
// Dual (go/mathopt-doc-math#dual):
//   min d_max(y, r)
//        y + r_1 == 2.0
//        y + r_2 == 1.0
//
// Problem is primal infeasible and dual unbounded.
//
// Dual ray / primal infeasibility certificate must satisfy
// (go/mathopt-solutions#primal-inf-cert):
//
//                                           y + r_1 == 0.0
//                                           y + r_2 == 0.0
//                  {-1.0*y : y > 0} + {p*y : y < 0}
//       + {3.0*r_1 : r_1 > 0} + {3.0*r_2 : r_2 > 0}  < 0
//
// Because p <= -1.0, the only solution (up to scaling) is
// (y, r_1, r_2) = (1.0, -1.0, -1.0).
SolvedModel SimpleInfeasibleLPMax(double p) {
  auto model = std::make_unique<Model>();
  const Variable x_1 = model->AddContinuousVariable(0, 3, "x_1");
  const Variable x_2 = model->AddContinuousVariable(0, 3, "x_2");
  model->Maximize(2 * x_1 + x_2);
  const LinearConstraint y =
      model->AddLinearConstraint(p <= x_1 + x_2 <= -1, "y");
  SolveResult result{Termination::Infeasible(
      /*is_maximize=*/true,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible)};
  result.dual_rays.push_back(
      {.dual_values = {{y, 1.0}}, .reduced_costs = {{x_1, -1.0}, {x_2, -1.0}}});
  return {std::move(model), result};
}

TEST_P(SimpleLpTest, SimpleRangedInfeasibleMax) {
  const SolvedModel solved_model = SimpleInfeasibleLPMax(-2.0);
  ASSERT_OK_AND_ASSIGN(const SolveResult actual,
                       SimpleSolve(*solved_model.model));
  EXPECT_THAT(actual,
              IsConsistentWith(solved_model.expected_result,
                               DualUnboundedMatchOptions(GetParam(), actual)));
}

TEST_P(SimpleLpTest, SimpleNonRangedInfeasibleMax) {
  const SolvedModel solved_model = SimpleInfeasibleLPMax(-kInf);
  ASSERT_OK_AND_ASSIGN(const SolveResult actual,
                       SimpleSolve(*solved_model.model));
  EXPECT_THAT(actual,
              IsConsistentWith(solved_model.expected_result,
                               DualUnboundedMatchOptions(GetParam(), actual)));
}

// For p in [2.0, kInf]
// Primal:
//   max  x_2
//   s.t. - p  <= x_1 + x_2 <= 2.0 (y_1)
//        -2.0 <= x_1 - x_2 <= p   (y_2)
//        -1.0 <= x_1 <= 1.0
//         0.0 <= x_2 <= kInf
//
// Dual (go/mathopt-doc-math#dual):
//   min d_max(y, r)
//        y_1 + y_2 + r_1 == 0.0
//        y_1 - y_2 + r_2 == 1.0
//
// Unique optimal primal solution is (x*_1, x*_2) = (0.0. 2.0).
// Complementary slackness conditions for x*
// (go/mathopt-dual#primal-dual-optimal-pairs) imply (note that we have
// a maximization problem so the inequalities in the TIP environment hold):
//
//        y_1 + y_2 + r_1 == 0.0
//        y_1 - y_2 + r_2 == 1.0
//                    r_1 == 0.0
//                    r_2 == 0.0
//                    y_1 >= 0.0
//                    y_2 <= 0.0
//
// which has the unique solution
// (y*_1, y*_2, r*_1, r*_2) = (0.5, -0.5, 0.0, 0.0).
//
// From go/mathopt-basis#primal we have C = {1, 2}, V = {},
//
// s^c_1 = AT_UPPER_BOUND
// s^c_2 = AT_LOWER_BOUND
// s^v_1 = BASIC
// s^v_2 = BASIC
//
// We can check that these statuses are compatible with the dual feasibility
// conditions in go/mathopt-basis#dual (note again that we have
// a maximization problem so the inequalities in the IMPORTANT environment
// hold).
SolvedModel ConstraintDefinedBasisLP(const double p) {
  auto model = std::make_unique<Model>();
  const Variable x_1 = model->AddContinuousVariable(-1, 1, "x_1");
  const Variable x_2 = model->AddContinuousVariable(0, kInf, "x_2");
  model->Maximize(x_2);
  const LinearConstraint y_1 =
      model->AddLinearConstraint(-p <= x_1 + x_2 <= 2, "y_1");
  const LinearConstraint y_2 =
      model->AddLinearConstraint(-2 <= x_1 - x_2 <= p, "y_2");
  SolveResult result{Termination::Optimal(2.0)};
  result.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x_1, 0.0}, {x_2, 2.0}},
                         .objective_value = 2.0,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{y_1, 0.5}, {y_2, -0.5}},
                       .reduced_costs = {{x_1, 0.0}, {x_2, 0.0}},
                       .objective_value = 2.0,
                       .feasibility_status = SolutionStatus::kFeasible},
      .basis = Basis{.constraint_status = {{y_1, BasisStatus::kAtUpperBound},
                                           {y_2, BasisStatus::kAtLowerBound}},
                     .variable_status = {{x_1, BasisStatus::kBasic},
                                         {x_2, BasisStatus::kBasic}},
                     .basic_dual_feasibility = SolutionStatus::kFeasible}});
  return {std::move(model), result};
}

TEST_P(SimpleLpTest, ConstraintDefinedBasisLPRanged) {
  if (!GetParam().supports_basis) {
    GTEST_SKIP()
        << "Getting the basis is not supported for this config, skipping test.";
  }
  const SolvedModel solved_model = ConstraintDefinedBasisLP(2.0);
  EXPECT_THAT(Solve(*solved_model.model, TestedSolver()),
              IsOkAndHolds(IsConsistentWith(solved_model.expected_result,
                                            {.check_basis = true})));
}

TEST_P(SimpleLpTest, ConstraintDefinedBasisLPNonRanged) {
  if (!GetParam().supports_basis) {
    GTEST_SKIP()
        << "Getting the basis is not supported for this config, skipping test.";
  }
  const SolvedModel solved_model = ConstraintDefinedBasisLP(kInf);
  EXPECT_THAT(Solve(*solved_model.model, TestedSolver()),
              IsOkAndHolds(IsConsistentWith(solved_model.expected_result,
                                            {.check_basis = true})));
}

// For p in [2.0, kInf]
// Primal:
//   max  2.0*x_1 + x_2
//   s.t. - p  <= x_1 + x_2 <= 2.0 (y_1)
//        -2.0 <= x_1 - x_2 <= p   (y_2)
//        -1.0 <= x_1 <= 1.0
//         0.0 <= x_2 <= kInf
//
// Dual (go/mathopt-doc-math#dual):
//   min d_max(y, r)
//        y_1 + y_2 + r_1 == 2.0
//        y_1 - y_2 + r_2 == 1.0
//
// Unique optimal primal solution is (x*_1, x*_2) = (1.0. 1.0).
// Complementary slackness conditions for x*
// (go/mathopt-dual#primal-dual-optimal-pairs) imply (note that we have
// a maximization problem so the inequalities in the TIP environment hold):
//
//        y_1 + y_2 + r_1 == 2.0
//        y_1 - y_2 + r_2 == 1.0
//                    y_2 == 0.0
//                    r_2 == 0.0
//                    y_1 >= 0.0
//                    r_1 >= 0.0
//
// which has the unique solution
// (y*_1, y*_2, r*_1, r*_2) = (1.0, 0.0, 1.0, 0.0).
//
// From go/mathopt-basis#primal we have C = {1}, V = {1},
//
// s^c_1 = AT_UPPER_BOUND
// s^c_2 = BASIC
// s^v_1 = AT_UPPER_BOUND
// s^v_2 = BASIC
//
// We can check that these statuses are compatible with the dual feasibility
// conditions in go/mathopt-basis#dual (note again that we have
// a maximization problem so the inequalities in the TIP environment hold).
SolvedModel ConstraintVariableDefinedBasisLP(const double p) {
  auto model = std::make_unique<Model>();
  model->set_is_maximize(true);
  const Variable x_1 = model->AddContinuousVariable(-1.0, 1.0, "x_1");
  const Variable x_2 = model->AddContinuousVariable(0.0, kInf, "x_2");
  model->Maximize(2 * x_1 + x_2);
  const LinearConstraint y_1 =
      model->AddLinearConstraint(-p <= x_1 + x_2 <= 2.0, "y_1");
  const LinearConstraint y_2 =
      model->AddLinearConstraint(-2.0 <= x_1 - x_2 <= p, "y_2");

  SolveResult result{Termination::Optimal(3.0)};
  result.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x_1, 1.0}, {x_2, 1.0}},
                         .objective_value = 3,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{y_1, 1.0}, {y_2, 0.0}},
                       .reduced_costs = {{x_1, 1.0}, {x_2, 0.0}},
                       .objective_value = 3.0,
                       .feasibility_status = SolutionStatus::kFeasible},
      .basis = Basis{.constraint_status = {{y_1, BasisStatus::kAtUpperBound},
                                           {y_2, BasisStatus::kBasic}},
                     .variable_status = {{x_1, BasisStatus::kAtUpperBound},
                                         {x_2, BasisStatus::kBasic}},
                     .basic_dual_feasibility = SolutionStatus::kFeasible}});

  return {std::move(model), result};
}

TEST_P(SimpleLpTest, ConstraintVariableDefinedBasisLPRanged) {
  if (!GetParam().supports_basis) {
    GTEST_SKIP()
        << "Getting the basis is not supported for this config, skipping test.";
  }
  const SolvedModel solved_model = ConstraintVariableDefinedBasisLP(2.0);
  EXPECT_THAT(Solve(*solved_model.model, TestedSolver()),
              IsOkAndHolds(IsConsistentWith(solved_model.expected_result,
                                            {.check_basis = true})));
}

TEST_P(SimpleLpTest, ConstraintVariableDefinedBasisLPNonRanged) {
  if (!GetParam().supports_basis) {
    GTEST_SKIP()
        << "Getting the basis is not supported for this config, skipping test.";
  }
  const SolvedModel solved_model = ConstraintVariableDefinedBasisLP(kInf);
  EXPECT_THAT(Solve(*solved_model.model, TestedSolver()),
              IsOkAndHolds(IsConsistentWith(solved_model.expected_result,
                                            {.check_basis = true})));
}

// For p in [2.0, kInf]
// Primal:
//   min  x_1 + x_2
//   s.t. - p  <= x_1 + x_2 <= 2.0 (y_1)
//        -2.0 <= x_1 - x_2 <= p   (y_2)
//        -1.0 <= x_1 <= 1.0
//         0.0 <= x_2 <= kInf
//
// Dual (go/mathopt-doc-math#dual):
//   min d(y, r)
//        y_1 + y_2 + r_1 == 1.0
//        y_1 - y_2 + r_2 == 1.0
//
// Unique optimal primal solution is (x*_1, x*_2) = (-1.0. 0.0).
// Complementary slackness conditions for x*
// (go/mathopt-dual#primal-dual-optimal-pairs) imply:
//
//        y_1 + y_2 + r_1 == 1.0
//        y_1 - y_2 + r_2 == 1.0
//                    y_1 == 0.0
//                    y_2 == 0.0
//                    r_1 >= 0.0
//                    r_2 >= 0.0
//
// which has the unique solution
// (y*_1, y*_2, r*_1, r*_2) = (0.0, 0.0, 1.0, 1.0).
//
// From go/mathopt-basis#primal we have C = {}, V = {1, 2},
//
// s^c_1 = BASIC
// s^c_2 = BASIC
// s^v_1 = AT_LOWER_BOUND
// s^v_2 = AT_LOWER_BOUND
//
// We can check that these statuses are compatible with the dual feasibility
// conditions in go/mathopt-basis#dual.
SolvedModel VariableDefinedBasisLP(const double p) {
  auto model = std::make_unique<Model>();
  const Variable x_1 = model->AddContinuousVariable(-1.0, 1.0, "x_1");
  const Variable x_2 = model->AddContinuousVariable(0.0, kInf, "x_2");
  model->Minimize(x_1 + x_2);
  const LinearConstraint y_1 =
      model->AddLinearConstraint(-p <= x_1 + x_2 <= 2.0, "y_1");
  const LinearConstraint y_2 =
      model->AddLinearConstraint(-2.0 <= x_1 - x_2 <= p, "y_2");

  SolveResult result{Termination::Optimal(-1.0)};
  result.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x_1, -1.0}, {x_2, 0.0}},
                         .objective_value = -1,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{y_1, 0.0}, {y_2, 0.0}},
                       .reduced_costs = {{x_1, 1.0}, {x_2, 1.0}},
                       .objective_value = -1,
                       .feasibility_status = SolutionStatus::kFeasible},
      .basis = Basis{.constraint_status = {{y_1, BasisStatus::kBasic},
                                           {y_2, BasisStatus::kBasic}},
                     .variable_status = {{x_1, BasisStatus::kAtLowerBound},
                                         {x_2, BasisStatus::kAtLowerBound}},
                     .basic_dual_feasibility = SolutionStatus::kFeasible}});
  return {std::move(model), result};
}

TEST_P(SimpleLpTest, VariableDefinedBasisLPRanged) {
  if (!GetParam().supports_basis) {
    GTEST_SKIP()
        << "Getting the basis is not supported for this config, skipping test.";
  }
  const SolvedModel solved_model = VariableDefinedBasisLP(2.0);
  EXPECT_THAT(Solve(*solved_model.model, TestedSolver()),
              IsOkAndHolds(IsConsistentWith(solved_model.expected_result,
                                            {.check_basis = true})));
}

TEST_P(SimpleLpTest, VariableDefinedBasisLPNonRanged) {
  if (!GetParam().supports_basis) {
    GTEST_SKIP()
        << "Getting the basis is not supported for this config, skipping test.";
  }
  const SolvedModel solved_model = VariableDefinedBasisLP(kInf);
  EXPECT_THAT(Solve(*solved_model.model, TestedSolver()),
              IsOkAndHolds(IsConsistentWith(solved_model.expected_result,
                                            {.check_basis = true})));
}

// Primal:
//   max x_1 + x_2
//   s.t. 0.0 <= - x_1 + x_2 <= 0.0  (y_1)
//        0.0 <= x_1 <= 1.0
//      -kInf <= x_2 <= kInf
//
// Dual (go/mathopt-doc-math#dual):
//   min d_max(y, r)
//        -y_1 + r_1 == 1.0
//         y_1 + r_2 == 1.0
//
// Unique optimal primal solution is (x*_1, x*_2) = (1.0. 1.0).
// Complementary slackness conditions for x*
// (go/mathopt-dual#primal-dual-optimal-pairs) imply (note that we have
// a maximization problem so the inequalities in the TIP environment hold):
//
//        -y_1 + r_1 == 1.0
//         y_1 + r_2 == 1.0
//               r_2 == 0.0
//               r_1 >= 0.0
//
// which has the unique solution (y_1*, r*_1, r*_2) = (1.0, 2.0, 0.0).
// Dual feasibility of the basis (go/mathopt-basis#dual) and the sign
// of y*_1 imply that if the status of constraint (y_1) is not FIXED_VALUE, then
// it must be AT_UPPER_BOUND (note again that we have a maximization problem so
// the inequalities in the TIP environment hold). We can confirm this logic by
// noting that if we only keep the upper bound of constraint (y_1), the problem
// is unchanged.
TEST_P(SimpleLpTest, FixedBasis) {
  if (!GetParam().supports_basis) {
    GTEST_SKIP()
        << "Getting the basis is not supported for this config, skipping test.";
  }

  Model model;
  const Variable x_1 = model.AddContinuousVariable(0.0, 1.0, "x_1");
  const Variable x_2 = model.AddContinuousVariable(-kInf, kInf, "x_2");
  model.Maximize(x_1 + x_2);
  const LinearConstraint y_1 =
      model.AddLinearConstraint(-x_1 + x_2 == 0.0, "y_1");
  ASSERT_OK_AND_ASSIGN(const SolveResult result, Solve(model, TestedSolver()));
  ASSERT_THAT(result, IsOptimalWithSolution(2.0, {{x_1, 1.0}, {x_2, 1.0}}));
  ASSERT_THAT(result, IsOptimalWithDualSolution(2.0, {{y_1, 1.0}},
                                                {{x_1, 2.0}, {x_2, 0.0}}));
  const Basis expected_basis_alternative_one = {
      .constraint_status = {{y_1, BasisStatus::kFixedValue}},
      .variable_status = {{x_1, BasisStatus::kAtUpperBound},
                          {x_2, BasisStatus::kBasic}},
      .basic_dual_feasibility = SolutionStatus::kFeasible};
  const Basis expected_basis_alternative_two = {
      .constraint_status = {{y_1, BasisStatus::kAtUpperBound}},
      .variable_status = {{x_1, BasisStatus::kAtUpperBound},
                          {x_2, BasisStatus::kBasic}},
      .basic_dual_feasibility = SolutionStatus::kFeasible};

  ASSERT_TRUE(result.has_basis());
  EXPECT_THAT(*result.solutions[0].basis,
              AnyOf(BasisIs(expected_basis_alternative_one),
                    BasisIs(expected_basis_alternative_two)));
}

// Primal:
//   max 0.0
//   s.t. -kInf <= 2.0 * x_2 <= kInf  (y_1)
//        -kInf <= x_1 <= kInf
//        -kInf <= x_2 <= kInf
//
// Dual (go/mathopt-doc-math#dual):
//   min d_max(y, r)
//                  r_1 == 0.0
//        2.0*y_1 + r_2 == 0.0
//
// Any value for (x*_1, x*_2) yields an optimal solution. Complementary
// slackness conditions for any of these x*
// (go/mathopt-dual#primal-dual-optimal-pairs) imply:
//
//                  r_1 == 0.0
//        2.0*y_1 + r_2 == 0.0
//                  y_1 == 0.0
//                  r_1 == 0.0
//                  r_2 == 0.0

// By the cardinality and dimension requirements for a basis
// (go/mathopt-basis#primal) we have two possibilities for a basis:
//
// 1) C = {1} and V = {1}
// 2) C = {}  and V = {1, 2}
//
// For case 1), the finite/infinite bound conditions imply both y_1 and x_1
// must be BasisStatus::kFree. For y_1 this forces 2.0 * x_2 = 0 and for x_1 it
// forces x_1 = 0 yielding the basic solution (x_1, x_2) = (0.0, 0.0).
// (x_2 is BasisStatus::kBasic because 1 is not in V).
//
// For case 2), the finite/infinite bound conditions imply both x_1 and x_2
// must be BasisStatus::kFree. For x_1 this forces x_1 = 0 and for x_2 it
// forces x_2 = 0 yielding the basic solution (x_1, x_2) = (0.0, 0.0).
// (y_1 is BasisStatus::kBasic because 1 is not in C).
//

TEST_P(SimpleLpTest, FreeBasis) {
  if (!GetParam().supports_basis) {
    GTEST_SKIP()
        << "Getting the basis is not supported for this config, skipping test.";
  }

  Model model;
  model.Maximize(0.0);
  const Variable x_1 = model.AddContinuousVariable(-kInf, kInf, "x_1");
  const Variable x_2 = model.AddContinuousVariable(-kInf, kInf, "x_2");
  const LinearConstraint y_1 =
      model.AddLinearConstraint(-kInf <= 2 * x_2 <= kInf, "y_1");
  ASSERT_OK_AND_ASSIGN(const SolveResult result, Solve(model, TestedSolver()));
  ASSERT_THAT(result, IsOptimalWithSolution(0.0, {{x_1, 0.0}, {x_2, 0.0}}));
  ASSERT_THAT(result, IsOptimalWithDualSolution(0.0, {{y_1, 0.0}},
                                                {{x_1, 0.0}, {x_2, 0.0}}));

  const Basis expected_basis_alternative_one = {
      .constraint_status = {{y_1, BasisStatus::kFree}},
      .variable_status = {{x_1, BasisStatus::kFree},
                          {x_2, BasisStatus::kBasic}},
      .basic_dual_feasibility = SolutionStatus::kFeasible};
  const Basis expected_basis_alternative_two = {
      .constraint_status = {{y_1, BasisStatus::kBasic}},
      .variable_status = {{x_1, BasisStatus::kFree}, {x_2, BasisStatus::kFree}},
      .basic_dual_feasibility = SolutionStatus::kFeasible};

  ASSERT_TRUE(result.has_basis());
  EXPECT_THAT(*result.solutions[0].basis,
              AnyOf(BasisIs(expected_basis_alternative_one),
                    BasisIs(expected_basis_alternative_two)));
}

// Two simple incremental tests that check solver-result-structures are cleared
// between solves. Would have caught b/225153929 and a Gurobi issue resolved
// in cl/436321712. Using SimpleLpTest fixture to test multiple solve
// parameters.
TEST_P(SimpleLpTest, OptimalAfterInfeasible) {
  // TODO(b/226146622): Check if this is a GLPK bug.
  if (TestedSolver() == SolverType::kGlpk &&
      GetParam().parameters.lp_algorithm == LPAlgorithm::kBarrier) {
    GTEST_SKIP() << "Glpk returns [GLP_EFAIL] for the first solve.";
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Minimize(x);
  model.AddLinearConstraint(x >= 2);

  const SolveArguments arguments{.parameters = GetParam().parameters};

  ASSERT_OK_AND_ASSIGN(auto solver,
                       NewIncrementalSolver(&model, TestedSolver()));
  EXPECT_THAT(solver->Solve(arguments),
              IsOkAndHolds(TerminatesWithOneOf(
                  {TerminationReason::kInfeasible,
                   TerminationReason::kInfeasibleOrUnbounded})));
  model.set_upper_bound(x, 3);
  EXPECT_THAT(solver->Solve(arguments), IsOkAndHolds(IsOptimal(2.0)));
}

TEST_P(SimpleLpTest, OptimalAfterUnbounded) {
  // TODO(b/226146622): Check if this is a GLPK bug.
  if (TestedSolver() == SolverType::kGlpk &&
      GetParam().parameters.lp_algorithm == LPAlgorithm::kBarrier) {
    GTEST_SKIP() << "Glpk returns [GLP_EFAIL] for the first solve.";
  }
  Model model;
  const Variable x = model.AddContinuousVariable(-kInf, 1, "x");
  model.Minimize(x);

  const SolveArguments arguments{.parameters = GetParam().parameters};

  ASSERT_OK_AND_ASSIGN(auto solver,
                       NewIncrementalSolver(&model, TestedSolver()));
  EXPECT_THAT(solver->Solve(arguments),
              IsOkAndHolds(TerminatesWithOneOf(
                  {TerminationReason::kUnbounded,
                   TerminationReason::kInfeasibleOrUnbounded})));
  model.set_lower_bound(x, 0);
  EXPECT_THAT(solver->Solve(arguments), IsOkAndHolds(IsOptimal(0.0)));
}

TEST_P(IncrementalLpTest, EmptyUpdate) {
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(), IsOkAndHolds(IsOptimal(12.1)));
}

TEST_P(IncrementalLpTest, ObjDir) {
  model_.set_minimize();
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(), IsOkAndHolds(IsOptimal(0.1)));
}

TEST_P(IncrementalLpTest, ObjOffset) {
  model_.set_objective_offset(1.1);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(), IsOkAndHolds(IsOptimal(13.1)));
}

TEST_P(IncrementalLpTest, LinearObjCoef) {
  model_.set_objective_coefficient(x_1_, 5.0);
  model_.set_objective_coefficient(x_2_, 5.0);
  model_.set_objective_coefficient(x_3_, 5.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * 6 + 0.1)));
}

TEST_P(IncrementalLpTest, LinearObjCoefAndRemove) {
  model_.DeleteVariable(zero_);
  model_.set_objective_coefficient(x_1_, 5.0);
  model_.set_objective_coefficient(x_2_, 5.0);
  model_.set_objective_coefficient(x_3_, 5.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * 6 + 0.1)));
}

TEST_P(IncrementalLpTest, LinearObjCoefAfterRemove) {
  model_.DeleteVariable(zero_);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));

  model_.set_objective_coefficient(x_1_, 5.0);
  model_.set_objective_coefficient(x_2_, 5.0);
  model_.set_objective_coefficient(x_3_, 5.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * 6 + 0.1)));
}

//   max 0.1 + sum_{i=0}^2 (3.0 *x_i + 2.0 * y_i)
//   s.t. x_i + y_i <= 1.5 for all i \in {0,1,2} (c_i)
//       0 <= x_i <= 1
//       0 <= y_i <= 1 for all i \in {0,1,2}
//
// Optimal solution is (x_i,y_i)=(1.0, 0.5) for all i \in {0,1,2}, with
// objective value 3*4+0.1 = 12.1.

TEST_P(IncrementalLpTest, VariableLb) {
  model_.set_lower_bound(y_1_, 0.75);
  model_.set_lower_bound(y_2_, 0.75);
  model_.set_lower_bound(y_3_, 0.75);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * (3 * 0.75 + 2 * 0.75) + 0.1)));
}

TEST_P(IncrementalLpTest, VariableLbAndRemove) {
  model_.DeleteVariable(zero_);
  model_.set_lower_bound(y_1_, 0.75);
  model_.set_lower_bound(y_2_, 0.75);
  model_.set_lower_bound(y_3_, 0.75);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * (3 * 0.75 + 2 * 0.75) + 0.1)));
}

TEST_P(IncrementalLpTest, VariableLbAfterRemove) {
  model_.DeleteVariable(zero_);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));

  model_.set_lower_bound(y_1_, 0.75);
  model_.set_lower_bound(y_2_, 0.75);
  model_.set_lower_bound(y_3_, 0.75);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * (3 * 0.75 + 2 * 0.75) + 0.1)));
}

TEST_P(IncrementalLpTest, VariableUb) {
  model_.set_upper_bound(x_1_, 0.5);
  model_.set_upper_bound(x_2_, 0.5);
  model_.set_upper_bound(x_3_, 0.5);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * (3 * 0.5 + 2 * 1) + 0.1)));
}

TEST_P(IncrementalLpTest, LinearConstraintLb) {
  model_.set_lower_bound(c_1_, 1.0);
  model_.set_lower_bound(c_2_, 1.0);
  model_.set_lower_bound(c_3_, 1.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver_->SolveWithoutUpdate());
  EXPECT_THAT(result, IsOptimal(12.1));
  // Changing the lower bound does not effect the optimal solution, an
  // incremental solve does no work.
  EXPECT_EQ(result.solve_stats.simplex_iterations, 0);
  EXPECT_EQ(result.solve_stats.barrier_iterations, 0);
  EXPECT_EQ(result.solve_stats.first_order_iterations, 0);
}

// TODO(b/184447031): Consider more cases (e.g. induced by upper-bound changes).
TEST_P(IncrementalLpTest, ConstraintTypeSwitch) {
  // Check constraint-type changes by adding or removing finite lower bounds.
  // For some solvers this results in addition/deletion of slacks.

  // Single one-sided to two-sided change:
  //   * c_1_ from one-sided to two-sided
  model_.set_lower_bound(c_1_, 1.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult first_result,
                       solver_->SolveWithoutUpdate());
  EXPECT_THAT(first_result, IsOptimal(12.1));
  // Changing the lower bound does not effect the optimal solution, an
  // incremental solve does no work.
  EXPECT_EQ(first_result.solve_stats.simplex_iterations, 0);
  EXPECT_EQ(first_result.solve_stats.barrier_iterations, 0);
  EXPECT_EQ(first_result.solve_stats.first_order_iterations, 0);

  // Simultaneous changes in both directions:
  //   * c_1_ from two-sided to one-sided
  //   * c_2_ from one-sided to two-sided
  model_.set_lower_bound(c_1_, -kInf);
  model_.set_lower_bound(c_2_, 1.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult second_result,
                       solver_->SolveWithoutUpdate());
  EXPECT_THAT(second_result, IsOptimal(12.1));
  // Changing the lower bound does not effect the optimal solution, an
  // incremental solve does no work.
  EXPECT_EQ(second_result.solve_stats.simplex_iterations, 0);
  EXPECT_EQ(second_result.solve_stats.barrier_iterations, 0);
  EXPECT_EQ(second_result.solve_stats.first_order_iterations, 0);
  // Single two-sided to one-sided change:
  //   * c_2_ from two-sided to one-sided
  model_.set_lower_bound(c_2_, -kInf);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  ASSERT_OK_AND_ASSIGN(const SolveResult third_result,
                       solver_->SolveWithoutUpdate());
  EXPECT_THAT(third_result, IsOptimal(12.1));
  // Changing the lower bound does not effect the optimal solution, an
  // incremental solve does no work.
  EXPECT_EQ(third_result.solve_stats.simplex_iterations, 0);
  EXPECT_EQ(third_result.solve_stats.barrier_iterations, 0);
  EXPECT_EQ(third_result.solve_stats.first_order_iterations, 0);
}

TEST_P(IncrementalLpTest, LinearConstraintUb) {
  model_.set_upper_bound(c_1_, 2.0);
  model_.set_upper_bound(c_2_, 2.0);
  model_.set_upper_bound(c_3_, 2.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * (3 * 1 + 2 * 1) + 0.1)));
}

TEST_P(IncrementalLpTest, LinearConstraintCoefficient) {
  model_.set_coefficient(c_1_, y_1_, 0.5);
  model_.set_coefficient(c_2_, y_2_, 0.5);
  model_.set_coefficient(c_3_, y_3_, 0.5);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * (3 * 1 + 2 * 1) + 0.1)));
}

TEST_P(IncrementalLpTest, AddVariable) {
  const Variable z_1 = model_.AddContinuousVariable(0.0, 1.0, "z_1");
  model_.set_objective_coefficient(z_1, 10.0);
  model_.set_coefficient(c_1_, z_1, 1.0);
  const Variable z_2 = model_.AddContinuousVariable(0.0, 1.0, "z_2");
  model_.set_objective_coefficient(z_2, 10.0);
  model_.set_coefficient(c_2_, z_2, 1.0);
  const Variable z_3 = model_.AddContinuousVariable(0.0, 1.0, "z_3");
  model_.set_objective_coefficient(z_3, 10.0);
  model_.set_coefficient(c_3_, z_3, 1.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * (3 * 0.5 + 2 * 0 + 10 * 1) + 0.1)));
}

TEST_P(IncrementalLpTest, AddLinearConstraint) {
  const LinearConstraint d_1 = model_.AddLinearConstraint(0.0, 2.0, "d_1");
  model_.set_coefficient(d_1, x_1_, 1.0);
  model_.set_coefficient(d_1, y_1_, 2.0);
  const LinearConstraint d_2 = model_.AddLinearConstraint(0.0, 2.0, "d_2");
  model_.set_coefficient(d_2, x_2_, 1.0);
  model_.set_coefficient(d_2, y_2_, 2.0);
  const LinearConstraint d_3 = model_.AddLinearConstraint(0.0, 2.0, "d_3");
  model_.set_coefficient(d_3, x_3_, 1.0);
  model_.set_coefficient(d_3, y_3_, 2.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * (3 * 1 + 2 * 0.5) + 0.1)));
}

TEST_P(IncrementalLpTest, DeleteVariable) {
  model_.DeleteVariable(x_1_);
  model_.DeleteVariable(x_2_);
  model_.DeleteVariable(x_3_);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * (2 * 1) + 0.1)));
}

TEST_P(IncrementalLpTest, DeleteLinearConstraint) {
  model_.DeleteLinearConstraint(c_1_);
  model_.DeleteLinearConstraint(c_2_);
  model_.DeleteLinearConstraint(c_3_);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  EXPECT_THAT(solver_->SolveWithoutUpdate(),
              IsOkAndHolds(IsOptimal(3 * (3 * 1 + 2 * 1) + 0.1)));
}

TEST_P(IncrementalLpTest, ChangeBoundsWithTemporaryInversion) {
  model_.set_lower_bound(x_1_, 3.0);
  // At this point x_1_ lower bound is 3.0 and upper bound is 1.0.
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));

  model_.set_upper_bound(x_1_, 5.0);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));
  // At this point x_1_ upper bound is 5.0 and so is greater than the new lower
  // bound.

  // To make the problem feasible we update the bound of the constraint that
  // contains x_1_; we take this opportunity to also test inverting bounds of
  // constraints.
  model_.set_lower_bound(c_1_, 4.0);
  // At this point c_1_ lower bound is 4.0 and upper bound is 1.5.
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));

  // We restore valid bounds by setting c_1_ upper bound to 5.5.
  model_.set_upper_bound(c_1_, 5.5);
  ASSERT_THAT(solver_->Update(), IsOkAndHolds(DidUpdate()));

  EXPECT_THAT(
      solver_->SolveWithoutUpdate(),
      IsOkAndHolds(IsOptimal((3 * 5 + 2 * 0.5) + 2 * (3 * 1 + 2 * 0.5) + 0.1)));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
