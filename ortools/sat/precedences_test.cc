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

#include "ortools/sat/precedences.h"

#include <algorithm>
#include <numeric>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_solver_helpers.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

// A simple macro to make the code more readable.
// TODO(user): move that in a common place. test_utils?
#define EXPECT_BOUNDS_EQ(var, lb, ub)            \
  EXPECT_EQ(integer_trail->LowerBound(var), lb); \
  EXPECT_EQ(integer_trail->LevelZeroUpperBound(var), ub)

// All the tests here uses 10 integer variables initially in [0, 100].
std::vector<IntegerVariable> AddVariables(IntegerTrail* integer_trail) {
  std::vector<IntegerVariable> vars;
  const int num_variables = 10;
  const IntegerValue lower_bound(0);
  const IntegerValue upper_bound(100);
  for (int i = 0; i < num_variables; ++i) {
    vars.push_back(integer_trail->AddIntegerVariable(lower_bound, upper_bound));
  }
  return vars;
}

TEST(EnforcedLinear2BoundsTest, BasicAPI) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* root_bounds = model.GetOrCreate<RootLevelLinear2Bounds>();
  auto* precedence_builder =
      model.GetOrCreate<TransitivePrecedencesEvaluator>();
  const std::vector<IntegerVariable> vars = AddVariables(integer_trail);

  // Note that odd indices are for the negation.
  IntegerVariable a(0), b(2), c(4), d(6);

  root_bounds->AddUpperBound(LinearExpression2::Difference(a, b), -10);
  root_bounds->AddUpperBound(LinearExpression2::Difference(d, c), -7);
  root_bounds->AddUpperBound(LinearExpression2::Difference(b, d), -5);

  precedence_builder->Build();
  EXPECT_EQ(
      root_bounds->LevelZeroUpperBound(LinearExpression2::Difference(a, b)),
      -10);
  EXPECT_EQ(root_bounds->LevelZeroUpperBound(
                LinearExpression2::Difference(NegationOf(b), NegationOf(a))),
            -10);
  EXPECT_EQ(
      root_bounds->LevelZeroUpperBound(LinearExpression2::Difference(a, c)),
      -22);
  EXPECT_EQ(root_bounds->LevelZeroUpperBound(
                LinearExpression2::Difference(NegationOf(c), NegationOf(a))),
            -22);
  EXPECT_EQ(
      root_bounds->LevelZeroUpperBound(LinearExpression2::Difference(a, d)),
      -15);
  EXPECT_EQ(root_bounds->LevelZeroUpperBound(
                LinearExpression2::Difference(NegationOf(d), NegationOf(a))),
            -15);
  EXPECT_EQ(
      root_bounds->LevelZeroUpperBound(LinearExpression2::Difference(d, a)),
      100);

  // Once built, we can update the offsets.
  // Note however that this would not propagate through the precedence graphs.
  root_bounds->AddUpperBound(LinearExpression2::Difference(a, b), -15);
  EXPECT_EQ(
      root_bounds->LevelZeroUpperBound(LinearExpression2::Difference(a, b)),
      -15);
  EXPECT_EQ(root_bounds->LevelZeroUpperBound(
                LinearExpression2::Difference(NegationOf(b), NegationOf(a))),
            -15);
}

TEST(EnforcedLinear2BoundsTest, CornerCase1) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* root_bounds = model.GetOrCreate<RootLevelLinear2Bounds>();
  auto* precedence_builder =
      model.GetOrCreate<TransitivePrecedencesEvaluator>();
  const std::vector<IntegerVariable> vars = AddVariables(integer_trail);

  // Note that odd indices are for the negation.
  IntegerVariable a(0), b(2), c(4), d(6);

  root_bounds->AddUpperBound(LinearExpression2::Difference(a, b), -10);
  root_bounds->AddUpperBound(LinearExpression2::Difference(b, c), -7);
  root_bounds->AddUpperBound(LinearExpression2::Difference(b, d), -5);
  root_bounds->AddUpperBound(LinearExpression2::Difference(NegationOf(b), a),
                             -5);

  precedence_builder->Build();
  EXPECT_EQ(root_bounds->LevelZeroUpperBound(
                LinearExpression2::Difference(NegationOf(b), a)),
            -5);
  EXPECT_EQ(root_bounds->LevelZeroUpperBound(
                LinearExpression2::Difference(NegationOf(b), c)),
            -22);
  EXPECT_EQ(root_bounds->LevelZeroUpperBound(
                LinearExpression2::Difference(NegationOf(b), d)),
            -20);
}

TEST(EnforcedLinear2BoundsTest, CornerCase2) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* root_bounds = model.GetOrCreate<RootLevelLinear2Bounds>();
  auto* precedence_builder =
      model.GetOrCreate<TransitivePrecedencesEvaluator>();
  const std::vector<IntegerVariable> vars = AddVariables(integer_trail);

  // Note that odd indices are for the negation.
  IntegerVariable a(0), b(2), c(4), d(6);

  root_bounds->AddUpperBound(LinearExpression2::Difference(NegationOf(a), a),
                             -10);
  root_bounds->AddUpperBound(LinearExpression2::Difference(a, b), -7);
  root_bounds->AddUpperBound(LinearExpression2::Difference(a, c), -5);
  root_bounds->AddUpperBound(LinearExpression2::Difference(a, d), -2);
  EXPECT_EQ(root_bounds->LevelZeroUpperBound(
                LinearExpression2::Difference(NegationOf(b), NegationOf(a))),
            -7);

  precedence_builder->Build();
}

TEST(EnforcedLinear2BoundsTest, CoefficientGreaterThanOne) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* root_bounds = model.GetOrCreate<RootLevelLinear2Bounds>();
  auto* precedence_builder =
      model.GetOrCreate<TransitivePrecedencesEvaluator>();
  const std::vector<IntegerVariable> vars = AddVariables(integer_trail);

  // Note that odd indices are for the negation.
  IntegerVariable a(0), b(2), c(4);

  EnforcedLinear2Bounds precedences(&model);
  root_bounds->AddUpperBound(LinearExpression2(a, b, 3, -4), 7);
  root_bounds->AddUpperBound(LinearExpression2(a, c, 2, -3), -5);
  root_bounds->AddUpperBound(LinearExpression2(a, b, 6, -8), 5);
  EXPECT_EQ(root_bounds->LevelZeroUpperBound(LinearExpression2(a, b, 9, -12)),
            6);

  precedence_builder->Build();
}

