// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/integer_expr.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

// Weighted sum <= constant reified.
void AddWeightedSumLowerOrEqualReif(Literal is_le,
                                    absl::Span<const IntegerVariable> vars,
                                    absl::Span<const int64_t> coefficients,
                                    int64_t upper_bound, Model* model) {
  AddWeightedSumLowerOrEqual({is_le}, vars, coefficients, upper_bound, model);
  AddWeightedSumGreaterOrEqual({is_le.Negated()}, vars, coefficients,
                               upper_bound + 1, model);
}

// Weighted sum >= constant reified.
void AddWeightedSumGreaterOrEqualReif(Literal is_ge,
                                      absl::Span<const IntegerVariable> vars,
                                      absl::Span<const int64_t> coefficients,
                                      int64_t lower_bound, Model* model) {
  AddWeightedSumGreaterOrEqual({is_ge}, vars, coefficients, lower_bound, model);
  AddWeightedSumLowerOrEqual({is_ge.Negated()}, vars, coefficients,
                             lower_bound - 1, model);
}

// Weighted sum == constant reified.
// TODO(user): Simplify if the constant is at the edge of the possible values.
void AddFixedWeightedSumReif(Literal is_eq,
                             absl::Span<const IntegerVariable> vars,
                             absl::Span<const int64_t> coefficients,
                             int64_t value, Model* model) {
  // We creates two extra Boolean variables in this case. The alternative is
  // to code a custom propagator for the direction equality => reified.
  const Literal is_le = Literal(model->Add(NewBooleanVariable()), true);
  const Literal is_ge = Literal(model->Add(NewBooleanVariable()), true);
  model->Add(ReifiedBoolAnd({is_le, is_ge}, is_eq));
  AddWeightedSumLowerOrEqualReif(is_le, vars, coefficients, value, model);
  AddWeightedSumGreaterOrEqualReif(is_ge, vars, coefficients, value, model);
}

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

CpSolverResponse SolveAndCheck(
    const CpModelProto& initial_model, absl::string_view extra_parameters = "",
    absl::btree_set<std::vector<int>>* solutions = nullptr) {
  SatParameters params;
  params.set_enumerate_all_solutions(true);
  if (!extra_parameters.empty()) {
    params.MergeFromString(extra_parameters);
  }
  auto observer = [&](const CpSolverResponse& response) {
    VLOG(2) << response;
    EXPECT_TRUE(SolutionIsFeasible(
        initial_model, std::vector<int64_t>(response.solution().begin(),
                                            response.solution().end())));
    if (solutions != nullptr) {
      std::vector<int> solution;
      for (int var = 0; var < initial_model.variables_size(); ++var) {
        solution.push_back(response.solution(var));
      }
      solutions->insert(solution);
    }
  };
  Model model;
  model.Add(NewSatParameters(params));
  model.Add(NewFeasibleSolutionObserver(observer));
  return SolveCpModel(initial_model, &model);
}

// A simple macro to make the code more readable.
#define EXPECT_BOUNDS_EQ(var, lb, ub)               \
  EXPECT_TRUE((model.Get(LowerBound(var)) == lb) && \
              (model.Get(UpperBound(var)) == ub))

TEST(WeightedSumTest, LevelZeroPropagation) {
  Model model;
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(4, 9)),
                                    model.Add(NewIntegerVariable(-7, -2)),
                                    model.Add(NewIntegerVariable(3, 8))};

  const IntegerVariable sum =
      model.Add(NewWeightedSum(std::vector<int>{1, -2, 3}, vars));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_EQ(model.Get(LowerBound(sum)), 4 + 2 * 2 + 3 * 3);
  EXPECT_EQ(model.Get(UpperBound(sum)), 9 + 2 * 7 + 3 * 8);

  // Setting this leave only a slack of 2.
  model.Add(LowerOrEqual(sum, 19));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(vars[0], 4, 6);    // coeff = 1, slack = 2
  EXPECT_BOUNDS_EQ(vars[1], -3, -2);  // coeff = 2, slack = 1
  EXPECT_BOUNDS_EQ(vars[2], 3, 3);    // coeff = 3, slack = 0
}

TEST(WeightedSumLowerOrEqualTest, UnaryRounding) {
  Model model;
  IntegerVariable var = model.Add(NewIntegerVariable(0, 10));
  const std::vector<int64_t> coeffs = {-100};
  model.Add(WeightedSumLowerOrEqual({var}, coeffs, -320));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_EQ(model.Get(LowerBound(var)), 4);
}

// This one used to fail before CL 139204507.
TEST(WeightedSumTest, LevelZeroPropagationWithNegativeNumbers) {
  Model model;
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(-5, 0)),
                                    model.Add(NewIntegerVariable(-6, 0)),
                                    model.Add(NewIntegerVariable(-4, 0))};

  const IntegerVariable sum =
      model.Add(NewWeightedSum(std::vector<int>{3, 3, 3}, vars));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_EQ(model.Get(LowerBound(sum)), -15 * 3);
  EXPECT_EQ(model.Get(UpperBound(sum)), 0);

  // Setting this leave only a slack of 5 which is not an exact multiple of 3.
  model.Add(LowerOrEqual(sum, -40));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(vars[0], -5, -4);
  EXPECT_BOUNDS_EQ(vars[1], -6, -5);
  EXPECT_BOUNDS_EQ(vars[2], -4, -3);
}

TEST(ReifiedWeightedSumLeTest, ReifToBoundPropagation) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 9));
  AddWeightedSumLowerOrEqualReif(r, {var}, std::vector<int64_t>{1}, 6, &model);
  EXPECT_EQ(
      SatSolver::FEASIBLE,
      model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions({r}));
  EXPECT_BOUNDS_EQ(var, 4, 6);
  EXPECT_EQ(SatSolver::FEASIBLE,
            model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions(
                {r.Negated()}));
  EXPECT_BOUNDS_EQ(var, 7, 9);  // The associated literal (x <= 6) is false.
}

TEST(ReifiedWeightedSumLeTest, ReifToBoundPropagationWithNegatedCoeff) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(-9, 9));
  AddWeightedSumLowerOrEqualReif(r, {var}, std::vector<int64_t>{-3}, 7, &model);
  EXPECT_EQ(
      SatSolver::FEASIBLE,
      model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions({r}));
  EXPECT_BOUNDS_EQ(var, -2, 9);
  EXPECT_EQ(SatSolver::FEASIBLE,
            model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions(
                {r.Negated()}));
  EXPECT_BOUNDS_EQ(var, -9, -3);  // The associated literal (x >= -2) is false.
}

TEST(ReifiedWeightedSumGeTest, ReifToBoundPropagation) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 9));
  AddWeightedSumGreaterOrEqualReif(r, {var}, std::vector<int64_t>{1}, 6,
                                   &model);
  EXPECT_EQ(
      SatSolver::FEASIBLE,
      model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions({r}));
  EXPECT_BOUNDS_EQ(var, 6, 9);
  EXPECT_EQ(SatSolver::FEASIBLE,
            model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions(
                {r.Negated()}));
  EXPECT_BOUNDS_EQ(var, 4, 5);
}

TEST(ReifiedFixedWeightedSumTest, ReifToBoundPropagation) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 9));
  AddFixedWeightedSumReif(r, {var}, std::vector<int64_t>{1}, 6, &model);
  EXPECT_EQ(
      SatSolver::FEASIBLE,
      model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions({r}));
  EXPECT_BOUNDS_EQ(var, 6, 6);

  // Because we introduced intermediate Boolean, we decide if var is < 6 or > 6.
  EXPECT_EQ(SatSolver::FEASIBLE,
            model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions(
                {r.Negated()}));
  if (model.Get(LowerBound(var)) == 4) {
    EXPECT_BOUNDS_EQ(var, 4, 5);
  } else {
    EXPECT_BOUNDS_EQ(var, 7, 9);
  }
}

