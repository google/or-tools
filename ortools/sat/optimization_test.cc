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

#include "ortools/sat/optimization.h"

#include <stdint.h>

#include <functional>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;

// Test the lazy encoding logic on a trivial problem.
TEST(MinimizeIntegerVariableWithLinearScanAndLazyEncodingTest, BasicProblem) {
  Model model;
  IntegerVariable var = model.Add(NewIntegerVariable(-5, 10));
  model.GetOrCreate<SearchHeuristics>()->fixed_search =
      FirstUnassignedVarAtItsMinHeuristic({var}, &model);
  ConfigureSearchHeuristics(&model);
  int num_feasible_solution = 0;
  SatSolver::Status status =
      MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
          var,
          /*feasible_solution_observer=*/
          [var, &num_feasible_solution, &model]() {
            ++num_feasible_solution;
            EXPECT_EQ(model.Get(Value(var)), -5);
          },

          &model);
  EXPECT_EQ(num_feasible_solution, 1);
  EXPECT_EQ(status, SatSolver::Status::INFEASIBLE);  // Search done.
}

TEST(MinimizeIntegerVariableWithLinearScanAndLazyEncodingTest,
     BasicProblemWithSolutionLimit) {
  Model model;
  SatParameters* parameters = model.GetOrCreate<SatParameters>();
  parameters->set_stop_after_first_solution(true);
  IntegerVariable var = model.Add(NewIntegerVariable(-5, 10));
  model.GetOrCreate<SearchHeuristics>()->fixed_search =
      FirstUnassignedVarAtItsMinHeuristic({var}, &model);
  ConfigureSearchHeuristics(&model);

  SatSolver::Status status =
      MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
          var,
          /*feasible_solution_observer=*/
          [var, &model]() { EXPECT_EQ(model.Get(Value(var)), -5); }, &model);
  EXPECT_EQ(status, SatSolver::Status::LIMIT_REACHED);
}

TEST(MinimizeIntegerVariableWithLinearScanAndLazyEncodingTest,
     BasicProblemWithBadHeuristic) {
  Model model;
  IntegerVariable var = model.Add(NewIntegerVariable(-5, 10));
  int expected_value = 10;
  int num_feasible_solution = 0;

  model.GetOrCreate<SearchHeuristics>()->fixed_search =
      FirstUnassignedVarAtItsMinHeuristic({NegationOf(var)}, &model);
  ConfigureSearchHeuristics(&model);

  SatSolver::Status status =
      MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
          var,
          /*feasible_solution_observer=*/
          [&]() {
            ++num_feasible_solution;
            EXPECT_EQ(model.Get(Value(var)), expected_value--);
          },
          &model);
  EXPECT_EQ(num_feasible_solution, 16);
  EXPECT_EQ(status, SatSolver::Status::INFEASIBLE);  // Search done.
}

// TODO(user): The core find the best solution right away here, so it doesn't
// really exercise the solution limit...
TEST(MinimizeWithCoreAndLazyEncodingTest, BasicProblemWithSolutionLimit) {
  Model model;
  SatParameters* parameters = model.GetOrCreate<SatParameters>();
  parameters->set_stop_after_first_solution(true);
  IntegerVariable var = model.Add(NewIntegerVariable(-5, 10));
  std::vector<IntegerVariable> vars = {var};
  std::vector<IntegerValue> coeffs = {IntegerValue(1)};

  model.GetOrCreate<SearchHeuristics>()->fixed_search =
      FirstUnassignedVarAtItsMinHeuristic({var}, &model);
  ConfigureSearchHeuristics(&model);

  int num_solutions = 0;
  CoreBasedOptimizer core(
      var, vars, coeffs,
      /*feasible_solution_observer=*/
      [var, &model, &num_solutions]() {
        ++num_solutions;
        EXPECT_EQ(model.Get(Value(var)), -5);
      },
      &model);
  SatSolver::Status status = core.Optimize();
  EXPECT_EQ(status, SatSolver::Status::INFEASIBLE);  // i.e. optimal.
  EXPECT_EQ(1, num_solutions);
}

TEST(PresolveBooleanLinearExpressionTest, NegateCoeff) {
  Coefficient offset(0);
  std::vector<Literal> literals = Literals({+1});
  std::vector<Coefficient> coefficients = {Coefficient(-3)};
  PresolveBooleanLinearExpression(&literals, &coefficients, &offset);
  EXPECT_THAT(literals, ElementsAre(Literal(-1)));
  EXPECT_THAT(coefficients, ElementsAre(Coefficient(3)));
  EXPECT_EQ(offset, -3);
}

TEST(PresolveBooleanLinearExpressionTest, Duplicate) {
  Coefficient offset(0);
  std::vector<Literal> literals = Literals({+1, -4, +1});
  std::vector<Coefficient> coefficients = {Coefficient(-3), Coefficient(7),
                                           Coefficient(5)};
  PresolveBooleanLinearExpression(&literals, &coefficients, &offset);
  EXPECT_THAT(literals, ElementsAre(Literal(+1), Literal(-4)));
  EXPECT_THAT(coefficients, ElementsAre(Coefficient(2), Coefficient(7)));
  EXPECT_EQ(offset, 0);
}

TEST(PresolveBooleanLinearExpressionTest, NegatedLiterals) {
  Coefficient offset(0);
  std::vector<Literal> literals = Literals({+1, -4, -1});
  std::vector<Coefficient> coefficients = {Coefficient(-3), Coefficient(7),
                                           Coefficient(-5)};
  PresolveBooleanLinearExpression(&literals, &coefficients, &offset);
  EXPECT_THAT(literals, ElementsAre(Literal(+1), Literal(-4)));
  EXPECT_THAT(coefficients, ElementsAre(Coefficient(2), Coefficient(7)));
  EXPECT_EQ(offset, -5);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