TEST(EnforcedLinear2BoundsTest, ConditionalRelations) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* lin2_bounds = model.GetOrCreate<Linear2Bounds>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* precedences = model.GetOrCreate<EnforcedLinear2Bounds>();
  auto* lin2_indices = model.GetOrCreate<Linear2Indices>();
  const std::vector<IntegerVariable> vars = AddVariables(integer_trail);

  const Literal l(model.Add(NewBooleanVariable()), true);
  EXPECT_TRUE(sat_solver->EnqueueDecisionIfNotConflicting(l));

  // Note that odd indices are for the negation.
  IntegerVariable a(0), b(2);
  precedences->PushConditionalRelation({l}, LinearExpression2(a, b, 1, 1), 15);
  precedences->PushConditionalRelation({l}, LinearExpression2(a, b, 1, 1), 20);

  LinearExpression2 expr_a_plus_b =
      LinearExpression2::Difference(a, NegationOf(b));
  expr_a_plus_b.SimpleCanonicalization();
  // We only keep the best one.
  EXPECT_EQ(lin2_bounds->UpperBound(expr_a_plus_b), 15);
  std::vector<Literal> literal_reason;
  std::vector<IntegerLiteral> integer_reason;
  precedences->AddReasonForUpperBoundLowerThan(
      lin2_indices->AddOrGet(expr_a_plus_b), 15, &literal_reason,
      &integer_reason);
  EXPECT_THAT(literal_reason, ElementsAre(l.Negated()));

  // Backtrack works.
  EXPECT_TRUE(sat_solver->ResetToLevelZero());
  EXPECT_EQ(lin2_bounds->UpperBound(expr_a_plus_b), 200);
  literal_reason.clear();
  integer_reason.clear();
  precedences->AddReasonForUpperBoundLowerThan(
      lin2_indices->AddOrGet(expr_a_plus_b), kMaxIntegerValue, &literal_reason,
      &integer_reason);
  EXPECT_THAT(literal_reason, IsEmpty());
}

TEST(PrecedencesPropagatorTest, Empty) {
  Model model;
  Trail* trail = model.GetOrCreate<Trail>();
  PrecedencesPropagator* propagator =
      model.GetOrCreate<PrecedencesPropagator>();
  EXPECT_TRUE(propagator->Propagate(trail));
  EXPECT_TRUE(propagator->Propagate(trail));
  propagator->Untrail(*trail, 0);
}

TEST(PrecedencesPropagatorTest, BasicPropagationTest) {
  Model model;
  Trail* trail = model.GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  PrecedencesPropagator* propagator =
      model.GetOrCreate<PrecedencesPropagator>();

  std::vector<IntegerVariable> vars = AddVariables(integer_trail);
  propagator->AddPrecedenceWithOffset(vars[0], vars[1], IntegerValue(4));
  propagator->AddPrecedenceWithOffset(vars[0], vars[2], IntegerValue(8));
  propagator->AddPrecedenceWithOffset(vars[1], vars[2], IntegerValue(10));

  EXPECT_TRUE(propagator->Propagate(trail));
  EXPECT_BOUNDS_EQ(vars[0], 0, 86);
  EXPECT_BOUNDS_EQ(vars[1], 4, 90);
  EXPECT_BOUNDS_EQ(vars[2], 14, 100);

  // Lets now move vars[1] lower bound.
  std::vector<Literal> lr;
  std::vector<IntegerLiteral> ir;
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(vars[1], IntegerValue(20)), lr, ir));

  EXPECT_TRUE(propagator->Propagate(trail));
  EXPECT_BOUNDS_EQ(vars[1], 20, 90);
  EXPECT_BOUNDS_EQ(vars[2], 30, 100);
}

TEST(PrecedencesPropagatorTest, PropagationTestWithVariableOffset) {
  Model model;
  Trail* trail = model.GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  PrecedencesPropagator* propagator =
      model.GetOrCreate<PrecedencesPropagator>();

  std::vector<IntegerVariable> vars = AddVariables(integer_trail);
  propagator->AddPrecedenceWithVariableOffset(vars[0], vars[1], vars[2]);

  // Make var[2] >= 10 and propagate
  std::vector<Literal> lr;
  std::vector<IntegerLiteral> ir;
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(vars[2], IntegerValue(10)), lr, ir));
  EXPECT_TRUE(propagator->Propagate(trail));
  EXPECT_BOUNDS_EQ(vars[0], 0, 90);
  EXPECT_BOUNDS_EQ(vars[1], 10, 100);

  // Change the lower bound to 40 and propagate again.
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(vars[2], IntegerValue(40)), lr, ir));
  EXPECT_TRUE(propagator->Propagate(trail));
  EXPECT_BOUNDS_EQ(vars[0], 0, 60);
  EXPECT_BOUNDS_EQ(vars[1], 40, 100);
}

TEST(PrecedencesPropagatorTest, BasicPropagation) {
  Model model;
  Trail* trail = model.GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  PrecedencesPropagator* propagator =
      model.GetOrCreate<PrecedencesPropagator>();
  trail->Resize(10);

  std::vector<IntegerVariable> vars = AddVariables(integer_trail);
  propagator->AddPrecedenceWithOffset(vars[0], vars[1], IntegerValue(4));
  propagator->AddPrecedenceWithOffset(vars[1], vars[2], IntegerValue(8));
  propagator->AddPrecedenceWithOffset(vars[0], vars[3], IntegerValue(90));

  // These arcs are not possible, because the upper bound of vars[0] is 10.
  propagator->AddConditionalPrecedenceWithOffset(vars[1], vars[0],
                                                 IntegerValue(7), Literal(+1));
  propagator->AddConditionalPrecedenceWithOffset(vars[2], vars[0],
                                                 IntegerValue(-1), Literal(+2));

  // These are is ok.
  propagator->AddConditionalPrecedenceWithOffset(vars[1], vars[0],
                                                 IntegerValue(6), Literal(+3));
  propagator->AddConditionalPrecedenceWithOffset(vars[2], vars[0],
                                                 IntegerValue(-2), Literal(+4));

  EXPECT_TRUE(propagator->Propagate(trail));
  EXPECT_TRUE(trail->Assignment().LiteralIsFalse(Literal(+1)));
  EXPECT_TRUE(trail->Assignment().LiteralIsFalse(Literal(+2)));
  EXPECT_FALSE(trail->Assignment().VariableIsAssigned(Literal(+3).Variable()));
  EXPECT_FALSE(trail->Assignment().VariableIsAssigned(Literal(+4).Variable()));
}