TEST(ReifiedWeightedSumTest, BoundToReifTrueLe) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 9));
  AddWeightedSumLowerOrEqualReif(r, {var}, std::vector<int64_t>{1}, 9, &model);
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_TRUE(model.Get(Value(r)));
}

TEST(ReifiedWeightedSumTest, BoundToReifFalseLe) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 9));
  AddWeightedSumLowerOrEqualReif(r, {var}, std::vector<int64_t>{1}, 3, &model);
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_FALSE(model.Get(Value(r)));
}

TEST(ReifiedWeightedSumTest, BoundToReifTrueEq) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 4));
  AddFixedWeightedSumReif(r, {var}, std::vector<int64_t>{1}, 4, &model);
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_TRUE(model.Get(Value(r)));
}

TEST(ReifiedWeightedSumTest, BoundToReifFalseEq1) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 6));
  AddFixedWeightedSumReif(r, {var}, std::vector<int64_t>{1}, 8, &model);
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_FALSE(model.Get(Value(r)));
}

TEST(ReifiedWeightedSumTest, BoundToReifFalseEq2) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 6));
  AddFixedWeightedSumReif(r, {var}, std::vector<int64_t>{1}, 3, &model);
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_FALSE(model.Get(Value(r)));
}

TEST(ConditionalLbTest, BasicPositiveCase) {
  Model model;
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable obj = model.Add(NewIntegerVariable(-10, 10));

  std::vector<IntegerVariable> vars{var, obj};
  std::vector<IntegerValue> coeffs{6, -2};
  const IntegerValue rhs = 4;
  IntegerSumLE constraint({}, vars, coeffs, rhs, &model);

  // We have 2 * obj >= 6 * var - 4.
  const auto result =
      constraint.ConditionalLb(IntegerLiteral::GreaterOrEqual(var, 1), obj);
  EXPECT_EQ(result.first, -2);  // When false.
  EXPECT_EQ(result.second, 1);  // When true.

  // We have 2 * obj >= 6 * var - 4.
  const auto result2 =
      constraint.ConditionalLb(IntegerLiteral::GreaterOrEqual(var, 3), obj);
  EXPECT_EQ(result2.first, -2);  // When false.
  EXPECT_EQ(result2.second, 7);  // When true.
}

TEST(ConditionalLbTest, CornerCase) {
  Model model;
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable obj = model.Add(NewIntegerVariable(-10, 10));

  std::vector<IntegerVariable> vars{var, obj};
  std::vector<IntegerValue> coeffs{6, -2};
  const IntegerValue rhs = 4;
  IntegerSumLE constraint({}, vars, coeffs, rhs, &model);

  // Here we don't even look at the equation.
  const auto result =
      constraint.ConditionalLb(IntegerLiteral::GreaterOrEqual(obj, 2), obj);
  EXPECT_EQ(result.first, kMinIntegerValue);  // When false.
  EXPECT_EQ(result.second, 2);                // When true.

  const auto result2 =
      constraint.ConditionalLb(IntegerLiteral::LowerOrEqual(obj, 3), obj);
  EXPECT_EQ(result2.first, 4);                  // When false.
  EXPECT_EQ(result2.second, kMinIntegerValue);  // When true.
}

TEST(ConditionalLbTest, BasicNegativeCase) {
  Model model;
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable obj = model.Add(NewIntegerVariable(-10, 10));

  std::vector<IntegerVariable> vars{var, obj};
  std::vector<IntegerValue> coeffs{-6, -1};
  const IntegerValue rhs = -4;
  IntegerSumLE constraint({}, vars, coeffs, rhs, &model);

  // We have obj >= 4 - 6 * var.
  const auto result =
      constraint.ConditionalLb(IntegerLiteral::LowerOrEqual(var, 0), obj);
  EXPECT_EQ(result.first, -2);  // false, var <= 1
  EXPECT_EQ(result.second, 4);  // true, var <= 0.
}

TEST(MinMaxTest, LevelZeroPropagation) {
  Model model;
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(4, 9)),
                                    model.Add(NewIntegerVariable(2, 7)),
                                    model.Add(NewIntegerVariable(3, 8))};
  const IntegerVariable min = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable max = model.Add(NewIntegerVariable(0, 10));
  model.Add(IsEqualToMinOf(min, vars));
  model.Add(IsEqualToMaxOf(max, vars));

  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(min, 2, 7);
  EXPECT_BOUNDS_EQ(max, 4, 9);

  model.Add(LowerOrEqual(min, 5));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(min, 2, 5);

  model.Add(GreaterOrEqual(max, 7));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(max, 7, 9);

  // Test the propagation in the other direction (PrecedencesPropagator).
  model.Add(GreaterOrEqual(min, 5));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(vars[0], 5, 9);
  EXPECT_BOUNDS_EQ(vars[1], 5, 7);
  EXPECT_BOUNDS_EQ(vars[2], 5, 8);

  model.Add(LowerOrEqual(max, 8));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(vars[0], 5, 8);
  EXPECT_BOUNDS_EQ(vars[1], 5, 7);
  EXPECT_BOUNDS_EQ(vars[2], 5, 8);
}

TEST(LinMinMaxTest, LevelZeroPropagation) {
  Model model;
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(4, 9)),
                                    model.Add(NewIntegerVariable(2, 7)),
                                    model.Add(NewIntegerVariable(3, 8))};
  std::vector<LinearExpression> exprs;
  for (const IntegerVariable var : vars) {
    LinearExpression expr;
    expr.vars.push_back(var);
    expr.coeffs.push_back(1);
    exprs.push_back(expr);
  }
  const IntegerVariable min = model.Add(NewIntegerVariable(-100, 100));
  LinearExpression min_expr;
  min_expr.vars.push_back(min);
  min_expr.coeffs.push_back(1);
  model.Add(IsEqualToMinOf(min_expr, exprs));

  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(min, 2, 7);

  model.Add(LowerOrEqual(min, 5));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(min, 2, 5);

  // Test the propagation in the other direction (PrecedencesPropagator).
  model.Add(GreaterOrEqual(min, 5));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(vars[0], 5, 9);
  EXPECT_BOUNDS_EQ(vars[1], 5, 7);
  EXPECT_BOUNDS_EQ(vars[2], 5, 8);
}

TEST(MinTest, OnlyOnePossibleCandidate) {
  Model model;
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(4, 7)),
                                    model.Add(NewIntegerVariable(2, 9)),
                                    model.Add(NewIntegerVariable(5, 8))};
  const IntegerVariable min = model.Add(NewIntegerVariable(0, 10));
  model.Add(IsEqualToMinOf(min, vars));

  // So far everything is normal.
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(min, 2, 7);

  // But now, if the min is known to be <= 3, the minimum variable is known! it
  // has to be variable #1, so we can propagate its upper bound.
  model.Add(LowerOrEqual(min, 3));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(min, 2, 3);
  EXPECT_BOUNDS_EQ(vars[1], 2, 3);

  // Test infeasibility.
  model.Add(LowerOrEqual(min, 1));
  EXPECT_EQ(SatSolver::INFEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
}

TEST(LinMinTest, OnlyOnePossibleCandidate) {
  Model model;
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(4, 7)),
                                    model.Add(NewIntegerVariable(2, 9)),
                                    model.Add(NewIntegerVariable(5, 8))};
  std::vector<LinearExpression> exprs;
  for (const IntegerVariable var : vars) {
    LinearExpression expr;
    expr.vars.push_back(var);
    expr.coeffs.push_back(1);
    exprs.push_back(expr);
  }
  const IntegerVariable min = model.Add(NewIntegerVariable(-100, 100));
  LinearExpression min_expr;
  min_expr.vars.push_back(min);
  min_expr.coeffs.push_back(1);
  model.Add(IsEqualToMinOf(min_expr, exprs));

  // So far everything is normal.
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(min, 2, 7);

  // But now, if the min is known to be <= 3, the minimum variable is known! it
  // has to be variable #1, so we can propagate its upper bound.
  model.Add(LowerOrEqual(min, 3));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(min, 2, 3);
  EXPECT_BOUNDS_EQ(vars[1], 2, 3);

  // Test infeasibility.
  model.Add(LowerOrEqual(min, 1));
  EXPECT_EQ(SatSolver::INFEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
}

