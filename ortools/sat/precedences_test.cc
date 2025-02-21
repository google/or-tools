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
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

// A simple macro to make the code more readable.
// TODO(user): move that in a common place. test_utils?
#define EXPECT_BOUNDS_EQ(var, lb, ub)            \
  EXPECT_EQ(integer_trail->LowerBound(var), lb); \
  EXPECT_EQ(integer_trail->UpperBound(var), ub)

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

TEST(PrecedenceRelationsTest, BasicAPI) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  const std::vector<IntegerVariable> vars = AddVariables(integer_trail);

  // Note that odd indices are for the negation.
  IntegerVariable a(0), b(2), c(4), d(6);

  PrecedenceRelations precedences(&model);
  precedences.Add(a, b, 10);
  precedences.Add(d, c, 7);
  precedences.Add(b, d, 5);

  precedences.Build();
  EXPECT_EQ(precedences.GetOffset(a, b), 10);
  EXPECT_EQ(precedences.GetOffset(NegationOf(b), NegationOf(a)), 10);
  EXPECT_EQ(precedences.GetOffset(a, c), 22);
  EXPECT_EQ(precedences.GetOffset(NegationOf(c), NegationOf(a)), 22);
  EXPECT_EQ(precedences.GetOffset(a, d), 15);
  EXPECT_EQ(precedences.GetOffset(NegationOf(d), NegationOf(a)), 15);
  EXPECT_EQ(precedences.GetOffset(d, a), kMinIntegerValue);

  // Once built, we can update the offsets.
  // Note however that this would not propagate through the precedence graphs.
  precedences.Add(a, b, 15);
  EXPECT_EQ(precedences.GetOffset(a, b), 15);
  EXPECT_EQ(precedences.GetOffset(NegationOf(b), NegationOf(a)), 15);
}

TEST(PrecedenceRelationsTest, CornerCase1) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  const std::vector<IntegerVariable> vars = AddVariables(integer_trail);

  // Note that odd indices are for the negation.
  IntegerVariable a(0), b(2), c(4), d(6);

  PrecedenceRelations precedences(&model);
  precedences.Add(a, b, 10);
  precedences.Add(b, c, 7);
  precedences.Add(b, d, 5);
  precedences.Add(NegationOf(b), a, 5);

  precedences.Build();
  EXPECT_EQ(precedences.GetOffset(NegationOf(b), a), 5);
  EXPECT_EQ(precedences.GetOffset(NegationOf(b), b), 15);
  EXPECT_EQ(precedences.GetOffset(NegationOf(b), c), 22);
  EXPECT_EQ(precedences.GetOffset(NegationOf(b), d), 20);
}

TEST(PrecedenceRelationsTest, CornerCase2) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  const std::vector<IntegerVariable> vars = AddVariables(integer_trail);

  // Note that odd indices are for the negation.
  IntegerVariable a(0), b(2), c(4), d(6);

  PrecedenceRelations precedences(&model);
  precedences.Add(NegationOf(a), a, 10);
  precedences.Add(a, b, 7);
  precedences.Add(a, c, 5);
  precedences.Add(a, d, 2);

  precedences.Build();
  EXPECT_EQ(precedences.GetOffset(NegationOf(a), a), 10);
  EXPECT_EQ(precedences.GetOffset(NegationOf(a), b), 17);
  EXPECT_EQ(precedences.GetOffset(NegationOf(a), c), 15);
  EXPECT_EQ(precedences.GetOffset(NegationOf(a), d), 12);
}

