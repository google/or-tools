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

#include "ortools/sat/linear_programming_constraint.h"

#include <stdint.h>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_literal.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

TEST(ScatteredIntegerVectorTest, BasicDenseBehavior) {
  ScatteredIntegerVector v;
  v.ClearAndResize(10);
  v.AddLinearExpressionMultiple(IntegerValue(4), {glop::ColIndex(2)},
                                {IntegerValue(3)}, IntegerValue(3));
  v.AddLinearExpressionMultiple(IntegerValue(3), {glop::ColIndex(1)},
                                {IntegerValue(3)}, IntegerValue(3));
  v.AddLinearExpressionMultiple(IntegerValue(5), {glop::ColIndex(2)},
                                {IntegerValue(3)}, IntegerValue(3));
  const std::vector<std::pair<glop::ColIndex, IntegerValue>> expected{
      {glop::ColIndex(1), IntegerValue(3 * 3)},
      {glop::ColIndex(2), IntegerValue(3 * 4 + 5 * 3)}};
  EXPECT_FALSE(v.IsSparse());
  EXPECT_EQ(v.GetTerms(), expected);
}

TEST(ScatteredIntegerVectorTest, BasicSparseBehavior) {
  ScatteredIntegerVector v;
  v.ClearAndResize(100000);
  v.AddLinearExpressionMultiple(IntegerValue(4), {glop::ColIndex(2)},
                                {IntegerValue(3)}, IntegerValue(3));
  v.AddLinearExpressionMultiple(IntegerValue(3), {glop::ColIndex(1)},
                                {IntegerValue(3)}, IntegerValue(3));
  v.AddLinearExpressionMultiple(IntegerValue(5), {glop::ColIndex(2)},
                                {IntegerValue(3)}, IntegerValue(3));
  const std::vector<std::pair<glop::ColIndex, IntegerValue>> expected{
      {glop::ColIndex(1), IntegerValue(3 * 3)},
      {glop::ColIndex(2), IntegerValue(3 * 4 + 5 * 3)}};
  EXPECT_EQ(v.GetTerms(), expected);
  EXPECT_TRUE(v.IsSparse());
}

TEST(ScatteredIntegerVectorTest, OverflowChecks) {
  constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
  constexpr int64_t kMax = std::numeric_limits<int64_t>::max();

  ScatteredIntegerVector v;
  v.ClearAndResize(100);
  EXPECT_TRUE(v.Add(glop::ColIndex(0), IntegerValue(kMax - 1)));
  // Exact kint64max (2^63 - 1) is accepted, and kint64max + 1 overflows.
  EXPECT_TRUE(v.Add(glop::ColIndex(0), IntegerValue(1)));
  EXPECT_EQ(v[glop::ColIndex(0)], IntegerValue(kMax));
  EXPECT_FALSE(v.Add(glop::ColIndex(0), IntegerValue(1)));

  v.ClearAndResize(100);
  EXPECT_TRUE(v.Add(glop::ColIndex(1), IntegerValue(-(int64_t{1} << 62))));
  EXPECT_TRUE(v.Add(glop::ColIndex(1), IntegerValue(-(int64_t{1} << 62) + 1)));
  EXPECT_EQ(v[glop::ColIndex(1)], IntegerValue(-kMax));
  // Exact kint64min (-2^63) is rejected by AddTo().
  EXPECT_FALSE(v.Add(glop::ColIndex(1), IntegerValue(-1)));

  v.ClearAndResize(100);
  EXPECT_FALSE(
      v.AddLinearExpressionMultiple(IntegerValue(kMin), {glop::ColIndex(0)},
                                    {IntegerValue(1)}, IntegerValue(1)));
  EXPECT_TRUE(v.AddLinearExpressionMultiple(
      IntegerValue(7), {glop::ColIndex(0)}, {IntegerValue(kMax / 7)},
      IntegerValue(kMax / 7)));
  EXPECT_EQ(v[glop::ColIndex(0)], IntegerValue(kMax));
  v.ClearAndResize(100);
  EXPECT_TRUE(v.AddLinearExpressionMultiple(
      IntegerValue(-7), {glop::ColIndex(0)}, {IntegerValue(kMax / 7)},
      IntegerValue(kMax / 7)));
  EXPECT_EQ(v[glop::ColIndex(0)], IntegerValue(-kMax));
  EXPECT_FALSE(v.AddLinearExpressionMultiple(
      IntegerValue(int64_t{1} << 62), {glop::ColIndex(0)}, {IntegerValue(2)},
      IntegerValue(2)));
  v.ClearAndResize(100);
  EXPECT_TRUE(v.AddLinearExpressionMultiple(
      IntegerValue(-(int64_t{1} << 62)), {glop::ColIndex(0)}, {IntegerValue(1)},
      IntegerValue(1)));
  EXPECT_FALSE(v.AddLinearExpressionMultiple(
      IntegerValue(-(int64_t{1} << 62)), {glop::ColIndex(0)}, {IntegerValue(1)},
      IntegerValue(1)));
}

// TODO(user): Check that SAT solutions respect linear equations.
struct LPProblem {
  const std::vector<IntegerValue> integer_lb;
  const std::vector<IntegerValue> integer_ub;
  const std::vector<IntegerValue> constraint_lb;
  const std::vector<IntegerValue> constraint_ub;
  const std::vector<std::vector<int>> constraint_integer_indices;
  const std::vector<std::vector<IntegerValue>> constraint_integer_coefs;
  const std::vector<int> objective_indices;