TEST(LinMinTest, OnlyOnePossibleExpr) {
  Model model;
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(1, 2)),
                                    model.Add(NewIntegerVariable(0, 3)),
                                    model.Add(NewIntegerVariable(-2, 4))};
  std::vector<LinearExpression> exprs;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  LinearExpression expr1;  // 2x0 + 3x1 - 5
  expr1.vars = {vars[0], vars[1]};
  expr1.coeffs = {2, 3};
  expr1.offset = -5;
  expr1 = CanonicalizeExpr(expr1);
  EXPECT_EQ(-3, expr1.Min(*integer_trail));
  EXPECT_EQ(8, expr1.Max(*integer_trail));

  LinearExpression expr2;  // 2x1 - 5x2 + 6
  expr2.vars = {vars[1], vars[2]};
  expr2.coeffs = {2, -5};
  expr2.offset = 6;
  expr2 = CanonicalizeExpr(expr2);
  EXPECT_EQ(-14, expr2.Min(*integer_trail));
  EXPECT_EQ(22, expr2.Max(*integer_trail));

  LinearExpression expr3;  // 2x0 + 3x2
  expr3.vars = {vars[0], vars[2]};
  expr3.coeffs = {2, 3};
  expr3 = CanonicalizeExpr(expr3);
  EXPECT_EQ(-4, expr3.Min(*integer_trail));
  EXPECT_EQ(16, expr3.Max(*integer_trail));

  exprs.push_back(expr1);
  exprs.push_back(expr2);
  exprs.push_back(expr3);
  IntegerVariable min = model.Add(NewIntegerVariable(-100, 100));
  LinearExpression min_expr;
  min_expr.vars.push_back(min);
  min_expr.coeffs.push_back(1);
  model.Add(IsEqualToMinOf(min_expr, exprs));

  // So far everything is normal.
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(min, -14, 8);

  // But now, if the min is known to be <= -5, the minimum expression has to be
  // expr 2, so we can propagate its upper bound.
  model.Add(LowerOrEqual(min, -5));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(min, -14, -5);
  EXPECT_BOUNDS_EQ(vars[0], 1, 2);
  EXPECT_BOUNDS_EQ(vars[1], 0, 3);
  EXPECT_BOUNDS_EQ(vars[2], 3, 4);
  // NOTE: The expression bound is not as tight because the underlying variable
  // bounds can't be propagated enough without throwing away valid solutions.
  EXPECT_LE(expr2.Max(*integer_trail), -3);

  // Test infeasibility.
  model.Add(LowerOrEqual(min, -15));
  EXPECT_EQ(SatSolver::INFEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
}

TEST(OneOfTest, BasicPropagation) {
  Model model;

  IntegerVariable var = model.Add(NewIntegerVariable(0, 10));
  std::vector<Literal> selectors;
  for (int i = 0; i < 5; ++i) {
    selectors.push_back(Literal(model.Add(NewBooleanVariable()), true));
  }
  std::vector<IntegerValue> values{5, 0, 3, 3, 9};

  model.Add(IsOneOf(var, selectors, values));

  // We start with nothing fixed and then start fixing variables.
  SatSolver* solver = model.GetOrCreate<SatSolver>();
  EXPECT_TRUE(solver->Propagate());
  EXPECT_BOUNDS_EQ(var, 0, 9);
  EXPECT_TRUE(solver->EnqueueDecisionIfNotConflicting(selectors[1].Negated()));
  EXPECT_BOUNDS_EQ(var, 3, 9);
  EXPECT_TRUE(solver->EnqueueDecisionIfNotConflicting(selectors[4].Negated()));
  EXPECT_BOUNDS_EQ(var, 3, 5);
  EXPECT_TRUE(solver->EnqueueDecisionIfNotConflicting(selectors[2].Negated()));
  EXPECT_BOUNDS_EQ(var, 3, 5);
  EXPECT_TRUE(solver->EnqueueDecisionIfNotConflicting(selectors[3].Negated()));
  EXPECT_BOUNDS_EQ(var, 5, 5);

  // Now we restrict the possible values by changing the bound.
  solver->Backtrack(0);
  model.Add(LowerOrEqual(var, 3));
  EXPECT_EQ(
      SatSolver::FEASIBLE,
      model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions({}));
  EXPECT_FALSE(model.Get(Value(selectors[0])));
  EXPECT_FALSE(model.Get(Value(selectors[4])));
}

// Propagates a * b = p by hand. Return false if the domains are empty,
// otherwise returns true and the expected domains value. This is slow and
// work in O(product of domain(a).size() * domain(b).size())!.
bool TestProductPropagation(const IntegerTrail& trail,
                            std::vector<IntegerVariable> vars,
                            std::vector<IntegerValue>* expected_mins,
                            std::vector<IntegerValue>* expected_maxs) {
  const IntegerValue min_a = trail.LowerBound(vars[0]);
  const IntegerValue max_a = trail.UpperBound(vars[0]);
  const IntegerValue min_b = trail.LowerBound(vars[1]);
  const IntegerValue max_b = trail.UpperBound(vars[1]);
  const IntegerValue min_p = trail.LowerBound(vars[2]);
  const IntegerValue max_p = trail.UpperBound(vars[2]);

  std::vector<absl::btree_set<IntegerValue>> new_values(3);
  for (IntegerValue va(min_a); va <= max_a; ++va) {
    for (IntegerValue vb(min_b); vb <= max_b; ++vb) {
      const IntegerValue vp = va * vb;
      if (vp >= min_p && vp <= max_p) {
        new_values[0].insert(va);
        new_values[1].insert(vb);
        new_values[2].insert(vp);
      }
    }
  }
  if (new_values[0].empty() || new_values[1].empty() || new_values[2].empty()) {
    return false;
  }

  expected_mins->clear();
  expected_maxs->clear();
  for (int i = 0; i < 3; ++i) {
    std::vector<IntegerValue> sorted(new_values[i].begin(),
                                     new_values[i].end());
    expected_mins->push_back(sorted.front());
    expected_maxs->push_back(sorted.back());
  }
  return true;
}