TEST(PrecedencesPropagatorTest, PropagateOnVariableOffset) {
  Model model;
  Trail* trail = model.GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  PrecedencesPropagator* propagator =
      model.GetOrCreate<PrecedencesPropagator>();
  trail->Resize(10);

  std::vector<IntegerVariable> vars = AddVariables(integer_trail);
  propagator->AddPrecedenceWithVariableOffset(vars[0], vars[1], vars[2]);
  propagator->AddPrecedenceWithOffset(vars[1], vars[3], IntegerValue(50));

  EXPECT_TRUE(propagator->Propagate(trail));
  EXPECT_BOUNDS_EQ(vars[0], 0, 50);
  EXPECT_BOUNDS_EQ(vars[1], 0, 50);
  EXPECT_BOUNDS_EQ(vars[2], 0, 50);
}

TEST(PrecedencesPropagatorTest, Cycles) {
  Model model;
  Trail* trail = model.GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  PrecedencesPropagator* propagator =
      model.GetOrCreate<PrecedencesPropagator>();
  trail->Resize(10);

  std::vector<IntegerVariable> vars = AddVariables(integer_trail);
  propagator->AddPrecedenceWithOffset(vars[0], vars[1], IntegerValue(4));
  propagator->AddPrecedenceWithOffset(vars[1], vars[2], IntegerValue(8));
  propagator->AddConditionalPrecedenceWithOffset(
      vars[2], vars[3], IntegerValue(-10), Literal(+1));
  propagator->AddConditionalPrecedenceWithOffset(vars[3], vars[0],
                                                 IntegerValue(-2), Literal(+2));
  propagator->AddConditionalPrecedence(vars[3], vars[0], Literal(+3));

  // This one will force the upper bound of vars[0] to be 50, so we can
  // check that the cycle is detected before the lower bound of var[0] crosses
  // this bound.
  propagator->AddConditionalPrecedenceWithOffset(vars[0], vars[4],
                                                 IntegerValue(50), Literal(+4));

  // If we add this one, the cycle will be detected using the integer bound and
  // not the graph cycle. TODO(user): Maybe this is a bad thing? but it seems
  // difficult to avoid it without extra computations.
  propagator->AddConditionalPrecedenceWithOffset(vars[0], vars[4],
                                                 IntegerValue(99), Literal(+5));

  EXPECT_TRUE(propagator->Propagate(trail));

  // Cycle of weight zero is fine.
  trail->SetDecisionLevel(1);
  EXPECT_TRUE(integer_trail->Propagate(trail));
  trail->Enqueue(Literal(+1), AssignmentType::kUnitReason);
  trail->Enqueue(Literal(+2), AssignmentType::kUnitReason);
  trail->Enqueue(Literal(+4), AssignmentType::kUnitReason);
  EXPECT_TRUE(propagator->Propagate(trail));

  // But a cycle of positive length is not!
  trail->Enqueue(Literal(+3), AssignmentType::kUnitReason);
  EXPECT_FALSE(propagator->Propagate(trail));
  EXPECT_THAT(trail->FailingClause(),
              UnorderedElementsAre(Literal(-1), Literal(-3)));

  // Test the untrail.
  trail->SetDecisionLevel(0);
  integer_trail->Untrail(*trail, 0);
  propagator->Untrail(*trail, 0);
  trail->Untrail(0);
  EXPECT_TRUE(propagator->Propagate(trail));

  // Still fine here.
  trail->SetDecisionLevel(1);
  EXPECT_TRUE(integer_trail->Propagate(trail));
  trail->Enqueue(Literal(+5), AssignmentType::kUnitReason);
  EXPECT_TRUE(propagator->Propagate(trail));

  // But fail there with a different and longer reason.
  trail->Enqueue(Literal(+1), AssignmentType::kUnitReason);
  trail->Enqueue(Literal(+3), AssignmentType::kUnitReason);
  EXPECT_FALSE(propagator->Propagate(trail));
  EXPECT_THAT(trail->FailingClause(),
              UnorderedElementsAre(Literal(-1), Literal(-3), Literal(-5)));
}

// This test a tricky situation:
//
// vars[0] + (offset = vars[2]) <= var[1]
// vars[1] <= vars[2] !!
TEST(PrecedencesPropagatorTest, TrickyCycle) {
  Model model;
  Trail* trail = model.GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  PrecedencesPropagator* propagator =
      model.GetOrCreate<PrecedencesPropagator>();
  trail->Resize(10);

  std::vector<IntegerVariable> vars = AddVariables(integer_trail);
  propagator->AddPrecedenceWithVariableOffset(vars[0], vars[1], vars[2]);
  propagator->AddPrecedence(vars[1], vars[2]);

  // This will cause an infinite cycle.
  propagator->AddConditionalPrecedenceWithOffset(vars[3], vars[0],
                                                 IntegerValue(1), Literal(+1));

  // So far so good.
  EXPECT_TRUE(propagator->Propagate(trail));
  trail->SetDecisionLevel(1);
  EXPECT_TRUE(integer_trail->Propagate(trail));

  // Conflict.
  trail->Enqueue(Literal(+1), AssignmentType::kUnitReason);
  EXPECT_FALSE(propagator->Propagate(trail));
  EXPECT_THAT(trail->FailingClause(), ElementsAre(Literal(-1)));

  // Test that the code detected properly a positive cycle in the dependency
  // graph instead of just pushing the bounds until the upper bound is reached.
  EXPECT_LT(integer_trail->num_enqueues(), 10);
}

TEST(PrecedencesPropagatorTest, ZeroWeightCycleOnDiscreteDomain) {
  Model model;
  IntegerVariable a = model.Add(
      NewIntegerVariable(Domain::FromValues({2, 5, 7, 15, 16, 17, 20, 32})));
  IntegerVariable b = model.Add(
      NewIntegerVariable(Domain::FromValues({3, 6, 9, 14, 16, 18, 20, 35})));

  // Add the fact that a == b with two inequalities.
  auto* precedences = model.GetOrCreate<PrecedencesPropagator>();
  precedences->AddPrecedence(a, b);
  precedences->AddPrecedence(b, a);

  // After propagation, we should detect that the only common values fall in
  // [16, 20].
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());

  // The integer_trail is only used in the macros below.
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  EXPECT_BOUNDS_EQ(a, 16, 20);
  EXPECT_BOUNDS_EQ(b, 16, 20);
}