  int num_integer_vars() const { return integer_lb.size(); }
  int num_constraints() const { return constraint_lb.size(); }
  int num_objectives() const { return objective_indices.size(); }
};

// Generates a problem that encodes a permutation as a bipartite matching
// from size 'left' nodes to size 'right' nodes.
// Decision variables are edges linking left nodes to right nodes.
// For a given node, only one adjacent edge can be present,
// which is encoded by one constraint per node.
LPProblem GeneratePermutationProblem(int size) {
  const std::vector<IntegerValue> edge_lb(size * size, IntegerValue(0));
  const std::vector<IntegerValue> edge_ub(size * size, IntegerValue(1));

  const std::vector<IntegerValue> node_lb(2 * size, IntegerValue(1));
  const std::vector<IntegerValue> node_ub(2 * size, IntegerValue(1));

  std::vector<std::vector<int>> node_constraint_indices;
  std::vector<std::vector<IntegerValue>> node_constraint_coefs;

  std::vector<int> randomized_left;
  std::vector<int> randomized_right;
  for (int i = 0; i < size; i++) {
    randomized_left.push_back(i);
    randomized_right.push_back(i);
  }
  std::shuffle(randomized_left.begin(), randomized_left.end(), absl::BitGen());
  std::shuffle(randomized_right.begin(), randomized_right.end(),
               absl::BitGen());

  // Left and right nodes are indexed by [0, size).
  // The edge (left, right) has number left * size + right.
  const std::vector<IntegerValue> ones(size, IntegerValue(1));
  for (const int left : randomized_left) {
    std::vector<int> indices;
    for (int right = 0; right < size; ++right) {
      indices.push_back(left * size + right);
    }
    node_constraint_indices.push_back(indices);
    node_constraint_coefs.push_back(ones);
  }

  for (const int right : randomized_right) {
    std::vector<int> indices;
    for (int left = 0; left < size; ++left) {
      indices.push_back(left * size + right);
    }
    node_constraint_indices.push_back(indices);
    node_constraint_coefs.push_back(ones);
  }

  return LPProblem{edge_lb,
                   edge_ub,
                   node_lb,
                   node_ub,
                   node_constraint_indices,
                   node_constraint_coefs,
                   {}};
}

int CountSolutionsOfLPProblemUsingSAT(const LPProblem& problem) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_add_lp_constraints_lazily(false);
  model.GetOrCreate<SatParameters>()->set_use_sat_inprocessing(false);
  model.GetOrCreate<SatParameters>()->set_linearization_level(10);

  std::vector<IntegerVariable> cp_variables;
  const int num_cp_vars = problem.num_integer_vars();
  for (int i = 0; i < num_cp_vars; i++) {
    IntegerVariable var = model.Add(NewIntegerVariable(
        problem.integer_lb[i].value(), problem.integer_ub[i].value()));
    cp_variables.push_back(var);
  }

  LinearProgrammingConstraint* lp =
      new LinearProgrammingConstraint(&model, cp_variables);
  model.TakeOwnership(lp);

  const int num_constraints = problem.num_constraints();
  for (int c = 0; c < num_constraints; c++) {
    LinearConstraint ct(problem.constraint_lb[c], problem.constraint_ub[c]);
    const int num_integer = problem.constraint_integer_indices[c].size();
    ct.resize(num_integer);
    for (int j = 0; j < num_integer; j++) {
      ct.vars[j] = cp_variables[problem.constraint_integer_indices[c][j]];
      ct.coeffs[j] = problem.constraint_integer_coefs[c][j];
    }
    lp->AddLinearConstraint(std::move(ct));
  }

  lp->RegisterWith(&model);

  int num_solutions = 0;
  while (SolveIntegerProblemWithLazyEncoding(&model) ==
         SatSolver::Status::FEASIBLE) {
    model.Add(ExcludeCurrentSolutionAndBacktrack());
    num_solutions++;
  }
  return num_solutions;
}

TEST(LinearProgrammingConstraintTest, CountPermutations) {
  int factorial_of_size = 1;
  for (int size = 2; size < 6; size++) {
    factorial_of_size *= size;
    LPProblem problem = GeneratePermutationProblem(size);
    ASSERT_EQ(CountSolutionsOfLPProblemUsingSAT(problem), factorial_of_size);
  }
}