TEST(ProductConstraintTest, RandomCases) {
  absl::BitGen random;

  int num_non_perfect = 0;
  const int num_tests = 1000;
  for (int i = 0; i < num_tests; ++i) {
    Model model;
    IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
    std::vector<IntegerVariable> vars;
    std::string input_string;
    for (int v = 0; v < 3; ++v) {
      const int limit = v < 2 ? 20 : 200;
      int64_t min = absl::Uniform<int>(random, -limit, limit);
      int64_t max = absl::Uniform<int>(random, -limit, limit);
      if (min > max) std::swap(min, max);
      absl::StrAppend(&input_string,
                      (v == 1   ? " * "
                       : v == 2 ? " = "
                                : ""),
                      "[", min, ", ", max, "]");
      vars.push_back(model.Add(NewIntegerVariable(min, max)));
    }

    // Start by computing the expected result.
    std::vector<IntegerValue> expected_mins;
    std::vector<IntegerValue> expected_maxs;
    const bool expected_result = TestProductPropagation(
        *integer_trail, vars, &expected_mins, &expected_maxs);

    bool perfect_propagation = true;
    bool ok_propagation = true;
    model.Add(ProductConstraint(vars[0], vars[1], vars[2]));
    const bool result = model.GetOrCreate<SatSolver>()->Propagate();
    if (expected_result != result) {
      if (expected_result) {
        ok_propagation = false;
      } else {
        // If the exact result is UNSAT, we might not have seen that.
        perfect_propagation = false;
      }
    }
    std::string expected_string;
    std::string result_string;
    for (int i = 0; i < 3; ++i) {
      const int64_t lb = integer_trail->LowerBound(vars[i]).value();
      const int64_t ub = integer_trail->UpperBound(vars[i]).value();
      if (expected_result) {
        if (expected_mins[i] != lb) perfect_propagation = false;
        if (expected_mins[i] < lb) ok_propagation = false;
        if (expected_maxs[i] != ub) perfect_propagation = false;
        if (expected_maxs[i] > ub) ok_propagation = false;

        // We should always be exact on the domain of a and b.
        if (i < 2 && !perfect_propagation) {
          ok_propagation = false;
        }
        absl::StrAppend(&expected_string, "[", expected_mins[i].value(), ", ",
                        expected_maxs[i].value(), "] ");
      }

      if (result) {
        absl::StrAppend(&result_string, "[", lb, ", ", ub, "] ");
      }
    }
    if (!perfect_propagation || !ok_propagation) {
      VLOG(1) << "Imperfect on input: " << input_string;
      if (expected_result) {
        VLOG(1) << "Expected: " << expected_string;
        if (result) {
          VLOG(1) << "Result:   " << result_string;
        } else {
          VLOG(1) << "UNSAT was received.";
        }
      } else {
        VLOG(1) << "Result:   " << result_string;
        VLOG(1) << "UNSAT was expected.";
      }
      ++num_non_perfect;
    }
    ASSERT_TRUE(ok_propagation);
  }

  // Unfortunately our TestProductPropagation() is too good and in some corner
  // cases like when the product is [18, 19] it can detect stuff like the
  // product 19 (which is prime) can't be reached by any product a * b,
  // whereas our propagator doesn't see that!
  LOG(INFO) << "Num imperfect: " << num_non_perfect << " / " << num_tests;
  EXPECT_LT(num_non_perfect, num_tests / 2);
}

TEST(ProductConstraintTest, RestrictedProductDomainPosPos) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'y' domain: 0 domain: 3 }
    variables { name: 'x' domain: 0 domain: 2 }
    variables { name: 'p' domain: 0 domain: 4 }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {0, 0, 0}, {0, 1, 0}, {0, 2, 0}, {1, 0, 0}, {1, 1, 1}, {1, 2, 2},
      {2, 0, 0}, {2, 1, 2}, {2, 2, 4}, {3, 0, 0}, {3, 1, 3},
  };
  EXPECT_EQ(solutions, expected);
}

TEST(ProductConstraintTest, RestrictedProductDomainPosNeg) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'y' domain: 0 domain: 3 }
    variables { name: 'x' domain: -2 domain: 0 }
    variables { name: 'p' domain: -4 domain: 0 }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {0, 0, 0}, {0, -1, 0},  {0, -2, 0},  {1, 0, 0}, {1, -1, -1}, {1, -2, -2},
      {2, 0, 0}, {2, -1, -2}, {2, -2, -4}, {3, 0, 0}, {3, -1, -3},
  };
  EXPECT_EQ(solutions, expected);
}

TEST(ProductConstraintTest, RestrictedProductDomainNegPos) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'y' domain: -3 domain: 0 }
    variables { name: 'x' domain: 0 domain: 2 }
    variables { name: 'p' domain: -4 domain: 0 }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {0, 0, 0},   {0, 1, 0},   {0, 2, 0},   {-1, 0, 0},
      {-1, 1, -1}, {-1, 2, -2}, {-2, 0, 0},  {-2, 1, -2},
      {-2, 2, -4}, {-3, 0, 0},  {-3, 1, -3},
  };
  EXPECT_EQ(solutions, expected);
}

TEST(ProductConstraintTest, RestrictedProductDomainNegNeg) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'y' domain: -3 domain: 0 }
    variables { name: 'x' domain: -2 domain: 0 }
    variables { name: 'p' domain: 0 domain: 4 }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {0, 0, 0},   {0, -1, 0},  {0, -2, 0},  {-1, 0, 0},
      {-1, -1, 1}, {-1, -2, 2}, {-2, 0, 0},  {-2, -1, 2},
      {-2, -2, 4}, {-3, 0, 0},  {-3, -1, 3},
  };
  EXPECT_EQ(solutions, expected);
}

TEST(ProductConstraintTest, ProductIsNull) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'y' domain: 0 domain: 3 }
    variables { name: 'x' domain: 0 domain: 2 }
    variables { name: 'p' domain: 0 domain: 6 }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
    constraints { linear { vars: 2 coeffs: 1 domain: 0 domain: 0 } }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{{0, 0, 0}, {0, 1, 0}, {0, 2, 0},
                                             {1, 0, 0}, {2, 0, 0}, {3, 0, 0}};
  EXPECT_EQ(solutions, expected);
}

TEST(ProductConstraintTest, CheckAllSolutionsRandomProblem) {
  absl::BitGen random;
  const int kMaxValue = 50;
  const int kNumLoops = DEBUG_MODE ? 50 : 100;

  for (int loop = 0; loop < kNumLoops; ++loop) {
    CpModelProto cp_model;
    int x_min = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    int x_max = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    if (x_min > x_max) std::swap(x_min, x_max);
    IntegerVariableProto* x = cp_model.add_variables();
    x->add_domain(x_min);
    x->add_domain(x_max);

    int y_min = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    int y_max = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    if (y_min > y_max) std::swap(y_min, y_max);
    IntegerVariableProto* y = cp_model.add_variables();
    y->add_domain(y_min);
    y->add_domain(y_max);

    int z_min = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    int z_max = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    if (z_min > z_max) std::swap(z_min, z_max);
    IntegerVariableProto* z = cp_model.add_variables();
    z->add_domain(z_min);
    z->add_domain(z_max);

    // z == x * y.
    LinearArgumentProto* prod = cp_model.add_constraints()->mutable_int_prod();
    prod->add_exprs()->add_vars(0);  // x.
    prod->mutable_exprs(0)->add_coeffs(1);
    prod->add_exprs()->add_vars(1);  // y
    prod->mutable_exprs(1)->add_coeffs(1);
    prod->mutable_target()->add_vars(2);  // z
    prod->mutable_target()->add_coeffs(1);

    absl::btree_set<std::vector<int>> solutions;
    const CpSolverResponse response =
        SolveAndCheck(cp_model, "linearization_level:0", &solutions);

    // Loop through the domains of x and y, and collect valid solutions.
    absl::btree_set<std::vector<int>> expected;
    for (int i = x_min; i <= x_max; ++i) {
      for (int j = y_min; j <= y_max; ++j) {
        const int k = i * j;
        if (k < z_min || k > z_max) continue;
        expected.insert({i, j, k});
      }
    }

    // Checks that we get the same solution set through the two methods.
    EXPECT_EQ(solutions, expected);
  }
}

TEST(ProductPropagationTest, RightAcrossZero) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'y' domain: 2 domain: 4 }
    variables { name: 'x' domain: -6 domain: 6 }
    variables { name: 'p' domain: -30 domain: 30 }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {2, -6, -12}, {3, -6, -18}, {4, -6, -24}, {2, -5, -10}, {3, -5, -15},
      {4, -5, -20}, {2, -4, -8},  {3, -4, -12}, {4, -4, -16}, {2, -3, -6},
      {3, -3, -9},  {4, -3, -12}, {2, -2, -4},  {3, -2, -6},  {4, -2, -8},
      {2, -1, -2},  {3, -1, -3},  {4, -1, -4},  {2, 0, 0},    {3, 0, 0},
      {4, 0, 0},    {2, 1, 2},    {3, 1, 3},    {4, 1, 4},    {2, 2, 4},
      {3, 2, 6},    {4, 2, 8},    {2, 3, 6},    {3, 3, 9},    {4, 3, 12},
      {2, 4, 8},    {3, 4, 12},   {4, 4, 16},   {2, 5, 10},   {3, 5, 15},
      {4, 5, 20},   {2, 6, 12},   {3, 6, 18},   {4, 6, 24},
  };
  EXPECT_EQ(solutions.size(), 3 * 13);
  EXPECT_EQ(solutions, expected);
}