// This was failing before CL 135903015.
TEST(PrecedencesPropagatorTest, ConditionalPrecedencesOnFixedLiteral) {
  Model model;

  // To trigger the old bug, we need to add some precedences.
  IntegerVariable x = model.Add(NewIntegerVariable(0, 100));
  IntegerVariable y = model.Add(NewIntegerVariable(50, 100));
  auto* precedences = model.GetOrCreate<PrecedencesPropagator>();
  precedences->AddPrecedence(x, y);

  // We then add a Boolean variable and fix it.
  // This will trigger a propagation.
  BooleanVariable b = model.Add(NewBooleanVariable());
  model.Add(ClauseConstraint({Literal(b, true)}));  // Fix b To true.

  // We now add a conditional precedences using the fixed variable.
  // This used to not be taken into account.
  model.Add(ConditionalLowerOrEqualWithOffset(y, x, 0, Literal(b, true)));

  EXPECT_EQ(SatSolver::FEASIBLE, SolveIntegerProblemWithLazyEncoding(&model));
  EXPECT_EQ(model.Get(Value(x)), model.Get(Value(y)));
}

#undef EXPECT_BOUNDS_EQ

TEST(EnforcedLinear2BoundsTest, CollectPrecedences) {
  Model model;
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* relations = model.GetOrCreate<EnforcedLinear2Bounds>();
  auto* root_bounds = model.GetOrCreate<RootLevelLinear2Bounds>();

  std::vector<IntegerVariable> vars = AddVariables(integer_trail);
  root_bounds->AddUpperBound(LinearExpression2::Difference(vars[0], vars[2]),
                             IntegerValue(-1));
  root_bounds->AddUpperBound(LinearExpression2::Difference(vars[0], vars[5]),
                             IntegerValue(-1));
  root_bounds->AddUpperBound(LinearExpression2::Difference(vars[1], vars[2]),
                             IntegerValue(-1));
  root_bounds->AddUpperBound(LinearExpression2::Difference(vars[2], vars[4]),
                             IntegerValue(-1));
  root_bounds->AddUpperBound(LinearExpression2::Difference(vars[3], vars[4]),
                             IntegerValue(-1));
  root_bounds->AddUpperBound(LinearExpression2::Difference(vars[4], vars[5]),
                             IntegerValue(-1));

  std::vector<EnforcedLinear2Bounds::PrecedenceData> p;
  relations->CollectPrecedences({vars[0], vars[2], vars[3]}, &p);

  // Note that we do not return precedences with just one variable.
  std::vector<int> indices;
  std::vector<IntegerVariable> variables;
  for (const auto precedence : p) {
    indices.push_back(precedence.var_index);
    variables.push_back(precedence.var);
  }
  EXPECT_EQ(indices, (std::vector<int>{1, 2}));
  EXPECT_EQ(variables, (std::vector<IntegerVariable>{vars[4], vars[4]}));

  // Same with NegationOf() and also test that p is cleared.
  relations->CollectPrecedences({NegationOf(vars[0]), NegationOf(vars[4])}, &p);
  EXPECT_TRUE(p.empty());
}

TEST(BinaryRelationRepositoryTest, Build) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(-100, 100));
  const IntegerVariable y = model.Add(NewIntegerVariable(-100, 100));
  const IntegerVariable z = model.Add(NewIntegerVariable(-100, 100));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  const Literal lit_b = Literal(model.Add(NewBooleanVariable()), true);
  BinaryRelationRepository repository;
  RootLevelLinear2Bounds* root_level_bounds =
      model.GetOrCreate<RootLevelLinear2Bounds>();
  repository.Add(lit_a, LinearExpression2(NegationOf(x), y, 1, 1), 2, 8);
  root_level_bounds->Add(LinearExpression2(x, y, 2, -2), 0, 10);
  repository.Add(lit_a, LinearExpression2(x, NegationOf(y), -3, 2), 1, 15);
  repository.Add(lit_b, LinearExpression2(x, kNoIntegerVariable, -3, 0), 3, 5);
  root_level_bounds->Add(LinearExpression2(x, y, 3, -1), 5, 15);
  root_level_bounds->Add(LinearExpression2::Difference(x, z), 0, 10);
  repository.AddPartialRelation(lit_b, x, z);
  repository.Build();

  auto get_rel = [&](absl::Span<const int> indexes) {
    std::vector<Relation> result;
    for (int i : indexes) {
      result.push_back(repository.relation(i));
    }
    return result;
  };
  std::vector<int> all(repository.size());
  std::iota(all.begin(), all.end(), 0);
  EXPECT_THAT(
      get_rel(all),
      UnorderedElementsAre(
          Relation{lit_a, LinearExpression2(x, y, 1, -1), -8, -2},
          Relation{lit_a, LinearExpression2(x, y, 3, 2), -15, -1},
          Relation{lit_b, LinearExpression2(kNoIntegerVariable, x, 0, -3), 3,
                   5},
          Relation{lit_b, LinearExpression2(x, z, 1, 1), 0, 0}));
  EXPECT_THAT(get_rel(repository.IndicesOfRelationsEnforcedBy(lit_a)),
              UnorderedElementsAre(
                  Relation{lit_a, LinearExpression2(x, y, 1, -1), -8, -2},
                  Relation{lit_a, LinearExpression2(x, y, 3, 2), -15, -1}));
  EXPECT_THAT(
      get_rel(repository.IndicesOfRelationsEnforcedBy(lit_b)),
      UnorderedElementsAre(
          Relation{lit_b, LinearExpression2(kNoIntegerVariable, x, 0, -3), 3,
                   5},
          Relation{lit_b, LinearExpression2(x, z, 1, 1), 0, 0}));
  EXPECT_THAT(root_level_bounds->GetAllBoundsContainingVariable(x),
              UnorderedElementsAre(
                  FieldsAre(LinearExpression2(x, NegationOf(y), 1, 1), 0, 5),

                  FieldsAre(LinearExpression2(x, NegationOf(y), 3, 1), 5, 15),
                  FieldsAre(LinearExpression2(x, NegationOf(z), 1, 1), 0, 10)));
  EXPECT_THAT(
      root_level_bounds->GetAllBoundsContainingVariable(y),
      UnorderedElementsAre(FieldsAre(LinearExpression2(y, x, -1, 1), 0, 5),
                           FieldsAre(LinearExpression2(y, x, -1, 3), 5, 15)));
  EXPECT_THAT(
      root_level_bounds->GetAllBoundsContainingVariable(z),
      UnorderedElementsAre(FieldsAre(LinearExpression2(z, x, -1, 1), 0, 10)));
  EXPECT_THAT(
      root_level_bounds->GetAllBoundsContainingVariables(x, y),
      UnorderedElementsAre(FieldsAre(LinearExpression2(x, y, 1, -1), 0, 5),
                           FieldsAre(LinearExpression2(x, y, 3, -1), 5, 15)));
  EXPECT_THAT(
      root_level_bounds->GetAllBoundsContainingVariables(y, x),
      UnorderedElementsAre(FieldsAre(LinearExpression2(y, x, -1, 1), 0, 5),
                           FieldsAre(LinearExpression2(y, x, -1, 3), 5, 15)));
  EXPECT_THAT(
      root_level_bounds->GetAllBoundsContainingVariables(x, z),
      UnorderedElementsAre(FieldsAre(LinearExpression2(x, z, 1, -1), 0, 10)));
  EXPECT_THAT(root_level_bounds->GetAllBoundsContainingVariables(z, y),
              IsEmpty());
}