TEST(LinearProgrammingConstraintTest, SimpleInfeasibility) {
  // The following flow is infeasible, LP should detect it.
  //
  //   source
  //   = 2/2        0/2
  // ---------> ---------> A
  //           |           |
  //           |           |
  //       0/2 |           | 0/2
  //           |           |
  //           V           V
  //           B ------->    ------->
  //                0/2       [3,4] = sink
  Model model;

  // We need this parameter at false to detect it on the first propagation.
  model.Add(NewSatParameters("add_lp_constraints_lazily:false"));

  // Variables for the source and sink demands, and flow amounts.
  const IntegerVariable source = model.Add(NewIntegerVariable(2, 2));
  const IntegerVariable source_a = model.Add(NewIntegerVariable(0, 2));
  const IntegerVariable source_b = model.Add(NewIntegerVariable(0, 2));
  const IntegerVariable a_sink = model.Add(NewIntegerVariable(0, 2));
  const IntegerVariable b_sink = model.Add(NewIntegerVariable(0, 2));
  const IntegerVariable sink = model.Add(NewIntegerVariable(3, 4));

  // LP Constraint and flow conservation equalities.
  LinearProgrammingConstraint* lp = new LinearProgrammingConstraint(
      &model, {source, source_a, source_b, a_sink, b_sink, sink});
  model.TakeOwnership(lp);

  LinearConstraintBuilder ct_source(IntegerValue(0), IntegerValue(0));
  ct_source.AddTerm(source, IntegerValue(1));
  ct_source.AddTerm(source_a, IntegerValue(-1));
  ct_source.AddTerm(source_b, IntegerValue(-1));
  lp->AddLinearConstraint(ct_source.Build());

  LinearConstraintBuilder ct_a(IntegerValue(0), IntegerValue(0));
  ct_a.AddTerm(source_a, IntegerValue(1));
  ct_a.AddTerm(a_sink, IntegerValue(-1));
  lp->AddLinearConstraint(ct_a.Build());

  LinearConstraintBuilder ct_b(IntegerValue(0), IntegerValue(0));
  ct_b.AddTerm(source_b, IntegerValue(1));
  ct_b.AddTerm(b_sink, IntegerValue(-1));
  lp->AddLinearConstraint(ct_b.Build());

  LinearConstraintBuilder ct_sink(IntegerValue(0), IntegerValue(0));
  ct_sink.AddTerm(a_sink, IntegerValue(1));
  ct_sink.AddTerm(b_sink, IntegerValue(1));
  ct_sink.AddTerm(sink, IntegerValue(-1));
  lp->AddLinearConstraint(ct_sink.Build());

  lp->RegisterWith(&model);

  ASSERT_FALSE(model.GetOrCreate<SatSolver>()->Propagate());
}

TEST(LinearProgrammingConstraintTest, EmptyLP) {
  Model model;
  model.Add(NewSatParameters("linearization_level:2"));

  LinearProgrammingConstraint* lp = new LinearProgrammingConstraint(&model, {});
  model.TakeOwnership(lp);
  lp->RegisterWith(&model);

  ASSERT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
}

// This tests that the scaling of reduced costs is done correctly,
// using the problem
// min 64 x s.t. 0 <= 27x <= 81, x in [0, 50]
// The reduced cost of x should be 64.
// This will be wrong if the reduced cost ignores the objective scaling.
TEST(LinearProgrammingConstraintTest, ReducedCostScalingObjective) {
  Model m;
  IntegerVariable x = m.Add(NewIntegerVariable(0, 50));
  LinearProgrammingConstraint* lp = new LinearProgrammingConstraint(&m, {x});
  m.TakeOwnership(lp);

  LinearConstraintBuilder ct(IntegerValue(0), IntegerValue(81));
  ct.AddTerm(x, IntegerValue(27));
  lp->AddLinearConstraint(ct.Build());

  IntegerVariable obj = m.Add(NewIntegerVariable(0, 64 * 50));
  lp->SetObjectiveCoefficient(x, IntegerValue(64));
  lp->SetMainObjectiveVariable(obj);

  lp->RegisterWith(&m);
  lp->Propagate();
  const auto& reduced_costs = *m.GetOrCreate<ModelReducedCosts>();
  CHECK_LE(std::abs(reduced_costs[x] - 64), 1e-6);
}

// This tests that the scaling of reduced costs is done correctly,
// using the problem
// min 64 x + 32 y s.t. 0 <= 27x + 9y <= 81, x in [0, 50], y in [0, 20]
// The reduced cost of x should be 64, y should be 20.
// This will be wrong if the reduced cost ignores the column scaling.
TEST(LinearProgrammingConstraintTest, ReducedCostScalingColumns) {
  Model m;
  IntegerVariable x = m.Add(NewIntegerVariable(0, 50));
  IntegerVariable y = m.Add(NewIntegerVariable(0, 20));
  LinearProgrammingConstraint* lp = new LinearProgrammingConstraint(&m, {x, y});
  m.TakeOwnership(lp);

  LinearConstraintBuilder ct(IntegerValue(0), IntegerValue(81));
  ct.AddTerm(x, IntegerValue(27));
  ct.AddTerm(y, IntegerValue(9));
  lp->AddLinearConstraint(ct.Build());

  IntegerVariable obj = m.Add(NewIntegerVariable(0, 64 * 50 + 32 * 20));
  lp->SetObjectiveCoefficient(x, IntegerValue(64));
  lp->SetObjectiveCoefficient(y, IntegerValue(32));
  lp->SetMainObjectiveVariable(obj);

  lp->RegisterWith(&m);
  lp->Propagate();
  const auto& reduced_costs = *m.GetOrCreate<ModelReducedCosts>();
  CHECK_LE(std::abs(reduced_costs[x] - 64), 1e-6);
  CHECK_LE(std::abs(reduced_costs[y] - 32), 1e-6);
}