TEST(ProductPropagationTest, BothAcrossZero) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'y' domain: -2 domain: 3 }
    variables { name: 'x' domain: -3 domain: 2 }
    variables { name: 'p' domain: -10 domain: 10 }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {-2, -3, 6}, {-2, -2, 4}, {-2, -1, 2}, {-2, 0, 0},  {-2, 1, -2},
      {-2, 2, -4}, {-1, -3, 3}, {-1, -2, 2}, {-1, -1, 1}, {-1, 0, 0},
      {-1, 1, -1}, {-1, 2, -2}, {0, -3, 0},  {0, -2, 0},  {0, -1, 0},
      {0, 0, 0},   {0, 1, 0},   {0, 2, 0},   {1, -3, -3}, {1, -2, -2},
      {1, -1, -1}, {1, 0, 0},   {1, 1, 1},   {1, 2, 2},   {2, -3, -6},
      {2, -2, -4}, {2, -1, -2}, {2, 0, 0},   {2, 1, 2},   {2, 2, 4},
      {3, -3, -9}, {3, -2, -6}, {3, -1, -3}, {3, 0, 0},   {3, 1, 3},
      {3, 2, 6}};
  EXPECT_EQ(solutions.size(), 6 * 6);
  EXPECT_EQ(solutions, expected);
}

TEST(ProductPropagationTest, BothAcrossZeroWithRangeRestriction) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'y' domain: -2 domain: 3 }
    variables { name: 'x' domain: -3 domain: 2 }
    variables { name: 'p' domain: -3 domain: 4 }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {-2, -2, 4}, {-2, -1, 2}, {-2, 0, 0},  {-2, 1, -2}, {-1, -3, 3},
      {-1, -2, 2}, {-1, -1, 1}, {-1, 0, 0},  {-1, 1, -1}, {-1, 2, -2},
      {0, -3, 0},  {0, -2, 0},  {0, -1, 0},  {0, 0, 0},   {0, 1, 0},
      {0, 2, 0},   {1, -3, -3}, {1, -2, -2}, {1, -1, -1}, {1, 0, 0},
      {1, 1, 1},   {1, 2, 2},   {2, -1, -2}, {2, 0, 0},   {2, 1, 2},
      {2, 2, 4},   {3, -1, -3}, {3, 0, 0},   {3, 1, 3},
  };
  EXPECT_EQ(solutions, expected);
}

TEST(ProductPropagationTest, BothAcrossZeroWithPositiveTarget) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -2, 6 ] }
    variables { domain: [ -2, 6 ] }
    variables { domain: [ 12, 12 ] }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {2, 6, 12}, {3, 4, 12}, {4, 3, 12}, {6, 2, 12}};
  EXPECT_EQ(solutions, expected);
}

TEST(ProductPropagationTest, BothAcrossZeroWithFarPositiveTarget) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -2, 6 ] }
    variables { domain: [ -2, 6 ] }
    variables { domain: [ 15, 15 ] }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{{3, 5, 15}, {5, 3, 15}};
  EXPECT_EQ(solutions, expected);
}

TEST(ProductPropagationTest, BothAcrossZeroWithNegativeTarget) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -2, 6 ] }
    variables { domain: [ -2, 6 ] }
    variables { domain: [ -12, -12 ] }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{{-2, 6, -12}, {6, -2, -12}};
  EXPECT_EQ(solutions, expected);
}

TEST(ProductPropagationTest, LargePositiveDomain) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 3000000000 }
    variables { domain: 0 domain: 3000000000 }
    variables { domain: [ -30, -15, 15, 30 ] }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  const Domain dp = ReadDomainFromProto(initial_model.variables(2));
  absl::btree_set<std::vector<int>> expected;
  for (int vx = 0; vx <= 30; ++vx) {
    for (int vy = 0; vy <= 30; ++vy) {
      if (dp.Contains(vx * vy)) {
        expected.insert(std::vector<int>{vx, vy, vx * vy});
      }
    }
  }
  EXPECT_EQ(solutions, expected);
}

TEST(ProductPropagationTest, LargeDomain) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: -30 domain: 3000000000 }
    variables { domain: -30 domain: 3000000000 }
    variables { domain: [ -30, -15, 15, 30 ] }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  const Domain dp = ReadDomainFromProto(initial_model.variables(2));
  absl::btree_set<std::vector<int>> expected;
  for (int vx = -30; vx <= 30; ++vx) {
    for (int vy = -30; vy <= 30; ++vy) {
      if (dp.Contains(vx * vy)) {
        expected.insert(std::vector<int>{vx, vy, vx * vy});
      }
    }
  }
  EXPECT_EQ(solutions, expected);
}

TEST(DivisionConstraintTest, CheckAllSolutions) {
  absl::BitGen random;
  const int kMaxValue = 100;
  const int kShift = 10;
  const int kNumLoops = DEBUG_MODE ? 100 : 1000;

  for (int loop = 0; loop < kNumLoops; ++loop) {
    // Generate domains for x, y, and z.
    // z is meant to be roughly compatible with x / y. There can still be no
    // feasible solutions.
    CpModelProto cp_model;
    const int x_min = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    const int x_max = absl::Uniform<int>(random, x_min, kMaxValue);
    IntegerVariableProto* x = cp_model.add_variables();
    x->add_domain(x_min);
    x->add_domain(x_max);

    const int y_min = absl::Uniform<int>(random, 1, kMaxValue);
    const int y_max = absl::Uniform<int>(random, y_min, kMaxValue);
    IntegerVariableProto* y = cp_model.add_variables();
    y->add_domain(y_min);
    y->add_domain(y_max);

    const int z_min = std::max(
        x_min / y_max + absl::Uniform<int>(random, -kShift, kShift), 0);
    const int z_max = std::max(
        z_min, x_max / y_min + absl::Uniform<int>(random, -kShift, kShift));
    IntegerVariableProto* z = cp_model.add_variables();
    z->add_domain(z_min);
    z->add_domain(z_max);

    // z == x / y.
    LinearArgumentProto* div = cp_model.add_constraints()->mutable_int_div();
    div->add_exprs()->add_vars(0);  // x.
    div->mutable_exprs(0)->add_coeffs(1);
    div->add_exprs()->add_vars(1);  // y
    div->mutable_exprs(1)->add_coeffs(1);
    div->mutable_target()->add_vars(2);  // z
    div->mutable_target()->add_coeffs(1);

    absl::btree_set<std::vector<int>> solutions;
    const CpSolverResponse response =
        SolveAndCheck(cp_model, "linearization_level:0", &solutions);

    // Loop through the domains of x and y, and collect valid solutions.
    absl::btree_set<std::vector<int>> expected;
    for (int i = x_min; i <= x_max; ++i) {
      for (int j = y_min; j <= y_max; ++j) {
        const int k = i / j;
        if (k < z_min || k > z_max) continue;
        expected.insert({i, j, k});
      }
    }

    // Checks that we get the same solution set through the two methods.
    EXPECT_EQ(solutions, expected)
        << "x = [" << x_min << ".." << x_max << "], y = [" << y_min << ".."
        << y_max << "], z = [" << z_min << ".." << z_max << "]\n---------\n"
        << ProtobufDebugString(cp_model) << "---------\n";
  }
}