std::vector<Relation> GetRelations(Model& model) {
  const BinaryRelationRepository& repository =
      *model.GetOrCreate<BinaryRelationRepository>();
  std::vector<Relation> relations;
  for (int i = 0; i < repository.size(); ++i) {
    Relation r = repository.relation(i);
    if (r.expr.coeffs[0] < 0) {
      r = Relation({r.enforcement,
                    LinearExpression2(r.expr.vars[0], r.expr.vars[1],
                                      -r.expr.coeffs[0], -r.expr.coeffs[1]),
                    -r.rhs, -r.lhs});
    }
    relations.push_back(r);
  }
  return relations;
}

TEST(BinaryRelationRepositoryTest, LoadCpModelAddUnaryAndBinaryRelations) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 0 ]
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 10 ]
      }
    }
    constraints {
      enforcement_literal: [ 1 ]
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 5, 10 ]
      }
    }
    constraints {
      enforcement_literal: [ 0, 1, 4 ]
      linear {
        vars: [ 3 ]
        coeffs: [ 1 ]
        domain: [ 5, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 2, 3 ]
        coeffs: [ 3, -2 ]
        domain: [ -10, 10 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ -5, 5 ]
      }
    }
  )pb");
  Model model;

  LoadCpModel(model_proto, &model);

  const CpModelMapping& mapping = *model.GetOrCreate<CpModelMapping>();
  EXPECT_THAT(
      GetRelations(model),
      UnorderedElementsAre(Relation{mapping.Literal(0),
                                    LinearExpression2::Difference(
                                        mapping.Integer(2), mapping.Integer(3)),
                                    0, 10},
                           Relation{mapping.Literal(1),
                                    LinearExpression2(kNoIntegerVariable,
                                                      mapping.Integer(2), 0, 1),
                                    5, 10}));
}

TEST(BinaryRelationRepositoryTest,
     LoadCpModelAddsUnaryRelationsEnforcedByTwoLiterals_Case1) {
  // x in [10, 90] and "a and b => x in [20, 90]".
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 10, 90 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 20, 90 ]
      }
    }
  )pb");
  Model model;
  // This is needed to get an integer view of all the Boolean variables.
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);

  LoadCpModel(model_proto, &model);

  const CpModelMapping& mapping = *model.GetOrCreate<CpModelMapping>();
  const IntegerEncoder& encoder = *model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable a = encoder.GetLiteralView(mapping.Literal(0));
  const IntegerVariable b = encoder.GetLiteralView(mapping.Literal(1));
  const IntegerVariable x = mapping.Integer(2);
  // Two binary relations enforced by only one literal should be added:
  // - a => x - 10.b in [10, 90]
  // - b => x - 10.a in [10, 90]
  EXPECT_THAT(GetRelations(model),
              UnorderedElementsAre(
                  Relation{mapping.Literal(0), LinearExpression2(b, x, 10, -1),
                           -90, -10},
                  Relation{mapping.Literal(1), LinearExpression2(a, x, 10, -1),
                           -90, -10}));
}

TEST(BinaryRelationRepositoryTest,
     LoadCpModelAddsUnaryRelationsEnforcedByTwoLiterals_Case2) {
  // x in [10, 90] and "a and b => x in [10, 80]".
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 10, 90 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 10, 80 ]
      }
    }
  )pb");
  Model model;
  // This is needed to get an integer view of all the Boolean variables.
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);

  LoadCpModel(model_proto, &model);

  const CpModelMapping& mapping = *model.GetOrCreate<CpModelMapping>();
  const IntegerEncoder& encoder = *model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable a = encoder.GetLiteralView(mapping.Literal(0));
  const IntegerVariable b = encoder.GetLiteralView(mapping.Literal(1));
  const IntegerVariable x = mapping.Integer(2);
  // Two binary relations enforced by only one literal should be added:
  // - a => x + 10.b in [10, 90]
  // - b => x + 10.a in [10, 90]
  EXPECT_THAT(
      GetRelations(model),
      UnorderedElementsAre(
          Relation{mapping.Literal(0), LinearExpression2(b, x, 10, 1), 10, 90},
          Relation{mapping.Literal(1), LinearExpression2(a, x, 10, 1), 10,
                   90}));
}

TEST(BinaryRelationRepositoryTest,
     LoadCpModelAddsUnaryRelationsEnforcedByTwoLiterals_Case3) {
  // x in [10, 90] and "a and not(b) => x in [20, 90]".
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 10, 90 ] }
    constraints {
      enforcement_literal: [ 0, -2 ]
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 20, 90 ]
      }
    }
  )pb");
  Model model;
  // This is needed to get an integer view of all the Boolean variables.
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);

  LoadCpModel(model_proto, &model);

  const CpModelMapping& mapping = *model.GetOrCreate<CpModelMapping>();
  const IntegerEncoder& encoder = *model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable a = encoder.GetLiteralView(mapping.Literal(0));
  const IntegerVariable b = encoder.GetLiteralView(mapping.Literal(1));
  const IntegerVariable x = mapping.Integer(2);
  // Two binary relations enforced by only one literal should be added:
  // - a => x + 10.b in [20, 100]  (<=> a => x - (10-10.b) in [10, 90])
  // - b => x - 10.a in [10, 90]
  EXPECT_THAT(
      GetRelations(model),
      UnorderedElementsAre(
          Relation{mapping.Literal(0), LinearExpression2(b, x, 10, 1), 20, 100},
          Relation{mapping.Literal(1).Negated(),
                   LinearExpression2(a, x, 10, -1), -90, -10}));
}