TEST(LinearProgrammingConstraintTest,
     SubstitutedSingleVariableObjectiveDirectEquivalence) {
  Model m;
  m.GetOrCreate<SatParameters>()->set_add_lp_constraints_lazily(false);
  auto* sat_solver = m.GetOrCreate<SatSolver>();
  const Literal a(sat_solver->NewBooleanVariable(), true);
  const Literal b(sat_solver->NewBooleanVariable(), true);
  const IntegerVariable x = CreateNewIntegerVariableFromLiteral(a, &m);
  const IntegerVariable y = CreateNewIntegerVariableFromLiteral(b, &m);
  const IntegerVariable z = m.Add(NewIntegerVariable(0, 3));

  LinearProgrammingConstraint* lp =
      new LinearProgrammingConstraint(&m, {x, y, z});
  m.TakeOwnership(lp);

  // Register x first so y is substituted by x when a <=> b.
  LinearConstraintBuilder ct1(IntegerValue(3), IntegerValue(6));
  ct1.AddTerm(x, IntegerValue(3));
  ct1.AddTerm(z, IntegerValue(1));
  lp->AddLinearConstraint(ct1.Build());

  LinearConstraintBuilder ct2(IntegerValue(0), IntegerValue(3));
  ct2.AddTerm(y, IntegerValue(3));
  ct2.AddTerm(z, IntegerValue(-1));
  lp->AddLinearConstraint(ct2.Build());

  // Single-variable objective y (which is part of the LP).
  lp->SetObjectiveCoefficient(y, IntegerValue(1));
  lp->SetMainObjectiveVariable(y);

  auto* graph = m.GetOrCreate<BinaryImplicationGraph>();
  graph->AddImplication(a, b);
  graph->AddImplication(b, a);
  ASSERT_TRUE(graph->DetectEquivalences());

  lp->RegisterWith(&m);
  EXPECT_TRUE(lp->Propagate());
  EXPECT_NEAR(lp->GetSolutionValue(y), 0.5, 1e-6);
  EXPECT_EQ(m.GetOrCreate<IntegerTrail>()->LowerBound(y), IntegerValue(1));
}

TEST(LinearProgrammingConstraintTest,
     SubstitutedSingleVariableObjectiveNegatedEquivalence) {
  Model m;
  m.GetOrCreate<SatParameters>()->set_add_lp_constraints_lazily(false);
  auto* sat_solver = m.GetOrCreate<SatSolver>();
  const Literal a(sat_solver->NewBooleanVariable(), true);
  const Literal b(sat_solver->NewBooleanVariable(), true);
  const IntegerVariable x = CreateNewIntegerVariableFromLiteral(a, &m);
  const IntegerVariable y = CreateNewIntegerVariableFromLiteral(b, &m);
  const IntegerVariable z = m.Add(NewIntegerVariable(0, 3));

  LinearProgrammingConstraint* lp =
      new LinearProgrammingConstraint(&m, {x, y, z});
  m.TakeOwnership(lp);

  // Register x first so y is substituted by 1 - x when a <=> not(b).
  LinearConstraintBuilder ct1(IntegerValue(0), IntegerValue(3));
  ct1.AddTerm(x, IntegerValue(-3));
  ct1.AddTerm(z, IntegerValue(1));
  lp->AddLinearConstraint(ct1.Build());

  LinearConstraintBuilder ct2(IntegerValue(0), IntegerValue(3));
  ct2.AddTerm(y, IntegerValue(3));
  ct2.AddTerm(z, IntegerValue(-1));
  lp->AddLinearConstraint(ct2.Build());

  // Single-variable objective y (which is part of the LP).
  lp->SetObjectiveCoefficient(y, IntegerValue(1));
  lp->SetMainObjectiveVariable(y);

  auto* graph = m.GetOrCreate<BinaryImplicationGraph>();
  graph->AddImplication(a, b.Negated());
  graph->AddImplication(b.Negated(), a);
  ASSERT_TRUE(graph->DetectEquivalences());

  lp->RegisterWith(&m);
  EXPECT_TRUE(lp->Propagate());
  EXPECT_NEAR(lp->GetSolutionValue(y), 0.5, 1e-6);
  EXPECT_EQ(m.GetOrCreate<IntegerTrail>()->LowerBound(y), IntegerValue(1));
}

TEST(LinearProgrammingConstraintTest,
     SubstitutedSingleVariableNegatedObjective) {
  Model m;
  m.GetOrCreate<SatParameters>()->set_add_lp_constraints_lazily(false);
  auto* sat_solver = m.GetOrCreate<SatSolver>();
  const Literal a(sat_solver->NewBooleanVariable(), true);
  const Literal b(sat_solver->NewBooleanVariable(), true);
  const IntegerVariable x = CreateNewIntegerVariableFromLiteral(a, &m);
  const IntegerVariable y = CreateNewIntegerVariableFromLiteral(b, &m);
  const IntegerVariable z = m.Add(NewIntegerVariable(0, 3));

  LinearProgrammingConstraint* lp =
      new LinearProgrammingConstraint(&m, {x, y, z});
  m.TakeOwnership(lp);

  // Register x first so y is substituted by x when a <=> b.
  // 3x - z <= 0 and 3y + z <= 3 => 6x <= 3 => x <= 0.5 => -y >= -0.5 => y <= 0.
  LinearConstraintBuilder ct1(IntegerValue(-3), IntegerValue(0));
  ct1.AddTerm(x, IntegerValue(3));
  ct1.AddTerm(z, IntegerValue(-1));
  lp->AddLinearConstraint(ct1.Build());

  LinearConstraintBuilder ct2(IntegerValue(0), IntegerValue(3));
  ct2.AddTerm(y, IntegerValue(3));
  ct2.AddTerm(z, IntegerValue(1));
  lp->AddLinearConstraint(ct2.Build());

  // Single-variable objective -y.
  lp->SetObjectiveCoefficient(y, IntegerValue(-1));
  lp->SetMainObjectiveVariable(NegationOf(y));

  auto* graph = m.GetOrCreate<BinaryImplicationGraph>();
  graph->AddImplication(a, b);
  graph->AddImplication(b, a);
  ASSERT_TRUE(graph->DetectEquivalences());

  lp->RegisterWith(&m);
  EXPECT_TRUE(lp->Propagate());
  EXPECT_NEAR(lp->GetSolutionValue(y), 0.5, 1e-6);
  EXPECT_EQ(m.GetOrCreate<IntegerTrail>()->UpperBound(y), IntegerValue(0));
}

