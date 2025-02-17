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

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
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

  // Left and right nodes are indexed by [0, size).
  // The edge (left, right) has number left * size + right.
  const std::vector<IntegerValue> ones(size, IntegerValue(1));
  for (int left = 0; left < size; left++) {
    std::vector<int> indices;
    for (int right = 0; right < size; right++) {
      indices.push_back(left * size + right);
    }
    node_constraint_indices.push_back(indices);
    node_constraint_coefs.push_back(ones);
  }

  for (int right = 0; right < size; right++) {
    std::vector<int> indices;
    for (int left = 0; left < size; left++) {
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

}  // namespace
}  // namespace sat
}  // namespace operations_research