TEST(BinaryRelationRepositoryTest,
     LoadCpModelAddsUnaryRelationsEnforcedByTwoLiterals_Case4) {
  // x in [10, 90] and "a and not(b) => x in [10, 80]".
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 10, 90 ] }
    constraints {
      enforcement_literal: [ 0, -2 ]
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 10, 80 ]
      }
    }
  )pb");
  Model model;
  // This is needed to get an integer view of all the Boolean variables.
  model.GetOrCreate<SatParameters>()->set_linearization_level(2);

  LoadCpModel(model_proto, &model);

  const CpModelMapping& mapping = *model.GetOrCreate<CpModelMapping>();
  const IntegerEncoder& encoder = *model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable a = encoder.GetLiteralView(mapping.Literal(0));
  const IntegerVariable b = encoder.GetLiteralView(mapping.Literal(1));
  const IntegerVariable x = mapping.Integer(2);
  // Two binary relations enforced by only one literal should be added:
  // - a => x - 10.b in [0, 80]  (<=> a => x + (10-10.b) in [10, 90])
  // - b => x + 10.a in [10, 90]
  EXPECT_THAT(
      GetRelations(model),
      UnorderedElementsAre(
          Relation{mapping.Literal(0), LinearExpression2(b, x, 10, -1), -80, 0},
          Relation{mapping.Literal(1).Negated(), LinearExpression2(a, x, 10, 1),
                   10, 90}));
}

TEST(BinaryRelationRepositoryTest, PropagateLocalBounds_EnforcedRelation) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  BinaryRelationRepository repository;
  RootLevelLinear2Bounds* root_level_bounds =
      model.GetOrCreate<RootLevelLinear2Bounds>();
  repository.Add(lit_a, LinearExpression2::Difference(y, x), 2,
                 10);  // lit_a => y => x + 2
  repository.Build();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  absl::flat_hash_map<IntegerVariable, IntegerValue> input = {{x, 3}};
  absl::flat_hash_map<IntegerVariable, IntegerValue> output;

  const bool result = repository.PropagateLocalBounds(
      *integer_trail, *root_level_bounds, lit_a, input, &output);

  EXPECT_TRUE(result);
  EXPECT_THAT(output, UnorderedElementsAre(std::make_pair(NegationOf(x), -8),
                                           std::make_pair(y, 5)));
}

TEST(BinaryRelationRepositoryTest, PropagateLocalBounds_UnenforcedRelation) {
  Model model;
  RootLevelLinear2Bounds* root_level_bounds =
      model.GetOrCreate<RootLevelLinear2Bounds>();
  const IntegerVariable x = model.Add(NewIntegerVariable(-100, 100));
  const IntegerVariable y = model.Add(NewIntegerVariable(-100, 100));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  BinaryRelationRepository repository;
  repository.Add(lit_a, LinearExpression2(x, y, -1, 1), -5,
                 10);  // lit_a => y => x - 5
  root_level_bounds->Add(LinearExpression2(x, y, -1, 1), 2,
                         10);  // y => x + 2
  repository.Build();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  absl::flat_hash_map<IntegerVariable, IntegerValue> input = {{x, 3}};
  absl::flat_hash_map<IntegerVariable, IntegerValue> output;

  const bool result = repository.PropagateLocalBounds(
      *integer_trail, *root_level_bounds, lit_a, input, &output);

  EXPECT_TRUE(result);
  EXPECT_THAT(output, UnorderedElementsAre(std::make_pair(NegationOf(x), -98),
                                           std::make_pair(y, 5)));
}

TEST(BinaryRelationRepositoryTest,
     PropagateLocalBounds_EnforcedBoundSmallerThanLevelZeroBound) {
  Model model;
  RootLevelLinear2Bounds* root_level_bounds =
      model.GetOrCreate<RootLevelLinear2Bounds>();
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  const Literal lit_b = Literal(model.Add(NewBooleanVariable()), true);
  BinaryRelationRepository repository;
  repository.Add(lit_a, LinearExpression2::Difference(y, x), -5,
                 10);  // lit_a => y => x - 5
  repository.Add(lit_b, LinearExpression2::Difference(y, x), 2,
                 10);  // lit_b => y => x + 2
  repository.Build();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  absl::flat_hash_map<IntegerVariable, IntegerValue> input = {{x, 3}};
  absl::flat_hash_map<IntegerVariable, IntegerValue> output;

  const bool result = repository.PropagateLocalBounds(
      *integer_trail, *root_level_bounds, lit_a, input, &output);

  EXPECT_TRUE(result);
  EXPECT_THAT(output, IsEmpty());
}

TEST(BinaryRelationRepositoryTest,
     PropagateLocalBounds_EnforcedBoundSmallerThanOutputBound) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  BinaryRelationRepository repository;
  RootLevelLinear2Bounds* root_level_bounds =
      model.GetOrCreate<RootLevelLinear2Bounds>();
  repository.Add(lit_a, LinearExpression2::Difference(y, x), 2,
                 10);  // lit_a => y => x + 2
  repository.Build();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  absl::flat_hash_map<IntegerVariable, IntegerValue> input = {{x, 3}};
  absl::flat_hash_map<IntegerVariable, IntegerValue> output = {{y, 8}};

  const bool result = repository.PropagateLocalBounds(
      *integer_trail, *root_level_bounds, lit_a, input, &output);

  EXPECT_TRUE(result);
  EXPECT_THAT(output, UnorderedElementsAre(std::make_pair(NegationOf(x), -8),
                                           std::make_pair(y, 8)));
}