TEST(LinearProgrammingConstraintTest,
     SubstitutedSingleVariableNegatedObjectiveNegatedEquivalence) {
  Model m;
  m.GetOrCreate<SatParameters>()->set_add_lp_constraints_lazily(false);
  auto* sat_solver = m.GetOrCreate<SatSolver>();
  const Literal a(sat_solver->NewBooleanVariable(), true);
  const Literal b(sat_solver->NewBooleanVariable(), true);
  const IntegerVariable x = CreateNewIntegerVariableFromLiteral(a, &m);
  const IntegerVariable y = CreateNewIntegerVariableFromLiteral(b, &m);
  const IntegerVariable z = m.Add(NewIntegerVariable(0, 3));

  LinearProgrammingConstraint* lp =
      new LinearProgrammingConstraint(&m, {x, y, z});
  m.TakeOwnership(lp);

  // Register x first so y is substituted by 1 - x when a <=> not(b).
  // -3x - z <= -3 and 3y + z <= 3 (with y = 1 - x => -3x + z <= 0)
  // => -6x <= -3 => x >= 0.5 => y <= 0.5 => -y >= -0.5 => y <= 0.
  LinearConstraintBuilder ct1(IntegerValue(-6), IntegerValue(-3));
  ct1.AddTerm(x, IntegerValue(-3));
  ct1.AddTerm(z, IntegerValue(-1));
  lp->AddLinearConstraint(ct1.Build());

  LinearConstraintBuilder ct2(IntegerValue(0), IntegerValue(3));
  ct2.AddTerm(y, IntegerValue(3));
  ct2.AddTerm(z, IntegerValue(1));
  lp->AddLinearConstraint(ct2.Build());

  // Single-variable objective -y.
  lp->SetObjectiveCoefficient(y, IntegerValue(-1));
  lp->SetMainObjectiveVariable(NegationOf(y));

  auto* graph = m.GetOrCreate<BinaryImplicationGraph>();
  graph->AddImplication(a, b.Negated());
  graph->AddImplication(b.Negated(), a);
  ASSERT_TRUE(graph->DetectEquivalences());

  lp->RegisterWith(&m);
  EXPECT_TRUE(lp->Propagate());
  EXPECT_NEAR(lp->GetSolutionValue(y), 0.5, 1e-6);
  EXPECT_EQ(m.GetOrCreate<IntegerTrail>()->UpperBound(y), IntegerValue(0));
}

TEST(LinearProgrammingConstraintTest,
     AdjustTightensSubCoefficientRoundingResidualAcrossZero) {
  Model m;
  m.Add(NewSatParameters("add_lp_constraints_lazily:false,cut_level:0"));

  const IntegerVariable x = m.Add(NewIntegerVariable(-10'000, 10'000));
  const IntegerVariable y = m.Add(NewIntegerVariable(0, 100));
  const IntegerVariable z = m.Add(NewIntegerVariable(0, 100));
  const IntegerVariable w = m.Add(NewIntegerVariable(0, 100));
  const IntegerVariable u1 = m.Add(NewIntegerVariable(0, 1));
  const IntegerVariable u2 = m.Add(NewIntegerVariable(0, 1));
  const IntegerVariable u3 = m.Add(NewIntegerVariable(0, 1));
  const IntegerVariable obj = m.Add(NewIntegerVariable(0, 330));

  LinearProgrammingConstraint* lp =
      new LinearProgrammingConstraint(&m, {x, y, z, w, u1, u2, u3});
  m.TakeOwnership(lp);

  // Optimal dual multipliers are (-1/3, -1/8, -1/8). Power-of-two rounding in
  // ScaleMultipliers() leaves a residual |c_x| = 2e7 on basic variable x, which
  // is smaller than Row 2's |coeff(x)| = 3e7 and requires crossing zero to
  // tighten from |c_x| = 2e7 to |c_x| = 1e7 and recover obj >= 30.
  constexpr IntegerValue kTrivialUb(100'000'000'000'000LL);
  LinearConstraintBuilder ct0(IntegerValue(30), kTrivialUb);
  ct0.AddTerm(x, IntegerValue(60'000'000));
  ct0.AddTerm(y, IntegerValue(3));
  ct0.AddTerm(u1, IntegerValue(-1));
  lp->AddLinearConstraint(ct0.Build());

  LinearConstraintBuilder ct1(IntegerValue(80), kTrivialUb);
  ct1.AddTerm(x, IntegerValue(-130'000'000));
  ct1.AddTerm(z, IntegerValue(8));
  ct1.AddTerm(u2, IntegerValue(-1));
  lp->AddLinearConstraint(ct1.Build());

  LinearConstraintBuilder ct2(IntegerValue(80), kTrivialUb);
  ct2.AddTerm(x, IntegerValue(-30'000'000));
  ct2.AddTerm(w, IntegerValue(8));
  ct2.AddTerm(u3, IntegerValue(-1));
  lp->AddLinearConstraint(ct2.Build());

  lp->SetObjectiveCoefficient(y, IntegerValue(1));
  lp->SetObjectiveCoefficient(z, IntegerValue(1));
  lp->SetObjectiveCoefficient(w, IntegerValue(1));
  lp->SetObjectiveCoefficient(u1, IntegerValue(10));
  lp->SetObjectiveCoefficient(u2, IntegerValue(10));
  lp->SetObjectiveCoefficient(u3, IntegerValue(10));
  lp->SetMainObjectiveVariable(obj);

  lp->RegisterWith(&m);
  EXPECT_TRUE(lp->Propagate());
  EXPECT_EQ(lp->num_adjusts(), 1);
  EXPECT_EQ(m.GetOrCreate<IntegerTrail>()->LowerBound(obj), IntegerValue(30));
}