TEST(DivisionConstraintTest, NumeratorAcrossZeroPositiveDenom) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -2, 6 ] }
    variables { domain: [ 2, 4 ] }
    variables { domain: [ -1, 3 ] }
    constraints {
      int_div {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "linearization_level:0", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {-2, 2, -1}, {-2, 3, 0}, {-2, 4, 0}, {-1, 2, 0}, {-1, 3, 0}, {-1, 4, 0},
      {0, 2, 0},   {0, 3, 0},  {0, 4, 0},  {1, 2, 0},  {1, 3, 0},  {1, 4, 0},
      {2, 2, 1},   {2, 3, 0},  {2, 4, 0},  {3, 2, 1},  {3, 3, 1},  {3, 4, 0},
      {4, 2, 2},   {4, 3, 1},  {4, 4, 1},  {5, 2, 2},  {5, 3, 1},  {5, 4, 1},
      {6, 2, 3},   {6, 3, 2},  {6, 4, 1}};
  EXPECT_EQ(solutions, expected);
}

TEST(DivisionConstraintTest, NumeratorAcrossZeroNegativeDenom) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -2, 6 ] }
    variables { domain: [ -4, -2 ] }
    variables { domain: [ -3, 1 ] }
    constraints {
      int_div {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "linearization_level:0", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {-2, -4, 0}, {-2, -3, 0}, {-2, -2, 1}, {-1, -4, 0}, {-1, -3, 0},
      {-1, -2, 0}, {0, -4, 0},  {0, -3, 0},  {0, -2, 0},  {1, -4, 0},
      {1, -3, 0},  {1, -2, 0},  {2, -4, 0},  {2, -3, 0},  {2, -2, -1},
      {3, -4, 0},  {3, -3, -1}, {3, -2, -1}, {4, -4, -1}, {4, -3, -1},
      {4, -2, -2}, {5, -4, -1}, {5, -3, -1}, {5, -2, -2}, {6, -4, -1},
      {6, -3, -2}, {6, -2, -3}};
  EXPECT_EQ(solutions, expected);
}

TEST(DivisionConstraintTest, CheckAllPropagationsRandomProblem) {
  absl::BitGen random;
  const int kMaxValue = 50;
  const int kMaxDenom = 10;
  const int kNumLoops = DEBUG_MODE ? 5000 : 100000;

  for (int loop = 0; loop < kNumLoops; ++loop) {
    // Generate domains for x, y, and z.
    int x_min = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    int x_max = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    if (x_min > x_max) std::swap(x_min, x_max);
    int y_min = absl::Uniform<int>(random, 1, kMaxDenom);
    int y_max = absl::Uniform<int>(random, 1, kMaxDenom);
    if (y_min > y_max) std::swap(y_min, y_max);
    int z_min = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    int z_max = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    if (z_min > z_max) std::swap(z_min, z_max);

    // Loop through the domains of x and y, and collect valid bounds.
    int expected_x_min = std::numeric_limits<int>::max();
    int expected_x_max = std::numeric_limits<int>::min();
    int expected_y_min = std::numeric_limits<int>::max();
    int expected_y_max = std::numeric_limits<int>::min();
    int expected_z_min = std::numeric_limits<int>::max();
    int expected_z_max = std::numeric_limits<int>::min();
    for (int i = x_min; i <= x_max; ++i) {
      for (int j = y_min; j <= y_max; ++j) {
        const int k = i / j;
        if (k < z_min || k > z_max) continue;
        expected_x_min = std::min(expected_x_min, i);
        expected_x_max = std::max(expected_x_max, i);
        expected_y_min = std::min(expected_y_min, j);
        expected_y_max = std::max(expected_y_max, j);
        expected_z_min = std::min(expected_z_min, k);
        expected_z_max = std::max(expected_z_max, k);
      }
    }

    Model model;
    const IntegerVariable var_x = model.Add(NewIntegerVariable(x_min, x_max));
    const IntegerVariable var_y = model.Add(NewIntegerVariable(y_min, y_max));
    const IntegerVariable var_z = model.Add(NewIntegerVariable(z_min, z_max));
    model.Add(DivisionConstraint(var_x, var_y, var_z));
    const bool result = model.GetOrCreate<SatSolver>()->Propagate();
    if (result) {
      EXPECT_BOUNDS_EQ(var_x, expected_x_min, expected_x_max);
      EXPECT_BOUNDS_EQ(var_y, expected_y_min, expected_y_max);
      EXPECT_BOUNDS_EQ(var_z, expected_z_min, expected_z_max);
    } else {
      EXPECT_EQ(expected_x_max, std::numeric_limits<int>::min());
    }
  }
}

TEST(DivisionConstraintTest, CheckAllSolutionsOnExprs) {
  absl::BitGen random;
  const int kMaxValue = 30;
  const int kMaxCoeff = 5;
  const int kMaxOffset = 10;
  const int kNumLoops = DEBUG_MODE ? 100 : 10000;

  for (int loop = 0; loop < kNumLoops; ++loop) {
    CpModelProto initial_model;

    // Create the numerator.
    int num_var_min = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    int num_var_max = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    if (num_var_min > num_var_max) std::swap(num_var_min, num_var_max);
    IntegerVariableProto* num_var_proto = initial_model.add_variables();
    num_var_proto->add_domain(num_var_min);
    num_var_proto->add_domain(num_var_max);
    const int64_t num_coeff = absl::Uniform<int64_t>(random, 1, kMaxCoeff) *
                              (absl::Bernoulli(random, 0.5) ? 1 : -1);
    const int64_t num_offset = absl::Uniform(random, -kMaxOffset, kMaxOffset);

    // Create the denominator. Make sure 0 is not accessible.
    int denom_var_min = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    int denom_var_max = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    if (denom_var_min > denom_var_max) std::swap(denom_var_min, denom_var_max);
    const int64_t denom_coeff = absl::Uniform<int64_t>(random, 1, kMaxCoeff) *
                                (absl::Bernoulli(random, 0.5) ? 1 : -1);
    const int64_t denom_offset = absl::Uniform(random, -kMaxOffset, kMaxOffset);
    if (denom_coeff == 0) continue;
    Domain denom_var_domain = {denom_var_min, denom_var_max};
    const int64_t bad_value = -denom_offset / denom_coeff;
    if (denom_var_domain.Contains(bad_value) &&
        bad_value * denom_coeff == -denom_offset) {
      denom_var_domain =
          denom_var_domain.IntersectionWith(Domain(bad_value).Complement());
    }
    IntegerVariableProto* denom_var_proto = initial_model.add_variables();
    FillDomainInProto(denom_var_domain, denom_var_proto);

    int target_var_min = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    int target_var_max = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    if (target_var_min > target_var_max)
      std::swap(target_var_min, target_var_max);
    IntegerVariableProto* target_var_proto = initial_model.add_variables();
    target_var_proto->add_domain(target_var_min);
    target_var_proto->add_domain(target_var_max);
    const int64_t target_coeff = absl::Uniform<int64_t>(random, 1, kMaxCoeff) *
                                 (absl::Bernoulli(random, 0.5) ? 1 : -1);
    const int64_t target_offset =
        absl::Uniform(random, -kMaxOffset, kMaxOffset);

    // target = num / denom.
    LinearArgumentProto* div =
        initial_model.add_constraints()->mutable_int_div();
    div->add_exprs()->add_vars(0);  // num
    div->mutable_exprs(0)->add_coeffs(num_coeff);
    div->mutable_exprs(0)->set_offset(num_offset);
    div->add_exprs()->add_vars(1);  // denom
    div->mutable_exprs(1)->add_coeffs(denom_coeff);
    div->mutable_exprs(1)->set_offset(denom_offset);
    div->mutable_target()->add_vars(2);  // target
    div->mutable_target()->add_coeffs(target_coeff);
    div->mutable_target()->set_offset(target_offset);

    absl::btree_set<std::vector<int>> solutions;
    const CpSolverResponse response =
        SolveAndCheck(initial_model, "linearization_level:0", &solutions);

    // Loop through the domains of var and target, and collect valid solutions.
    absl::btree_set<std::vector<int>> expected;
    for (int i = num_var_min; i <= num_var_max; ++i) {
      const int num_value = num_coeff * i + num_offset;
      for (const int j : denom_var_domain.Values()) {
        const int denom_value = denom_coeff * j + denom_offset;
        if (denom_value == 0) continue;
        const int target_expr_value = num_value / denom_value;
        const int target_var_value =
            (target_expr_value - target_offset) / target_coeff;
        if (target_var_value >= target_var_min &&
            target_var_value <= target_var_max &&
            target_var_value * target_coeff + target_offset ==
                target_expr_value) {
          expected.insert({i, j, target_var_value});
        }
      }
    }

    // Checks that we get we get the same solution set through the two methods.
    EXPECT_EQ(solutions, expected)
        << "\n---------\n"
        << ProtobufDebugString(initial_model) << "---------\n";
  }
}