TEST(PrecedenceRelationsTest, ConditionalRelations) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  const std::vector<IntegerVariable> vars = AddVariables(integer_trail);

  const Literal l(model.Add(NewBooleanVariable()), true);
  EXPECT_TRUE(sat_solver->EnqueueDecisionIfNotConflicting(l));

  // Note that odd indices are for the negation.
  IntegerVariable a(0), b(2);
  PrecedenceRelations precedences(&model);
  precedences.PushConditionalRelation({l}, a, b, 15);
  precedences.PushConditionalRelation({l}, a, b, 20);

  // We only keep the best one.
  EXPECT_EQ(precedences.GetConditionalOffset(a, NegationOf(b)), -15);
  EXPECT_THAT(precedences.GetConditionalEnforcements(a, NegationOf(b)),
              ElementsAre(l));

  // Backtrack works.
  EXPECT_TRUE(sat_solver->ResetToLevelZero());
  EXPECT_EQ(precedences.GetConditionalOffset(a, NegationOf(b)),
            kMinIntegerValue);
  EXPECT_THAT(precedences.GetConditionalEnforcements(a, NegationOf(b)),
              ElementsAre());
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
  model.Add(LowerOrEqual(a, b));
  model.Add(LowerOrEqual(b, a));

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
  model.Add(LowerOrEqual(x, y));

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

TEST(PrecedenceRelationsTest, CollectPrecedences) {
  Model model;
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* relations = model.GetOrCreate<PrecedenceRelations>();

  std::vector<IntegerVariable> vars = AddVariables(integer_trail);
  relations->Add(vars[0], vars[2], IntegerValue(1));
  relations->Add(vars[0], vars[5], IntegerValue(1));
  relations->Add(vars[1], vars[2], IntegerValue(1));
  relations->Add(vars[2], vars[4], IntegerValue(1));
  relations->Add(vars[3], vars[4], IntegerValue(1));
  relations->Add(vars[4], vars[5], IntegerValue(1));

  std::vector<PrecedenceRelations::PrecedenceData> p;
  relations->CollectPrecedences({vars[0], vars[2], vars[3]}, &p);

  // Note that we do not return precedences with just one variable.
  std::vector<int> indices;
  std::vector<IntegerVariable> variables;
  for (const auto precedence : p) {
    indices.push_back(precedence.index);
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
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  const Literal lit_b = Literal(model.Add(NewBooleanVariable()), true);
  BinaryRelationRepository repository;
  repository.Add(lit_a, {NegationOf(x), 1}, {y, 1}, 2, 8);
  repository.Add(Literal(kNoLiteralIndex), {x, 2}, {y, -2}, 0, 10);
  repository.Add(lit_a, {x, -3}, {NegationOf(y), 2}, 1, 15);
  repository.Add(lit_b, {x, -3}, {kNoIntegerVariable, 0}, 3, 5);
  repository.Add(Literal(kNoLiteralIndex), {x, 3}, {y, -1}, 5, 15);
  repository.Add(Literal(kNoLiteralIndex), {x, 1}, {z, -1}, 0, 10);
  repository.AddPartialRelation(lit_b, x, z);
  repository.Build();

  EXPECT_EQ(repository.size(), 7);
  EXPECT_EQ(repository.relation(0), (Relation{lit_a, {x, -1}, {y, 1}, 2, 8}));
  EXPECT_EQ(repository.relation(1),
            (Relation{Literal(kNoLiteralIndex), {x, 2}, {y, -2}, 0, 10}));
  EXPECT_EQ(repository.relation(2), (Relation{lit_a, {x, -3}, {y, -2}, 1, 15}));
  EXPECT_EQ(repository.relation(3),
            (Relation{lit_b, {x, -3}, {kNoIntegerVariable, 0}, 3, 5}));
  EXPECT_EQ(repository.relation(6), (Relation{lit_b, {x, 1}, {z, 1}, 0, 0}));
  EXPECT_THAT(repository.IndicesOfRelationsEnforcedBy(lit_a),
              UnorderedElementsAre(0, 2));
  EXPECT_THAT(repository.IndicesOfRelationsEnforcedBy(lit_b),
              UnorderedElementsAre(3, 6));
  EXPECT_THAT(repository.IndicesOfRelationsContaining(x),
              UnorderedElementsAre(1, 4, 5));
  EXPECT_THAT(repository.IndicesOfRelationsContaining(y),
              UnorderedElementsAre(1, 4));
  EXPECT_THAT(repository.IndicesOfRelationsContaining(z),
              UnorderedElementsAre(5));
  EXPECT_THAT(repository.IndicesOfRelationsBetween(x, y),
              UnorderedElementsAre(1, 4));
  EXPECT_THAT(repository.IndicesOfRelationsBetween(x, z),
              UnorderedElementsAre(5));
  EXPECT_THAT(repository.IndicesOfRelationsBetween(z, y), IsEmpty());
}

TEST(BinaryRelationRepositoryTest, PropagateLocalBounds_EnforcedRelation) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  BinaryRelationRepository repository;
  repository.Add(lit_a, {x, -1}, {y, 1}, 2, 10);  // lit_a => y => x + 2
  repository.Build();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  absl::flat_hash_map<IntegerVariable, IntegerValue> input = {{x, 3}};
  absl::flat_hash_map<IntegerVariable, IntegerValue> output;

  const bool result =
      repository.PropagateLocalBounds(*integer_trail, lit_a, input, &output);

  EXPECT_TRUE(result);
  EXPECT_THAT(output, UnorderedElementsAre(std::make_pair(NegationOf(x), -8),
                                           std::make_pair(y, 5)));
}