TEST(BinaryRelationRepositoryTest, PropagateLocalBounds_Infeasible) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  BinaryRelationRepository repository;
  RootLevelLinear2Bounds* root_level_bounds =
      model.GetOrCreate<RootLevelLinear2Bounds>();
  repository.Add(lit_a, LinearExpression2::Difference(y, x), 8,
                 10);  // lit_a => y => x + 8
  repository.Build();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  absl::flat_hash_map<IntegerVariable, IntegerValue> input = {{x, 3}};
  absl::flat_hash_map<IntegerVariable, IntegerValue> output;

  const bool result = repository.PropagateLocalBounds(
      *integer_trail, *root_level_bounds, lit_a, input, &output);

  EXPECT_FALSE(result);
  EXPECT_THAT(output, UnorderedElementsAre(std::make_pair(NegationOf(x), -2),
                                           std::make_pair(y, 11)));
}

TEST(GreaterThanAtLeastOneOfDetectorTest, AddGreaterThanAtLeastOneOf) {
  Model model;
  const IntegerVariable a = model.Add(NewIntegerVariable(2, 10));
  const IntegerVariable b = model.Add(NewIntegerVariable(5, 10));
  const IntegerVariable c = model.Add(NewIntegerVariable(3, 10));
  const IntegerVariable d = model.Add(NewIntegerVariable(0, 10));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  const Literal lit_b = Literal(model.Add(NewBooleanVariable()), true);
  const Literal lit_c = Literal(model.Add(NewBooleanVariable()), true);
  model.Add(ClauseConstraint({lit_a, lit_b, lit_c}));

  auto* repository = model.GetOrCreate<BinaryRelationRepository>();
  repository->Add(lit_a, LinearExpression2::Difference(d, a), 2,
                  1000);  // d >= a + 2
  repository->Add(lit_b, LinearExpression2::Difference(d, b), -1,
                  1000);  // d >= b -1
  repository->Add(lit_c, LinearExpression2::Difference(d, c), 0,
                  1000);  // d >= c
  repository->Build();
  auto* detector = model.GetOrCreate<GreaterThanAtLeastOneOfDetector>();

  auto* solver = model.GetOrCreate<SatSolver>();
  EXPECT_TRUE(solver->Propagate());
  EXPECT_EQ(model.Get(LowerBound(d)), 0);

  EXPECT_EQ(1, detector->AddGreaterThanAtLeastOneOfConstraints(&model));
  EXPECT_TRUE(solver->Propagate());
  EXPECT_EQ(model.Get(LowerBound(d)), std::min({2 + 2, 5 - 1, 3 + 0}));
}

TEST(GreaterThanAtLeastOneOfDetectorTest,
     AddGreaterThanAtLeastOneOfWithAutoDetect) {
  Model model;
  const IntegerVariable a = model.Add(NewIntegerVariable(2, 10));
  const IntegerVariable b = model.Add(NewIntegerVariable(5, 10));
  const IntegerVariable c = model.Add(NewIntegerVariable(3, 10));
  const IntegerVariable d = model.Add(NewIntegerVariable(0, 10));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  const Literal lit_b = Literal(model.Add(NewBooleanVariable()), true);
  const Literal lit_c = Literal(model.Add(NewBooleanVariable()), true);
  model.Add(ClauseConstraint({lit_a, lit_b, lit_c}));

  auto* repository = model.GetOrCreate<BinaryRelationRepository>();
  repository->Add(lit_a, LinearExpression2(a, d, -1, 1), 2,
                  1000);  // d >= a + 2
  repository->Add(lit_b, LinearExpression2(b, d, -1, 1), -1,
                  1000);                                            // d >= b -1
  repository->Add(lit_c, LinearExpression2(c, d, -1, 1), 0, 1000);  // d >= c
  repository->Build();
  auto* detector = model.GetOrCreate<GreaterThanAtLeastOneOfDetector>();

  auto* solver = model.GetOrCreate<SatSolver>();
  EXPECT_TRUE(solver->Propagate());
  EXPECT_EQ(model.Get(LowerBound(d)), 0);

  EXPECT_EQ(1, detector->AddGreaterThanAtLeastOneOfConstraints(
                   &model, /*auto_detect_clauses=*/true));
  EXPECT_TRUE(solver->Propagate());
  EXPECT_EQ(model.Get(LowerBound(d)), std::min({2 + 2, 5 - 1, 3 + 0}));
}

TEST(TransitivePrecedencesEvaluatorTest, ComputeFullPrecedencesIfCycle) {
  Model model;
  std::vector<IntegerVariable> vars(10);
  for (int i = 0; i < vars.size(); ++i) {
    vars[i] = model.Add(NewIntegerVariable(0, 10));
  }

  // Even if the weight are compatible, we will fail here.
  auto* r = model.GetOrCreate<RootLevelLinear2Bounds>();
  r->AddUpperBound(LinearExpression2::Difference(vars[0], vars[1]), -2);
  r->AddUpperBound(LinearExpression2::Difference(vars[1], vars[2]), -2);
  r->AddUpperBound(LinearExpression2::Difference(vars[2], vars[1]), 10);
  r->AddUpperBound(LinearExpression2::Difference(vars[0], vars[2]), -5);

  std::vector<FullIntegerPrecedence> precedences;
  model.GetOrCreate<TransitivePrecedencesEvaluator>()->ComputeFullPrecedences(
      {vars[0], vars[1]}, &precedences);
  EXPECT_TRUE(precedences.empty());
}

TEST(TransitivePrecedencesEvaluatorTest, BasicTest1) {
  Model model;
  std::vector<IntegerVariable> vars(10);
  for (int i = 0; i < vars.size(); ++i) {
    vars[i] = model.Add(NewIntegerVariable(0, 10));
  }

  //    1
  //   / \
  //  0   2 -- 4
  //   \ /
  //    3
  auto* r = model.GetOrCreate<RootLevelLinear2Bounds>();
  r->AddUpperBound(LinearExpression2::Difference(vars[0], vars[1]), -2);
  r->AddUpperBound(LinearExpression2::Difference(vars[1], vars[2]), -2);
  r->AddUpperBound(LinearExpression2::Difference(vars[0], vars[3]), -1);
  r->AddUpperBound(LinearExpression2::Difference(vars[3], vars[2]), -2);
  r->AddUpperBound(LinearExpression2::Difference(vars[2], vars[4]), -2);

  std::vector<FullIntegerPrecedence> precedences;
  model.GetOrCreate<TransitivePrecedencesEvaluator>()->ComputeFullPrecedences(
      {vars[0], vars[1], vars[3]}, &precedences);

  // We only output size at least 2, and "relevant" precedences.
  // So here only vars[2].
  ASSERT_EQ(precedences.size(), 1);
  EXPECT_EQ(precedences[0].var, vars[2]);
  EXPECT_THAT(precedences[0].offsets, ElementsAre(4, 2, 2));
  EXPECT_THAT(precedences[0].indices, ElementsAre(0, 1, 2));
}