TEST(LinearProgrammingConstraintTest,
     AdjustTightensSingleVariableObjectiveInLp) {
  Model m;
  m.Add(NewSatParameters("add_lp_constraints_lazily:false,cut_level:0"));

  const IntegerVariable x = m.Add(NewIntegerVariable(-20'000, 20'000));
  const IntegerVariable y = m.Add(NewIntegerVariable(0, 100));
  const IntegerVariable z1 = m.Add(NewIntegerVariable(10, 100));
  const IntegerVariable z2 = m.Add(NewIntegerVariable(10, 100));
  const IntegerVariable u1 = m.Add(NewIntegerVariable(0, 1));
  const IntegerVariable u2 = m.Add(NewIntegerVariable(0, 1));
  const IntegerVariable u3 = m.Add(NewIntegerVariable(0, 1));

  LinearProgrammingConstraint* lp =
      new LinearProgrammingConstraint(&m, {x, y, z1, z2, u1, u2, u3});
  m.TakeOwnership(lp);

  // Single-variable objective y is part of the LP: Row 0 touches y and must be
  // kept fixed so y is not cancelled out, while Row 2 adjusts across zero on x
  // to recover the tight lower bound y >= 10.
  constexpr IntegerValue kTrivialUb(100'000'000'000'000LL);
  LinearConstraintBuilder ct0(IntegerValue(31), kTrivialUb);
  ct0.AddTerm(x, IntegerValue(60'000'000));
  ct0.AddTerm(y, IntegerValue(3));
  ct0.AddTerm(u1, IntegerValue(1));
  lp->AddLinearConstraint(ct0.Build());

  LinearConstraintBuilder ct1(IntegerValue(-79), kTrivialUb);
  ct1.AddTerm(x, IntegerValue(-130'000'000));
  ct1.AddTerm(z1, IntegerValue(-8));
  ct1.AddTerm(u2, IntegerValue(1));
  lp->AddLinearConstraint(ct1.Build());

  LinearConstraintBuilder ct2(IntegerValue(-79), kTrivialUb);
  ct2.AddTerm(x, IntegerValue(-30'000'000));
  ct2.AddTerm(z2, IntegerValue(-8));
  ct2.AddTerm(u3, IntegerValue(1));
  lp->AddLinearConstraint(ct2.Build());

  lp->SetObjectiveCoefficient(y, IntegerValue(1));
  lp->SetMainObjectiveVariable(y);

  lp->RegisterWith(&m);
  EXPECT_TRUE(lp->Propagate());
  EXPECT_EQ(lp->num_adjusts(), 1);
  EXPECT_EQ(m.GetOrCreate<IntegerTrail>()->LowerBound(y), IntegerValue(10));
}