TEST(BinaryRelationRepositoryTest, PropagateLocalBounds_UnenforcedRelation) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  const Literal kNoLiteral = Literal(kNoLiteralIndex);
  BinaryRelationRepository repository;
  repository.Add(lit_a, {x, -1}, {y, 1}, -5, 10);      // lit_a => y => x - 5
  repository.Add(kNoLiteral, {x, -1}, {y, 1}, 2, 10);  // y => x + 2
  repository.Build();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  absl::flat_hash_map<IntegerVariable, IntegerValue> input = {{x, 3}};
  absl::flat_hash_map<IntegerVariable, IntegerValue> output;

  const bool result =
      repository.PropagateLocalBounds(*integer_trail, lit_a, input, &output);

  EXPECT_TRUE(result);
  EXPECT_THAT(output, UnorderedElementsAre(std::make_pair(NegationOf(x), -8),
                                           std::make_pair(y, 5)));
}

TEST(BinaryRelationRepositoryTest,
     PropagateLocalBounds_EnforcedBoundSmallerThanLevelZeroBound) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const Literal lit_a = Literal(model.Add(NewBooleanVariable()), true);
  const Literal lit_b = Literal(model.Add(NewBooleanVariable()), true);
  BinaryRelationRepository repository;
  repository.Add(lit_a, {x, -1}, {y, 1}, -5, 10);  // lit_a => y => x - 5
  repository.Add(lit_b, {x, -1}, {y, 1}, 2, 10);   // lit_b => y => x + 2
  repository.Build();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  absl::flat_hash_map<IntegerVariable, IntegerValue> input = {{x, 3}};
  absl::flat_hash_map<IntegerVariable, IntegerValue> output;

  const bool result =
      repository.PropagateLocalBounds(*integer_trail, lit_a, input, &output);

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
  repository.Add(lit_a, {x, -1}, {y, 1}, 2, 10);  // lit_a => y => x + 2
  repository.Build();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  absl::flat_hash_map<IntegerVariable, IntegerValue> input = {{x, 3}};
  absl::flat_hash_map<IntegerVariable, IntegerValue> output = {{y, 8}};

  const bool result =
      repository.PropagateLocalBounds(*integer_trail, lit_a, input, &output);

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
  repository.Add(lit_a, {x, -1}, {y, 1}, 8, 10);  // lit_a => y => x + 8
  repository.Build();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  absl::flat_hash_map<IntegerVariable, IntegerValue> input = {{x, 3}};
  absl::flat_hash_map<IntegerVariable, IntegerValue> output;

  const bool result =
      repository.PropagateLocalBounds(*integer_trail, lit_a, input, &output);

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
  repository->Add(lit_a, {a, -1}, {d, 1}, 2, 1000);   // d >= a + 2
  repository->Add(lit_b, {b, -1}, {d, 1}, -1, 1000);  // d >= b -1
  repository->Add(lit_c, {c, -1}, {d, 1}, 0, 1000);   // d >= c
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
  repository->Add(lit_a, {a, -1}, {d, 1}, 2, 1000);   // d >= a + 2
  repository->Add(lit_b, {b, -1}, {d, 1}, -1, 1000);  // d >= b -1
  repository->Add(lit_c, {c, -1}, {d, 1}, 0, 1000);   // d >= c
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