void TestAllDivisionValues(int64_t min_a, int64_t max_a, int64_t b,
                           int64_t min_c, int64_t max_c) {
  int64_t true_min_a = std::numeric_limits<int64_t>::max();
  int64_t true_max_a = std::numeric_limits<int64_t>::min();
  int64_t true_min_c = std::numeric_limits<int64_t>::max();
  int64_t true_max_c = std::numeric_limits<int64_t>::min();
  for (int64_t a = min_a; a <= max_a; ++a) {
    for (int64_t c = min_c; c <= max_c; ++c) {
      if (a / b == c) {
        true_min_a = std::min(true_min_a, a);
        true_max_a = std::max(true_max_a, a);
        true_min_c = std::min(true_min_c, c);
        true_max_c = std::max(true_max_c, c);
      }
    }
  }
  Model model;
  const AffineExpression var_a =
      min_a == max_a
          ? AffineExpression(IntegerValue(min_a))
          : AffineExpression(model.Add(NewIntegerVariable(min_a, max_a)));
  const AffineExpression var_c =
      min_c == max_c
          ? AffineExpression(IntegerValue(min_c))
          : AffineExpression(model.Add(NewIntegerVariable(min_c, max_c)));
  model.Add(FixedDivisionConstraint(var_a, IntegerValue(b), var_c));
  const bool result = model.GetOrCreate<SatSolver>()->Propagate();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  if (result) {
    EXPECT_EQ(integer_trail->LowerBound(var_a), true_min_a);
    EXPECT_EQ(integer_trail->UpperBound(var_a), true_max_a);
    EXPECT_EQ(integer_trail->LowerBound(var_c), true_min_c);
    EXPECT_EQ(integer_trail->UpperBound(var_c), true_max_c);
  } else {
    EXPECT_EQ(true_min_a, std::numeric_limits<int64_t>::max());  // No solution.
  }
}

TEST(FixedDivisionConstraintTest, AllSmallValues) {
  for (int b = 1; b < 7; ++b) {
    for (int min_a = -10; min_a <= 10; ++min_a) {
      for (int max_a = min_a; max_a <= 10; ++max_a) {
        TestAllDivisionValues(min_a, max_a, b, -20, 20);
      }
    }
    for (int min_c = -10; min_c <= 10; ++min_c) {
      for (int max_c = min_c; max_c <= 10; ++max_c) {
        TestAllDivisionValues(-100, 100, b, min_c, max_c);
      }
    }
  }
}

bool PropagateFixedDivision(int64_t a, int64_t max_a, int64_t b, int64_t c,
                            int64_t max_c, int64_t new_a, int64_t new_max_a,
                            int64_t new_c, int64_t new_max_c) {
  Model model;
  const IntegerVariable var_a = model.Add(NewIntegerVariable(a, max_a));
  const IntegerVariable var_c = model.Add(NewIntegerVariable(c, max_c));
  model.Add(FixedDivisionConstraint(var_a, IntegerValue(b), var_c));
  const bool result = model.GetOrCreate<SatSolver>()->Propagate();
  if (result) {
    EXPECT_BOUNDS_EQ(var_a, new_a, new_max_a);
    EXPECT_BOUNDS_EQ(var_c, new_c, new_max_c);
  }
  return result;
}

TEST(FixedDivisionConstraintTest, ExpectedPropagation) {
  // Propagate from a to c.
  EXPECT_TRUE(PropagateFixedDivision(/*a=*/2, 21, /*b=*/3, /*c=*/-5, 10,
                                     /*new_a=*/2, 21, /*new_c=*/0, 7));
  EXPECT_TRUE(PropagateFixedDivision(/*a=*/4, 20, /*b=*/3, /*c=*/0, 10,
                                     /*new_a=*/4, 20, /*new_c=*/1, 6));
  EXPECT_TRUE(PropagateFixedDivision(/*a=*/-4, 20, /*b=*/3, /*c=*/-5, 10,
                                     /*new_a=*/-4, 20, /*new_c=*/-1, 6));
  EXPECT_TRUE(PropagateFixedDivision(/*a=*/-15, -5, /*b=*/3, /*c=*/-10, 10,
                                     /*new_a=*/-15, -5, /*new_c=*/-5, -1));
  // Propagate from c to a.
  EXPECT_TRUE(PropagateFixedDivision(/*a=*/-10, 10, /*b=*/3, /*c=*/-2, 2,
                                     /*new_a=*/-8, 8, /*new_c=*/-2, 2));
  EXPECT_TRUE(PropagateFixedDivision(/*a=*/-10, 10, /*b=*/3, /*c=*/1, 2,
                                     /*new_a=*/3, 8, /*new_c=*/1, 2));
  EXPECT_TRUE(PropagateFixedDivision(/*a=*/-10, 10, /*b=*/3, /*c=*/0, 2,
                                     /*new_a=*/-2, 8, /*new_c=*/0, 2));
  EXPECT_TRUE(PropagateFixedDivision(/*a=*/-10, 10, /*b=*/3, /*c=*/-2, -1,
                                     /*new_a=*/-8, -3, /*new_c=*/-2, -1));
  EXPECT_TRUE(PropagateFixedDivision(/*a=*/-10, 10, /*b=*/3, /*c=*/-2, 0,
                                     /*new_a=*/-8, 2, /*new_c=*/-2, 0));
  // Check large domains.
  EXPECT_TRUE(PropagateFixedDivision(
      /*a=*/0, std::numeric_limits<int64_t>::max() / 2,
      /*b=*/5, /*c=*/3, std::numeric_limits<int64_t>::max() - 3,
      /*new_a=*/15, std::numeric_limits<int64_t>::max() / 2,
      /*new_c=*/3, std::numeric_limits<int64_t>::max() / 10));
  EXPECT_TRUE(PropagateFixedDivision(
      /*a=*/0, std::numeric_limits<int64_t>::max() / 2,
      /*b=*/5, /*c=*/3, std::numeric_limits<int64_t>::max() - 3,
      /*new_a=*/15, std::numeric_limits<int64_t>::max() / 2,
      /*new_c=*/3, std::numeric_limits<int64_t>::max() / 10));
}