TEST(LinearProgrammingConstraintTest,
     AdjustPreservesExactPrecisionAboveDoubleMantissa) {
  Model m;
  m.Add(NewSatParameters("add_lp_constraints_lazily:false,cut_level:0"));

  const IntegerVariable x = m.Add(
      NewIntegerVariable(10'000'000'000'000'001LL, 10'000'000'000'000'010LL));
  const IntegerVariable y = m.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = m.Add(NewIntegerVariable(0, 10));
  const IntegerVariable u1 = m.Add(NewIntegerVariable(0, 1));
  const IntegerVariable obj = m.Add(NewIntegerVariable(0, 7));

  LinearProgrammingConstraint* lp =
      new LinearProgrammingConstraint(&m, {x, y, z, u1});
  m.TakeOwnership(lp);

  // Bounds exceed 2^53 (~9.0e15), where IEEE-754 double loses unit precision:
  // 3y >= 3x - u1 - 29'999'999'999'999'987 >= 3*(10^16 + 1) - 1 - (3*10^16-13)
  //     = 15 => y >= 5, and z >= 2 => obj = y + z >= 7.
  LinearConstraintBuilder ct0(IntegerValue(-29'999'999'999'999'987LL),
                              IntegerValue(100));
  ct0.AddTerm(x, IntegerValue(-3));
  ct0.AddTerm(y, IntegerValue(3));
  ct0.AddTerm(u1, IntegerValue(1));
  lp->AddLinearConstraint(ct0.Build());

  LinearConstraintBuilder ct1(IntegerValue(2), IntegerValue(100));
  ct1.AddTerm(z, IntegerValue(1));
  lp->AddLinearConstraint(ct1.Build());

  lp->SetObjectiveCoefficient(y, IntegerValue(1));
  lp->SetObjectiveCoefficient(z, IntegerValue(1));
  lp->SetMainObjectiveVariable(obj);

  lp->RegisterWith(&m);
  EXPECT_TRUE(lp->Propagate());
  EXPECT_EQ(m.GetOrCreate<IntegerTrail>()->LowerBound(obj), IntegerValue(7));
  EXPECT_EQ(m.GetOrCreate<IntegerTrail>()->LowerBound(y), IntegerValue(5));
}

TEST(LinearProgrammingConstraintTest,
     PropagateExactDualRayProvesConflictAcrossZero) {
  Model m;
  m.Add(NewSatParameters("add_lp_constraints_lazily:false,cut_level:0"));

  const IntegerVariable x1 = m.Add(NewIntegerVariable(-600, 600));
  const IntegerVariable x2 = m.Add(NewIntegerVariable(-10, 10));
  const IntegerVariable y = m.Add(NewIntegerVariable(1, 10));
  const IntegerVariable z = m.Add(NewIntegerVariable(1, 10));
  const IntegerVariable w = m.Add(NewIntegerVariable(1, 10));
  const IntegerVariable u1 = m.Add(NewIntegerVariable(0, 1));
  const IntegerVariable u2 = m.Add(NewIntegerVariable(0, 1));
  const IntegerVariable u3 = m.Add(NewIntegerVariable(0, 1));

  LinearProgrammingConstraint* lp =
      new LinearProgrammingConstraint(&m, {x1, x2, y, z, w, u1, u2, u3});
  m.TakeOwnership(lp);

  // Any two constraints are feasible in R^2, while (1/3)*ct0 + (1/8)*ct1 +
  // (1/8)*ct2 gives y + z + w + (1/3)u1 + (1/8)u2 + (1/8)u3 <= 2.75, which is
  // infeasible for y, z, w >= 1. Power-of-two dual ray scaling leaves |c_x1| =
  // 2e7 on x1 in [-600, 600] (losing 1.2e10 > 0.25*S of slack without
  // adjustment); crossing zero on ct2 reduces |c_x1| to 1e7 (6e9 < 0.25*S) and
  // proves exact infeasibility.
  constexpr IntegerValue kTrivialLb(-100'000'000'000'000LL);
  LinearConstraintBuilder ct0(kTrivialLb, IntegerValue(3));
  ct0.AddTerm(x1, IntegerValue(60'000'000));
  ct0.AddTerm(x2, IntegerValue(60'000'000));
  ct0.AddTerm(y, IntegerValue(3));
  ct0.AddTerm(u1, IntegerValue(1));
  lp->AddLinearConstraint(ct0.Build());

  LinearConstraintBuilder ct1(kTrivialLb, IntegerValue(7));
  ct1.AddTerm(x1, IntegerValue(-130'000'000));
  ct1.AddTerm(x2, IntegerValue(-160'000'000));
  ct1.AddTerm(z, IntegerValue(8));
  ct1.AddTerm(u2, IntegerValue(1));
  lp->AddLinearConstraint(ct1.Build());

  LinearConstraintBuilder ct2(kTrivialLb, IntegerValue(7));
  ct2.AddTerm(x1, IntegerValue(-30'000'000));
  ct2.AddTerm(w, IntegerValue(8));
  ct2.AddTerm(u3, IntegerValue(1));
  lp->AddLinearConstraint(ct2.Build());

  lp->RegisterWith(&m);
  EXPECT_FALSE(lp->Propagate());
  EXPECT_EQ(lp->num_adjusts(), 1);
}

TEST(LinearProgrammingConstraintTest,
     AdjustClampsMultiplierStepToAvoidInt64Overflow) {
  Model m;
  m.Add(NewSatParameters("add_lp_constraints_lazily:false,cut_level:0"));

  const IntegerVariable x = m.Add(NewIntegerVariable(1, 2));
  const IntegerVariable z = m.Add(NewIntegerVariable(-100, 100));

  LinearProgrammingConstraint* lp = new LinearProgrammingConstraint(&m, {x, z});
  m.TakeOwnership(lp);

  // Neither constraint alone is infeasible on x in [1, 2], z in [-100, 100],
  // while 3 * ct0 + 1 * ct1 gives 2 * x <= -10 (infeasible). Power-of-two
  // scaling of the [1, 1/3] dual ray gives large coprime multipliers (m0, m1)
  // with current[x] = m0 - m1 < m0 and row_bound(ct0) = 0; clamping pos_limit
  // by kMaxWantedCoeff - m0 prevents m0 + to_add from overflowing int64_t.
  constexpr IntegerValue kTrivialLb(-100'000'000'000'000LL);
  LinearConstraintBuilder ct0(kTrivialLb, IntegerValue(0));
  ct0.AddTerm(x, IntegerValue(1));
  ct0.AddTerm(z, IntegerValue(-1));
  lp->AddLinearConstraint(ct0.Build());

  LinearConstraintBuilder ct1(kTrivialLb, IntegerValue(-10));
  ct1.AddTerm(x, IntegerValue(-1));
  ct1.AddTerm(z, IntegerValue(3));
  lp->AddLinearConstraint(ct1.Build());

  lp->RegisterWith(&m);
  EXPECT_FALSE(lp->Propagate());
}

TEST(LinearProgrammingConstraintTest, MirCutWithImpliedBoundsAndLpSlack) {
  auto solve_root_lb = [](int cut_level, bool use_implied_bounds = true) {
    Model m;
    auto* params = m.GetOrCreate<SatParameters>();
    params->set_add_lp_constraints_lazily(false);
    params->set_linearization_level(10);
    params->set_cut_level(cut_level);
    params->set_max_cut_rounds_at_level_zero(5);
    params->set_use_implied_bounds(use_implied_bounds);
    params->set_max_all_diff_cut_size(0);

    auto* sat_solver = m.GetOrCreate<SatSolver>();
    auto* implied_bounds = m.GetOrCreate<ImpliedBounds>();

    std::vector<IntegerVariable> x_vars;
    std::vector<IntegerVariable> z_vars;
    for (int i = 0; i < 3; ++i) {
      const BooleanVariable b = sat_solver->NewBooleanVariable();
      const IntegerVariable z =
          CreateNewIntegerVariableFromLiteral(Literal(b, true), &m);
      const IntegerVariable x = m.Add(NewIntegerVariable(0, 500000));
      implied_bounds->Add(Literal(b, true), IntegerLiteral::GreaterOrEqual(
                                                x, IntegerValue(25000)));
      implied_bounds->Add(Literal(b, false), IntegerLiteral::LowerOrEqual(
                                                 x, IntegerValue(100000)));
      x_vars.push_back(x);
      z_vars.push_back(z);
    }
    const IntegerVariable s = m.Add(NewIntegerVariable(0, 500000));
    const IntegerVariable obj = m.Add(NewIntegerVariable(0, 300));

    std::vector<IntegerVariable> lp_vars = {x_vars[0], x_vars[1], x_vars[2], s,
                                            z_vars[0], z_vars[1], z_vars[2]};
    std::sort(lp_vars.begin(), lp_vars.end());
    LinearProgrammingConstraint* lp =
        new LinearProgrammingConstraint(&m, lp_vars);
    m.TakeOwnership(lp);

    // Minimize 100*(z0 + z1 + z2) subject to x_i <= 100000 + 400000*z_i and
    // the ranged demand constraint 850000 <= x0 + x1 + x2 - s <= 1500000.
    // The LP relaxation sets x = (500000, 250000, 100000), z = (1, 0.375, 0),
    // s = 0 with objective 137.5, whereas at least two z_i must be 1 in any
    // integer solution (objective 200).
    for (int i = 0; i < 3; ++i) {
      LinearConstraintBuilder ub_ct(kMinIntegerValue, IntegerValue(100000));
      ub_ct.AddTerm(x_vars[i], IntegerValue(1));
      ub_ct.AddTerm(z_vars[i], IntegerValue(-400000));
      lp->AddLinearConstraint(ub_ct.Build());
    }
    LinearConstraintBuilder demand_ct(IntegerValue(850000),
                                      IntegerValue(1500000));
    demand_ct.AddTerm(x_vars[0], IntegerValue(1));
    demand_ct.AddTerm(x_vars[1], IntegerValue(1));
    demand_ct.AddTerm(x_vars[2], IntegerValue(1));
    demand_ct.AddTerm(s, IntegerValue(-1));
    lp->AddLinearConstraint(demand_ct.Build());

    for (int i = 0; i < 3; ++i) {
      lp->SetObjectiveCoefficient(z_vars[i], IntegerValue(100));
    }
    lp->SetMainObjectiveVariable(obj);

    lp->RegisterWith(&m);
    for (int round = 0; round < 5; ++round) {
      CHECK(sat_solver->Propagate());
    }
    return m.GetOrCreate<IntegerTrail>()->LowerBound(obj);
  };

  EXPECT_EQ(solve_root_lb(/*cut_level=*/0), IntegerValue(138));
  EXPECT_EQ(solve_root_lb(/*cut_level=*/1, /*use_implied_bounds=*/false),
            IntegerValue(200));
  EXPECT_EQ(solve_root_lb(/*cut_level=*/1, /*use_implied_bounds=*/true),
            IntegerValue(200));
}

TEST(LinearProgrammingConstraintTest,
     ExactLpReasonAbortsCleanlyWhenObjectiveMergeOverflows) {
  Model m;
  m.Add(NewSatParameters("add_lp_constraints_lazily:false,cut_level:0"));

  const IntegerVariable y = m.Add(NewIntegerVariable(0, 10'000));
  const IntegerVariable w = m.Add(NewIntegerVariable(0, 10'000));
  const IntegerVariable u = m.Add(NewIntegerVariable(0, 1));

  LinearProgrammingConstraint* lp =
      new LinearProgrammingConstraint(&m, {y, w, u});
  m.TakeOwnership(lp);

  // Optimal LP solution for min(y + w) s.t. -3072*y + 3072*w + u >= 2 is
  // u = 1, y = 0, w = 1/3072 with dual 1/3072 = (1/3) * 2^-10. Because
  // IEEE-754 double(1/3) rounds strictly down, at scale 2^62 we get the exact
  // odd 53-bit integer double(1/3072) * 2^62 = 1501199875790165 (gcd = 1, so
  // obj_scale = 2^62) and 2^62 + 3072 * 1501199875790165 = 2^63 - 1024 <
  // kMaxIntegerValue. Merging +obj_scale on y (from NegationOf(y)) into
  // sv[y] = 2^63 - 1024 overflows int64_t and aborts cleanly (returning true
  // without propagating the conflict y + w <= -y).
  constexpr IntegerValue kTrivialUb(100'000'000'000'000LL);
  LinearConstraintBuilder ct0(IntegerValue(2), kTrivialUb);
  ct0.AddTerm(y, IntegerValue(-3072));
  ct0.AddTerm(w, IntegerValue(3072));
  ct0.AddTerm(u, IntegerValue(1));
  lp->AddLinearConstraint(ct0.Build());

  lp->SetObjectiveCoefficient(y, IntegerValue(1));
  lp->SetObjectiveCoefficient(w, IntegerValue(1));
  lp->SetMainObjectiveVariable(NegationOf(y));

  lp->RegisterWith(&m);
  EXPECT_TRUE(lp->Propagate());
  EXPECT_EQ(m.GetOrCreate<IntegerTrail>()->LowerBound(NegationOf(y)),
            IntegerValue(-10'000));
}
}  // namespace
}  // namespace sat
}  // namespace operations_research
