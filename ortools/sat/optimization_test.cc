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
#include "ortools/sat/clause.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_literal.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/time_limit.h"

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

// TODO(user): The core finds the best solution right away here, so it doesn't
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

TEST(MinimizeCoreWithSearchTest, RemovesIrrelevantAmoGroupAndPartialAmoMember) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* implications = model.GetOrCreate<BinaryImplicationGraph>();
  auto* time_limit = model.GetOrCreate<TimeLimit>();
  sat_solver->SetNumVariables(8);

  // AMO group 1 on {-1, -2, -3} (i.e. at most one of ~x1, ~x2, ~x3 is true):
  // completely irrelevant to the UNSAT core.
  ASSERT_TRUE(implications->AddAtMostOne(Literals({-1, -2, -3})));
  // AMO group 2 on {-4, -5, -6}: only literal +4 is required for the core
  // {+4, +7}, while +5 and +6 are redundant.
  ASSERT_TRUE(implications->AddAtMostOne(Literals({-4, -5, -6})));
  // Make {+4, +7} incompatible via intermediate variable x8:
  // (+4 => +8) and (+7 => -8).
  sat_solver->AddProblemClause(Literals({-4, +8}));
  sat_solver->AddProblemClause(Literals({-7, -8}));

  std::vector<Literal> core = Literals({+1, +2, +3, +4, +5, +6, +7});
  MinimizeCoreWithSearch(time_limit, sat_solver, &core, implications);
  EXPECT_THAT(core, ElementsAre(Literal(+4), Literal(+7)));
}

TEST(OptimizeWithSatEncodingTest, CoverProbingAndStratification) {
  Model model;
  SatParameters* parameters = model.GetOrCreate<SatParameters>();
  parameters->set_cover_optimization(true);
  parameters->set_max_sat_stratification(SatParameters::STRATIFICATION_DESCENT);
  parameters->set_optimize_with_core(true);

  IntegerVariable x = model.Add(NewIntegerVariable(0, 2));
  IntegerVariable y = model.Add(NewIntegerVariable(0, 2));
  IntegerVariable obj = model.Add(NewIntegerVariable(0, 300));

  CpModelProto cp_model;
  cp_model.mutable_objective()->add_vars(0);
  cp_model.mutable_objective()->add_coeffs(1);
  model.GetOrCreate<SharedResponseManager>()->InitializeObjective(cp_model);

  auto* encoder = model.GetOrCreate<IntegerEncoder>();
  auto* sat_solver = model.GetOrCreate<SatSolver>();

  const Literal x_ge_1 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(x, IntegerValue(1)));
  const Literal x_ge_2 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(x, IntegerValue(2)));
  const Literal y_ge_1 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(y, IntegerValue(1)));
  const Literal y_ge_2 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(y, IntegerValue(2)));

  // Constraint: x >= 1
  EXPECT_TRUE(sat_solver->AddUnitClause(x_ge_1));

  // Constraint: x + y >= 2 (if x < 2 then y >= 1; if x < 1 then y >= 2)
  sat_solver->AddProblemClause({x_ge_2, y_ge_1});
  sat_solver->AddProblemClause({x_ge_1, y_ge_2});

  model.GetOrCreate<SearchHeuristics>()->fixed_search =
      FirstUnassignedVarAtItsMinHeuristic({x, y}, &model);
  ConfigureSearchHeuristics(&model);

  std::vector<IntegerVariable> vars = {x, y};
  std::vector<IntegerValue> coeffs = {IntegerValue(100), IntegerValue(10)};

  int num_solutions = 0;
  IntegerValue best_solution_obj = kMaxIntegerValue;
  CoreBasedOptimizer core(
      obj, vars, coeffs,
      [&model, x, y, &num_solutions, &best_solution_obj]() {
        ++num_solutions;
        best_solution_obj =
            IntegerValue(100 * model.Get(Value(x)) + 10 * model.Get(Value(y)));
      },
      &model);

  SatSolver::Status status = core.Optimize();
  EXPECT_EQ(status, SatSolver::Status::INFEASIBLE);  // Optimal proved.
  EXPECT_GT(num_solutions, 0);
  EXPECT_EQ(best_solution_obj, IntegerValue(110));
}

TEST(OptimizeWithSatEncodingTest, MultiStepLazyCoverProbing) {
  Model model;
  SatParameters* parameters = model.GetOrCreate<SatParameters>();
  parameters->set_cover_optimization(true);
  parameters->set_max_sat_stratification(SatParameters::STRATIFICATION_NONE);
  parameters->set_optimize_with_core(true);

  IntegerVariable x = model.Add(NewIntegerVariable(0, 5));
  IntegerVariable y = model.Add(NewIntegerVariable(0, 4));
  IntegerVariable z = model.Add(NewIntegerVariable(0, 4));
  IntegerVariable obj = model.Add(NewIntegerVariable(0, 1000));

  CpModelProto cp_model;
  cp_model.mutable_objective()->add_vars(0);
  cp_model.mutable_objective()->add_coeffs(1);
  model.GetOrCreate<SharedResponseManager>()->InitializeObjective(cp_model);

  auto* encoder = model.GetOrCreate<IntegerEncoder>();
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  const Literal helper(sat_solver->NewBooleanVariable(), true);

  // Enforce x >= 3 via a 1-conflict disjunction so level-0 propagation does not
  // fix x >= 1..3 upfront, exercising multi-step lazy GenericNode cover probes.
  const Literal x_ge_3 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(x, IntegerValue(3)));
  sat_solver->AddProblemClause({x_ge_3, helper});
  sat_solver->AddProblemClause({x_ge_3, helper.Negated()});

  // Enforce y + z >= 4 via clauses so a size-2 LazyCoreNode (initially
  // size() == 0) must be probed across multiple cover steps (sum >= 1..4).
  for (int vy = 0; vy <= 4; ++vy) {
    for (int vz = 0; vz <= 4; ++vz) {
      if (vy + vz >= 4) continue;
      const Literal y_ge = encoder->GetOrCreateAssociatedLiteral(
          IntegerLiteral::GreaterOrEqual(y, IntegerValue(vy + 1)));
      const Literal z_ge = encoder->GetOrCreateAssociatedLiteral(
          IntegerLiteral::GreaterOrEqual(z, IntegerValue(vz + 1)));
      sat_solver->AddProblemClause({y_ge, z_ge});
    }
  }

  model.GetOrCreate<SearchHeuristics>()->fixed_search =
      FirstUnassignedVarAtItsMinHeuristic({x, y, z}, &model);
  ConfigureSearchHeuristics(&model);

  int num_solutions = 0;
  IntegerValue best_solution_obj = kMaxIntegerValue;
  CoreBasedOptimizer core(
      obj, {x, y, z}, {IntegerValue(50), IntegerValue(10), IntegerValue(10)},
      [&model, x, y, z, &num_solutions, &best_solution_obj]() {
        ++num_solutions;
        best_solution_obj =
            IntegerValue(50 * model.Get(Value(x)) + 10 * model.Get(Value(y)) +
                         10 * model.Get(Value(z)));
      },
      &model);

  EXPECT_EQ(core.Optimize(), SatSolver::Status::INFEASIBLE);
  EXPECT_GT(num_solutions, 0);
  EXPECT_EQ(best_solution_obj, IntegerValue(190));
  EXPECT_EQ(model.GetOrCreate<IntegerTrail>()->LowerBound(obj),
            IntegerValue(190));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