TEST(ModuloConstraintTest, CheckAllSolutions) {
  absl::BitGen random;
  const int kMaxValue = 50;
  const int kMaxModulo = 10;
  const int kNumLoops = DEBUG_MODE ? 200 : 2000;

  for (int loop = 0; loop < kNumLoops; ++loop) {
    CpModelProto initial_model;
    int var_min = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    int var_max = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    if (var_min > var_max) std::swap(var_min, var_max);
    IntegerVariableProto* var = initial_model.add_variables();
    var->add_domain(var_min);
    var->add_domain(var_max);

    const int mod = absl::Uniform<int>(random, 1, kMaxModulo);
    IntegerVariableProto* mod_var = initial_model.add_variables();
    mod_var->add_domain(mod);
    mod_var->add_domain(mod);

    IntegerVariableProto* target = initial_model.add_variables();
    int target_min =
        absl::Uniform<int>(random, -2 * kMaxModulo, 2 * kMaxModulo);
    int target_max =
        absl::Uniform<int>(random, -2 * kMaxModulo, 2 * kMaxModulo);
    if (target_min > target_max) std::swap(target_min, target_max);
    target->add_domain(target_min);
    target->add_domain(target_max);

    // target = var % mod.
    LinearArgumentProto* modulo =
        initial_model.add_constraints()->mutable_int_mod();
    modulo->add_exprs()->add_vars(0);  // var.
    modulo->mutable_exprs(0)->add_coeffs(1);
    modulo->add_exprs()->add_vars(1);  // mod
    modulo->mutable_exprs(1)->add_coeffs(1);
    modulo->mutable_target()->add_vars(2);  // target
    modulo->mutable_target()->add_coeffs(1);

    absl::btree_set<std::vector<int>> solutions;
    const CpSolverResponse response =
        SolveAndCheck(initial_model, "linearization_level:0", &solutions);

    // Loop through the domains of var and target, and collect valid solutions.
    absl::btree_set<std::vector<int>> expected;
    for (int i = var_min; i <= var_max; ++i) {
      const int k = i % mod;
      if (k < target_min || k > target_max) continue;
      expected.insert({i, mod, k});
    }

    // Checks that we get we get the same solution set through the two methods.
    EXPECT_EQ(solutions, expected)
        << "\n---------\n"
        << ProtobufDebugString(initial_model) << "---------\n";
  }
}

TEST(ModuloConstraintTest, CheckAllPropagationsRandomProblem) {
  absl::BitGen random;
  const int kMaxValue = 50;
  const int kMaxModulo = 10;
  const int kNumLoops = DEBUG_MODE ? 5000 : 20000;

  for (int loop = 0; loop < kNumLoops; ++loop) {
    // Generate domains for var and target.
    int var_min = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    int var_max = absl::Uniform<int>(random, -kMaxValue, kMaxValue);
    if (var_min > var_max) std::swap(var_min, var_max);
    int mod = absl::Uniform<int>(random, 2, kMaxModulo);
    int target_min =
        absl::Uniform<int>(random, -2 * kMaxModulo, 2 * kMaxModulo);
    int target_max =
        absl::Uniform<int>(random, -2 * kMaxModulo, 2 * kMaxModulo);
    if (target_min > target_max) std::swap(target_min, target_max);

    // Loop through the domains of var and target, and collect valid bounds.
    int expected_var_min = std::numeric_limits<int>::max();
    int expected_var_max = std::numeric_limits<int>::min();
    int expected_target_min = std::numeric_limits<int>::max();
    int expected_target_max = std::numeric_limits<int>::min();
    for (int i = var_min; i <= var_max; ++i) {
      const int k = i % mod;
      if (k < target_min || k > target_max) continue;
      expected_var_min = std::min(expected_var_min, i);
      expected_var_max = std::max(expected_var_max, i);
      expected_target_min = std::min(expected_target_min, k);
      expected_target_max = std::max(expected_target_max, k);
    }

    Model model;
    const IntegerVariable var = model.Add(NewIntegerVariable(var_min, var_max));
    const IntegerVariable target =
        model.Add(NewIntegerVariable(target_min, target_max));
    model.Add(FixedModuloConstraint(var, IntegerValue(mod), target));
    const bool result = model.GetOrCreate<SatSolver>()->Propagate();
    if (result) {
      EXPECT_BOUNDS_EQ(var, expected_var_min, expected_var_max);
      EXPECT_BOUNDS_EQ(target, expected_target_min, expected_target_max)
          << "var = [" << var_min << ".." << var_max << "], mod = " << mod
          << ", target = [" << target_min << ".." << target_max
          << "], expected_target = [" << expected_target_min << ".."
          << expected_target_max << "], propagated target = ["
          << model.Get(LowerBound(target)) << ".."
          << model.Get(UpperBound(target)) << "]";
    } else {
      EXPECT_EQ(expected_var_max, std::numeric_limits<int>::min());
    }
  }
}

bool TestSquarePropagation(std::pair<int64_t, int64_t> initial_domain_x,
                           std::pair<int64_t, int64_t> initial_domain_s,
                           std::pair<int64_t, int64_t> expected_domain_x,
                           std::pair<int64_t, int64_t> expected_domain_s) {
  Model model;
  IntegerVariable x = model.Add(
      NewIntegerVariable(initial_domain_x.first, initial_domain_x.second));
  IntegerVariable s = model.Add(
      NewIntegerVariable(initial_domain_s.first, initial_domain_s.second));
  model.Add(ProductConstraint(x, x, s));
  const bool result = model.GetOrCreate<SatSolver>()->Propagate();
  if (result) {
    EXPECT_BOUNDS_EQ(x, expected_domain_x.first, expected_domain_x.second);
    EXPECT_BOUNDS_EQ(s, expected_domain_s.first, expected_domain_s.second);
  }
  return result;
}

bool TestSquarePropagation(std::pair<int64_t, int64_t> initial_domain_x,
                           std::pair<int64_t, int64_t> initial_domain_s) {
  return TestSquarePropagation(initial_domain_x, initial_domain_s,
                               initial_domain_x, initial_domain_s);
}

TEST(SquareConstraintTest, SquareExpectedPropagation) {
  // Propagate s -> x, then x -> s.
  EXPECT_TRUE(TestSquarePropagation({0, 3}, {1, 7}, {1, 2}, {1, 4}));
  // Same but negative.
  EXPECT_TRUE(TestSquarePropagation({-3, 0}, {1, 7}, {-2, -1}, {1, 4}));
  // No propagation.
  EXPECT_TRUE(TestSquarePropagation({2, 5}, {4, 25}));
  // Propagate x -> s.
  EXPECT_TRUE(TestSquarePropagation({2, 3}, {1, 12}, {2, 3}, {4, 9}));
  // Infeasible, s has no square in its domain.
  EXPECT_FALSE(TestSquarePropagation({0, 5}, {17, 20}));
  // Infeasible, s cannot be the square of x.
  EXPECT_FALSE(TestSquarePropagation({3, 7}, {50, 100}));
  // Propagate s -> x.
  EXPECT_TRUE(TestSquarePropagation({0, 10}, {16, 25}, {4, 5}, {16, 25}));
}

TEST(SquareConstraintTest, LargestSquare) {
  const int64_t max = kMaxIntegerValue.value();
  const int64_t square =
      static_cast<int64_t>(std::floor(std::sqrt(static_cast<double>(max))));
  CHECK_GE(CapProd(square + 1, square + 1), max);
  EXPECT_TRUE(TestSquarePropagation({0, max}, {0, max}, {0, square},
                                    {0, square * square}));
}

TEST(LevelZeroEqualityTest, BasicExample) {
  Model model;

  const IntegerVariable obj = model.Add(NewIntegerVariable(1, 14));
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(0, 1)),
                                    model.Add(NewIntegerVariable(0, 1)),
                                    model.Add(NewIntegerVariable(0, 1))};
  std::vector<IntegerValue> coeff{3, 4, 3};
  model.TakeOwnership(new LevelZeroEquality(obj, vars, coeff, &model));

  // No propagations.
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_EQ(model.Get(LowerBound(obj)), 1);
  EXPECT_EQ(model.Get(UpperBound(obj)), 14);

  // Fix vars[1], obj is detected to be 3*X + 4.
  //
  // Note that the LB is not 4 because we have just the LevelZeroEquality
  // propagator which doesn't propagate bounds.
  model.Add(GreaterOrEqual(vars[1], 1));
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_EQ(model.Get(LowerBound(obj)), 1);
  EXPECT_EQ(model.Get(UpperBound(obj)), 13);

  // Still propagate when new bound changes.
  model.Add(GreaterOrEqual(obj, 5));
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_EQ(model.Get(LowerBound(obj)), 7);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