TEST(PrecedencesPropagatorTest, ComputeFullPrecedencesIfCycle) {
  Model model;
  std::vector<IntegerVariable> vars(10);
  for (int i = 0; i < vars.size(); ++i) {
    vars[i] = model.Add(NewIntegerVariable(0, 10));
  }

  // Even if the weight are compatible, we will fail here.
  model.Add(LowerOrEqualWithOffset(vars[0], vars[1], 2));
  model.Add(LowerOrEqualWithOffset(vars[1], vars[2], 2));
  model.Add(LowerOrEqualWithOffset(vars[2], vars[1], -10));
  model.Add(LowerOrEqualWithOffset(vars[0], vars[2], 5));

  std::vector<FullIntegerPrecedence> precedences;
  model.GetOrCreate<PrecedenceRelations>()->ComputeFullPrecedences(
      {vars[0], vars[1]}, &precedences);
  EXPECT_TRUE(precedences.empty());
}

TEST(PrecedencesPropagatorTest, BasicFiltering) {
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
  model.Add(LowerOrEqualWithOffset(vars[0], vars[1], 2));
  model.Add(LowerOrEqualWithOffset(vars[1], vars[2], 2));
  model.Add(LowerOrEqualWithOffset(vars[0], vars[3], 1));
  model.Add(LowerOrEqualWithOffset(vars[3], vars[2], 2));
  model.Add(LowerOrEqualWithOffset(vars[2], vars[4], 2));

  std::vector<FullIntegerPrecedence> precedences;
  model.GetOrCreate<PrecedenceRelations>()->ComputeFullPrecedences(
      {vars[0], vars[1], vars[3]}, &precedences);

  // We only output size at least 2, and "relevant" precedences.
  // So here only vars[2].
  ASSERT_EQ(precedences.size(), 1);
  EXPECT_EQ(precedences[0].var, vars[2]);
  EXPECT_THAT(precedences[0].offsets, ElementsAre(4, 2, 2));
  EXPECT_THAT(precedences[0].indices, ElementsAre(0, 1, 2));
}

TEST(PrecedencesPropagatorTest, BasicFiltering2) {
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
  model.Add(LowerOrEqualWithOffset(vars[0], vars[1], 2));
  model.Add(LowerOrEqualWithOffset(vars[1], vars[2], 2));
  model.Add(LowerOrEqualWithOffset(vars[0], vars[3], 1));
  model.Add(LowerOrEqualWithOffset(vars[3], vars[2], 2));
  model.Add(LowerOrEqualWithOffset(vars[2], vars[4], 2));
  model.Add(LowerOrEqualWithOffset(vars[5], vars[4], 7));

  std::vector<FullIntegerPrecedence> precedences;
  model.GetOrCreate<PrecedenceRelations>()->ComputeFullPrecedences(
      {vars[0], vars[1], vars[3]}, &precedences);

  // Same as before here.
  ASSERT_EQ(precedences.size(), 1);
  EXPECT_EQ(precedences[0].var, vars[2]);
  EXPECT_THAT(precedences[0].offsets, ElementsAre(4, 2, 2));
  EXPECT_THAT(precedences[0].indices, ElementsAre(0, 1, 2));

  // But if we ask for 5, we will get two results.
  precedences.clear();
  model.GetOrCreate<PrecedenceRelations>()->ComputeFullPrecedences(
      {vars[0], vars[1], vars[3], vars[5]}, &precedences);
  ASSERT_EQ(precedences.size(), 2);
  EXPECT_EQ(precedences[0].var, vars[2]);
  EXPECT_THAT(precedences[0].offsets, ElementsAre(4, 2, 2));
  EXPECT_THAT(precedences[0].indices, ElementsAre(0, 1, 2));
  EXPECT_EQ(precedences[1].var, vars[4]);
  EXPECT_THAT(precedences[1].offsets, ElementsAre(6, 4, 4, 7));
  EXPECT_THAT(precedences[1].indices, ElementsAre(0, 1, 2, 3));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