TEST(TransitivePrecedencesEvaluatorTest, BasicTest2) {
  Model model;
  std::vector<IntegerVariable> vars(10);
  for (int i = 0; i < vars.size(); ++i) {
    vars[i] = model.Add(NewIntegerVariable(0, 10));
  }

  //    1
  //   / \
  //  0   2 -- 4
  //   \ /    /
  //    3    5
  auto* r = model.GetOrCreate<RootLevelLinear2Bounds>();
  r->AddUpperBound(LinearExpression2::Difference(vars[0], vars[1]), -2);
  r->AddUpperBound(LinearExpression2::Difference(vars[1], vars[2]), -2);
  r->AddUpperBound(LinearExpression2::Difference(vars[0], vars[3]), -1);
  r->AddUpperBound(LinearExpression2::Difference(vars[3], vars[2]), -2);
  r->AddUpperBound(LinearExpression2::Difference(vars[2], vars[4]), -2);
  r->AddUpperBound(LinearExpression2::Difference(vars[5], vars[4]), -7);

  std::vector<FullIntegerPrecedence> precedences;
  model.GetOrCreate<TransitivePrecedencesEvaluator>()->ComputeFullPrecedences(
      {vars[0], vars[1], vars[3]}, &precedences);

  // Same as before here.
  ASSERT_EQ(precedences.size(), 1);
  EXPECT_EQ(precedences[0].var, vars[2]);
  EXPECT_THAT(precedences[0].offsets, ElementsAre(4, 2, 2));
  EXPECT_THAT(precedences[0].indices, ElementsAre(0, 1, 2));

  // But if we ask for 5, we will get two results.
  precedences.clear();
  model.GetOrCreate<TransitivePrecedencesEvaluator>()->ComputeFullPrecedences(
      {vars[0], vars[1], vars[3], vars[5]}, &precedences);
  ASSERT_EQ(precedences.size(), 2);
  EXPECT_EQ(precedences[0].var, vars[2]);
  EXPECT_THAT(precedences[0].offsets, ElementsAre(4, 2, 2));
  EXPECT_THAT(precedences[0].indices, ElementsAre(0, 1, 2));
  EXPECT_EQ(precedences[1].var, vars[4]);
  EXPECT_THAT(precedences[1].offsets, ElementsAre(6, 4, 4, 7));
  EXPECT_THAT(precedences[1].indices, ElementsAre(0, 1, 2, 3));
}

TEST(BinaryRelationMapsTest, AffineUpperBound) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 2));
  const IntegerVariable w = model.Add(NewIntegerVariable(0, 20));

  // x - y;
  LinearExpression2 expr;
  expr.vars[0] = x;
  expr.vars[1] = y;
  expr.coeffs[0] = IntegerValue(1);
  expr.coeffs[1] = IntegerValue(-1);

  // Starts with trivial level zero bound.
  auto* bounds = model.GetOrCreate<Linear2Bounds>();
  auto* lin3_bounds = model.GetOrCreate<Linear2BoundsFromLinear3>();
  auto* root_bounds = model.GetOrCreate<RootLevelLinear2Bounds>();
  EXPECT_EQ(bounds->UpperBound(expr), IntegerValue(10));

  auto* search = model.GetOrCreate<IntegerSearchHelper>();
  search->TakeDecision(
      Literal(search->GetDecisionLiteral(BooleanOrIntegerLiteral(
          IntegerLiteral::LowerOrEqual(w, IntegerValue(10))))));

  // Lets add a relation.
  root_bounds->Add(expr, IntegerValue(-5), IntegerValue(5));
  EXPECT_EQ(bounds->UpperBound(expr), IntegerValue(5));

  // Note that we canonicalize with gcd.
  expr.coeffs[0] *= 3;
  expr.coeffs[1] *= 3;
  EXPECT_EQ(bounds->UpperBound(expr), IntegerValue(15));

  // Lets add an affine upper bound to that expression <= 4 * z + 1.
  EXPECT_TRUE(lin3_bounds->AddAffineUpperBound(
      expr, AffineExpression(z, IntegerValue(4), IntegerValue(1))));
  EXPECT_EQ(bounds->UpperBound(expr), IntegerValue(9));

  // Lets test the reason, first push a new bound.
  search->TakeDecision(
      Literal(search->GetDecisionLiteral(BooleanOrIntegerLiteral(
          IntegerLiteral::LowerOrEqual(z, IntegerValue(1))))));

  // Because of gcd, even though ub(affine) is now 5, we get 3,
  EXPECT_EQ(bounds->UpperBound(expr), IntegerValue(3));
  {
    std::vector<Literal> literal_reason;
    std::vector<IntegerLiteral> integer_reason;
    bounds->AddReasonForUpperBoundLowerThan(expr, IntegerValue(4),
                                            &literal_reason, &integer_reason);
    EXPECT_THAT(literal_reason, ElementsAre());
    EXPECT_THAT(integer_reason,
                ElementsAre(IntegerLiteral::LowerOrEqual(z, IntegerValue(1))));
  }

  // If we use a bound not as strong, we get a different reason though.
  {
    std::vector<Literal> literal_reason;
    std::vector<IntegerLiteral> integer_reason;
    bounds->AddReasonForUpperBoundLowerThan(expr, IntegerValue(9),
                                            &literal_reason, &integer_reason);
    EXPECT_THAT(literal_reason, ElementsAre());
    EXPECT_THAT(integer_reason,
                ElementsAre(IntegerLiteral::LowerOrEqual(z, IntegerValue(2))));
  }
  {
    // This is implied by the level zero relation x <= 5
    std::vector<Literal> literal_reason;
    std::vector<IntegerLiteral> integer_reason;
    bounds->AddReasonForUpperBoundLowerThan(expr, IntegerValue(15),
                                            &literal_reason, &integer_reason);
    EXPECT_THAT(literal_reason, ElementsAre());
    EXPECT_THAT(integer_reason, ElementsAre());
  }

  // Note that the bound works on the canonicalized expr.
  expr.coeffs[0] /= 3;
  expr.coeffs[1] /= 3;
  EXPECT_EQ(bounds->UpperBound(expr), IntegerValue(1));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
