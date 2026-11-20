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

#include "ortools/sat/integer_resolution.h"

#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_propagation.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_literal.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/sat_trail.h"

namespace operations_research::sat {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST(IntegerConflictResolutionTest, CombinedLinearReasonRelaxesWithMinSlack) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_create_1uip_boolean_during_icr(false);
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* icr = model.GetOrCreate<IntegerConflictResolution>();
  auto* linear = model.GetOrCreate<LinearPropagator>();

  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 10));
  linear->AddConstraint({}, {x1, x2, x3}, {1, 1, 1}, 10);

  const Literal d1(model.Add(NewBooleanVariable()), true);
  const Literal d2(model.Add(NewBooleanVariable()), true);
  const Literal d3(model.Add(NewBooleanVariable()), true);
  ASSERT_TRUE(sat_solver->Propagate());

  // Level 1: d1 => x1 >= 4.
  trail->EnqueueSearchDecision(d1);
  ASSERT_TRUE(integer_trail->Propagate(trail));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x1, 4),
                                     {d1.Negated()}, {}));

  // Level 2: d2 => x2 >= 4.
  trail->EnqueueSearchDecision(d2);
  ASSERT_TRUE(integer_trail->Propagate(trail));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x2, 4),
                                     {d2.Negated()}, {}));

  // Level 3: d3 => x3 >= 1 (highest trail index among {x1, x2, x3}, relaxable
  // down to x3 >= 0). Now x1 + x2 + x3 <= 10 propagates x1 <= 5 and x2 <= 5
  // with the same linear reason (slack = 1, propagation_slack = 0).
  trail->EnqueueSearchDecision(d3);
  ASSERT_TRUE(integer_trail->Propagate(trail));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x3, 1),
                                     {d3.Negated()}, {}));
  ASSERT_TRUE(sat_solver->Propagate());
  EXPECT_EQ(integer_trail->UpperBound(x1), 5);
  EXPECT_EQ(integer_trail->UpperBound(x2), 5);

  // Case 1: conflict requiring x1 <= 5 (slack 0) and x2 <= 6 (slack 1) has
  // min_slack = 0, so x3 >= 1 cannot be relaxed and ~d3 must stay.
  EXPECT_FALSE(
      integer_trail->ReportConflict({}, {IntegerLiteral::LowerOrEqual(x1, 5),
                                         IntegerLiteral::LowerOrEqual(x2, 6)}));
  std::vector<Literal> conflict;
  std::vector<Literal> reason_used;
  icr->ComputeFirstUIPConflict(&conflict, &reason_used);
  EXPECT_THAT(conflict, ElementsAre(d3.Negated(), d2.Negated(), d1.Negated()));

  // Case 2: conflict requiring only x1 <= 6 (slack 1) and x2 <= 6 (slack 1)
  // has min_slack = 1, relaxing x3 >= 1 down to level-zero x3 >= 0 and
  // dropping d3 completely.
  EXPECT_FALSE(
      integer_trail->ReportConflict({}, {IntegerLiteral::LowerOrEqual(x1, 6),
                                         IntegerLiteral::LowerOrEqual(x2, 6)}));
  icr->ComputeFirstUIPConflict(&conflict, &reason_used);
  EXPECT_THAT(conflict, ElementsAre(d2.Negated(), d1.Negated()));
}

TEST(IntegerConflictResolutionTest,
     CombinedLinearReasonSettledBatchVarDoesNotOverRelax) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_create_1uip_boolean_during_icr(false);
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* encoder = model.GetOrCreate<IntegerEncoder>();
  auto* icr = model.GetOrCreate<IntegerConflictResolution>();
  auto* linear = model.GetOrCreate<LinearPropagator>();

  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 14));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 14));
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 14));
  linear->AddConstraint({}, {x1, x2, x3}, {1, 1, 1}, 14);

  const Literal d1(model.Add(NewBooleanVariable()), true);
  const Literal d3(model.Add(NewBooleanVariable()), true);
  // Associated literal d2 <=> (x1 >= 5).
  const Literal d2 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(x1, 5));
  ASSERT_TRUE(sat_solver->Propagate());

  // Level 1: d1 => x1 >= 4 and x2 >= 4.
  trail->EnqueueSearchDecision(d1);
  ASSERT_TRUE(integer_trail->Propagate(trail));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x1, 4),
                                     {d1.Negated()}, {}));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x2, 4),
                                     {d1.Negated()}, {}));
  ASSERT_TRUE(sat_solver->Propagate());

  // Level 2: d3 => x3 >= 4. Now x1 + x2 + x3 <= 14 propagates x1 <= 6 and
  // x2 <= 6 (slack = 2, propagation_slack = 0).
  trail->EnqueueSearchDecision(d3);
  ASSERT_TRUE(integer_trail->Propagate(trail));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x3, 4),
                                     {d3.Negated()}, {}));
  ASSERT_TRUE(sat_solver->Propagate());
  EXPECT_EQ(integer_trail->UpperBound(x1), 6);
  EXPECT_EQ(integer_trail->UpperBound(x2), 6);

  // Level 3: decision d2 (x1 >= 5). Note that (x1=7, x2=4, x3=3) satisfies
  // x1 + x2 + x3 <= 14, d1 (x1>=4, x2>=4), and d2 (x1>=5), while falsifying
  // x1 <= 6, so ~d3 must remain in the learned conflict.
  trail->EnqueueSearchDecision(d2);
  ASSERT_TRUE(sat_solver->Propagate());

  EXPECT_FALSE(integer_trail->ReportConflict(
      {d2.Negated()}, {IntegerLiteral::LowerOrEqual(x1, 6),
                       IntegerLiteral::LowerOrEqual(x2, 6)}));

  std::vector<Literal> conflict;
  std::vector<Literal> reason_used;
  icr->ComputeFirstUIPConflict(&conflict, &reason_used);
  EXPECT_THAT(conflict,
              UnorderedElementsAre(d2.Negated(), d3.Negated(), d1.Negated()));
  EXPECT_EQ(conflict[0], d2.Negated());
}

TEST(IntegerConflictResolutionTest,
     CombinedLinearReasonExtraSlackForMinSlackTerm) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_create_1uip_boolean_during_icr(false);
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* icr = model.GetOrCreate<IntegerConflictResolution>();
  auto* linear = model.GetOrCreate<LinearPropagator>();

  const IntegerVariable x1 = model.Add(NewIntegerVariable(3, 10));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 10));
  linear->AddConstraint({}, {x1, x2, x3}, {1, 1, 1}, 10);

  const Literal d1(model.Add(NewBooleanVariable()), true);
  const Literal d2(model.Add(NewBooleanVariable()), true);
  const Literal d3(model.Add(NewBooleanVariable()), true);
  ASSERT_TRUE(sat_solver->Propagate());

  // Level 1: d1 => x1 >= 4 (relaxable by 1 down to level-zero x1 >= 3).
  trail->EnqueueSearchDecision(d1);
  ASSERT_TRUE(integer_trail->Propagate(trail));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x1, 4),
                                     {d1.Negated()}, {}));

  // Level 2: d2 => x2 >= 4.
  trail->EnqueueSearchDecision(d2);
  ASSERT_TRUE(integer_trail->Propagate(trail));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x2, 4),
                                     {d2.Negated()}, {}));

  // Level 3: d3 => x3 >= 1. Now x1 + x2 + x3 <= 10 propagates x1 <= 5 and
  // x2 <= 5 (slack = 1, propagation_slack = 0).
  trail->EnqueueSearchDecision(d3);
  ASSERT_TRUE(integer_trail->Propagate(trail));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x3, 1),
                                     {d3.Negated()}, {}));
  ASSERT_TRUE(sat_solver->Propagate());
  EXPECT_EQ(integer_trail->UpperBound(x1), 5);
  EXPECT_EQ(integer_trail->UpperBound(x2), 5);

  // Case 1: conflict requiring x1 <= 5 (slack 0) and x2 <= 6 (slack 1).
  // Minimum slack is 0 (for x1), so x2 and x3 cannot be relaxed, while x1 gets
  // the extra slack 1 from the other entry, relaxing x1 >= 4 to level-zero
  // x1 >= 3 and dropping ~d1.
  EXPECT_FALSE(
      integer_trail->ReportConflict({}, {IntegerLiteral::LowerOrEqual(x1, 5),
                                         IntegerLiteral::LowerOrEqual(x2, 6)}));
  std::vector<Literal> conflict;
  std::vector<Literal> reason_used;
  icr->ComputeFirstUIPConflict(&conflict, &reason_used);
  EXPECT_THAT(conflict, ElementsAre(d3.Negated(), d2.Negated()));

  // Case 2: conflict requiring x1 <= 5 (slack 0) and x2 <= 5 (slack 0).
  // Both entries tie at slack 0, so there is no extra slack for x1 (indeed
  // (x1=3, x2=6, x3=1) violates x2 <= 5) and ~d1 must stay.
  EXPECT_FALSE(
      integer_trail->ReportConflict({}, {IntegerLiteral::LowerOrEqual(x1, 5),
                                         IntegerLiteral::LowerOrEqual(x2, 5)}));
  icr->ComputeFirstUIPConflict(&conflict, &reason_used);
  EXPECT_THAT(conflict, ElementsAre(d3.Negated(), d2.Negated(), d1.Negated()));
}

TEST(IntegerConflictResolutionTest, CombinedLinearReasonDelayedSecondSlack) {
  // Case 1: another term (x3) is popped first and consumes the shared slack 1,
  // while the minimum-slack term (x1) still gets the remaining extra slack 2.
  {
    Model model;
    model.GetOrCreate<SatParameters>()->set_create_1uip_boolean_during_icr(
        false);
    auto* sat_solver = model.GetOrCreate<SatSolver>();
    auto* trail = model.GetOrCreate<Trail>();
    auto* integer_trail = model.GetOrCreate<IntegerTrail>();
    auto* icr = model.GetOrCreate<IntegerConflictResolution>();
    auto* linear = model.GetOrCreate<LinearPropagator>();

    const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 12));
    const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 12));
    const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 12));
    linear->AddConstraint({}, {x1, x2, x3}, {1, 1, 1}, 12);

    const Literal d1a(model.Add(NewBooleanVariable()), true);
    const Literal d1b(model.Add(NewBooleanVariable()), true);
    const Literal d2(model.Add(NewBooleanVariable()), true);
    const Literal d3a(model.Add(NewBooleanVariable()), true);
    const Literal d3b(model.Add(NewBooleanVariable()), true);
    ASSERT_TRUE(sat_solver->Propagate());

    trail->EnqueueSearchDecision(d1a);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x1, 2),
                                       {d1a.Negated()}, {}));

    trail->EnqueueSearchDecision(d1b);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x1, 4),
                                       {d1b.Negated()}, {}));

    trail->EnqueueSearchDecision(d2);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x2, 4),
                                       {d2.Negated()}, {}));

    trail->EnqueueSearchDecision(d3a);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x3, 1),
                                       {d3a.Negated()}, {}));

    trail->EnqueueSearchDecision(d3b);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x3, 2),
                                       {d3b.Negated()}, {}));
    ASSERT_TRUE(sat_solver->Propagate());
    EXPECT_EQ(integer_trail->UpperBound(x1), 6);
    EXPECT_EQ(integer_trail->UpperBound(x2), 6);

    // Requiring x1 <= 7 (slack 1) and x2 <= 9 (slack 3): x3 relaxes from 2 to 1
    // (dropping d3b) and x1 relaxes from 4 to 2 (dropping d1b).
    EXPECT_FALSE(integer_trail->ReportConflict(
        {}, {IntegerLiteral::LowerOrEqual(x1, 7),
             IntegerLiteral::LowerOrEqual(x2, 9)}));
    std::vector<Literal> conflict;
    std::vector<Literal> reason_used;
    icr->ComputeFirstUIPConflict(&conflict, &reason_used);
    EXPECT_THAT(conflict,
                ElementsAre(d3a.Negated(), d2.Negated(), d1a.Negated()));
  }

  // Case 2: the minimum-slack term (x1) is popped first and consumes the full
  // second_slack (3), preventing x3 from also consuming the shared slack (1).
  {
    Model model;
    model.GetOrCreate<SatParameters>()->set_create_1uip_boolean_during_icr(
        false);
    auto* sat_solver = model.GetOrCreate<SatSolver>();
    auto* trail = model.GetOrCreate<Trail>();
    auto* integer_trail = model.GetOrCreate<IntegerTrail>();
    auto* icr = model.GetOrCreate<IntegerConflictResolution>();
    auto* linear = model.GetOrCreate<LinearPropagator>();

    const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 12));
    const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 12));
    const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 12));
    linear->AddConstraint({}, {x1, x2, x3}, {1, 1, 1}, 12);

    const Literal d3a(model.Add(NewBooleanVariable()), true);
    const Literal d3b(model.Add(NewBooleanVariable()), true);
    const Literal d2(model.Add(NewBooleanVariable()), true);
    const Literal d1a(model.Add(NewBooleanVariable()), true);
    const Literal d1b(model.Add(NewBooleanVariable()), true);
    ASSERT_TRUE(sat_solver->Propagate());

    trail->EnqueueSearchDecision(d3a);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x3, 1),
                                       {d3a.Negated()}, {}));

    trail->EnqueueSearchDecision(d3b);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x3, 2),
                                       {d3b.Negated()}, {}));

    trail->EnqueueSearchDecision(d2);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x2, 4),
                                       {d2.Negated()}, {}));

    trail->EnqueueSearchDecision(d1a);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x1, 1),
                                       {d1a.Negated()}, {}));

    // Propagate d1a before pushing x1 >= 4 at level 5 so x1 <= 6 and x2 <= 6
    // are both pushed at level 5 with the same linear reason.
    ASSERT_TRUE(sat_solver->Propagate());

    trail->EnqueueSearchDecision(d1b);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x1, 4),
                                       {d1b.Negated()}, {}));
    ASSERT_TRUE(sat_solver->Propagate());
    EXPECT_EQ(integer_trail->UpperBound(x1), 6);
    EXPECT_EQ(integer_trail->UpperBound(x2), 6);

    // Requiring x1 <= 7 (slack 1) and x2 <= 9 (slack 3): x1 relaxes by 3 from
    // 4 down to 1 (dropping d1b at level 5), leaving 0 slack for x3 so ~d3b
    // stays (needed since (x1=1, x2=10, x3=1) violates x2 <= 9).
    EXPECT_FALSE(integer_trail->ReportConflict(
        {}, {IntegerLiteral::LowerOrEqual(x1, 7),
             IntegerLiteral::LowerOrEqual(x2, 9)}));
    std::vector<Literal> conflict;
    std::vector<Literal> reason_used;
    icr->ComputeFirstUIPConflict(&conflict, &reason_used);
    EXPECT_THAT(conflict,
                ElementsAre(d1a.Negated(), d2.Negated(), d3b.Negated()));
  }
}

TEST(IntegerConflictResolutionTest,
     CombinedLinearReasonSettledBatchVarIncreasesOtherEntrySlack) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_create_1uip_boolean_during_icr(false);
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* encoder = model.GetOrCreate<IntegerEncoder>();
  auto* icr = model.GetOrCreate<IntegerConflictResolution>();
  auto* linear = model.GetOrCreate<LinearPropagator>();

  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 14));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 14));
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 14));
  linear->AddConstraint({}, {x1, x2, x3}, {1, 1, 1}, 14);

  const Literal d1(model.Add(NewBooleanVariable()), true);
  const Literal d3a(model.Add(NewBooleanVariable()), true);
  const Literal d3b(model.Add(NewBooleanVariable()), true);
  const Literal d2 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(x1, 5));
  ASSERT_TRUE(sat_solver->Propagate());

  // Level 1: d1 => x1 >= 4 and x2 >= 4.
  trail->EnqueueSearchDecision(d1);
  ASSERT_TRUE(integer_trail->Propagate(trail));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x1, 4),
                                     {d1.Negated()}, {}));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x2, 4),
                                     {d1.Negated()}, {}));
  ASSERT_TRUE(sat_solver->Propagate());

  // Level 2: d3a => x3 >= 3.
  trail->EnqueueSearchDecision(d3a);
  ASSERT_TRUE(integer_trail->Propagate(trail));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x3, 3),
                                     {d3a.Negated()}, {}));
  ASSERT_TRUE(sat_solver->Propagate());

  // Level 3: d3b => x3 >= 4. Now x1 + x2 + x3 <= 14 propagates x1 <= 6 and
  // x2 <= 6 (slack = 2, propagation_slack = 0).
  trail->EnqueueSearchDecision(d3b);
  ASSERT_TRUE(integer_trail->Propagate(trail));
  ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x3, 4),
                                     {d3b.Negated()}, {}));
  ASSERT_TRUE(sat_solver->Propagate());
  EXPECT_EQ(integer_trail->UpperBound(x1), 6);
  EXPECT_EQ(integer_trail->UpperBound(x2), 6);

  // Level 4: decision d2 (x1 >= 5), which settles x1 >= 5 as the 1-UIP.
  trail->EnqueueSearchDecision(d2);
  ASSERT_TRUE(sat_solver->Propagate());

  // Requiring x1 <= 7 (bound_diff = 1 => slack 1) and x2 <= 6 (slack +1 from
  // settled x1 >= 5 > 4 => slack 1): both entries have slack 1, relaxing
  // x3 >= 4 to x3 >= 3 and dropping ~d3b.
  EXPECT_FALSE(integer_trail->ReportConflict(
      {d2.Negated()}, {IntegerLiteral::LowerOrEqual(x1, 7),
                       IntegerLiteral::LowerOrEqual(x2, 6)}));

  std::vector<Literal> conflict;
  std::vector<Literal> reason_used;
  icr->ComputeFirstUIPConflict(&conflict, &reason_used);
  EXPECT_THAT(conflict,
              UnorderedElementsAre(d2.Negated(), d3a.Negated(), d1.Negated()));
  EXPECT_EQ(conflict[0], d2.Negated());
}

TEST(IntegerConflictResolutionTest,
     CombinedLinearReasonTwoEntriesDecouplePropagatedTerms) {
  // Immediate relaxation on a tie: both x1 and x2 relax by 2 simultaneously.
  {
    Model model;
    model.GetOrCreate<SatParameters>()->set_create_1uip_boolean_during_icr(
        false);
    auto* sat_solver = model.GetOrCreate<SatSolver>();
    auto* trail = model.GetOrCreate<Trail>();
    auto* integer_trail = model.GetOrCreate<IntegerTrail>();
    auto* icr = model.GetOrCreate<IntegerConflictResolution>();
    auto* linear = model.GetOrCreate<LinearPropagator>();

    const IntegerVariable x1 = model.Add(NewIntegerVariable(2, 10));
    const IntegerVariable x2 = model.Add(NewIntegerVariable(2, 10));
    const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 10));
    linear->AddConstraint({}, {x1, x2, x3}, {1, 1, 1}, 10);

    const Literal d1(model.Add(NewBooleanVariable()), true);
    const Literal d2(model.Add(NewBooleanVariable()), true);
    const Literal d3(model.Add(NewBooleanVariable()), true);
    ASSERT_TRUE(sat_solver->Propagate());

    trail->EnqueueSearchDecision(d1);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x1, 4),
                                       {d1.Negated()}, {}));

    trail->EnqueueSearchDecision(d2);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x2, 4),
                                       {d2.Negated()}, {}));

    trail->EnqueueSearchDecision(d3);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x3, 1),
                                       {d3.Negated()}, {}));
    ASSERT_TRUE(sat_solver->Propagate());
    EXPECT_EQ(integer_trail->UpperBound(x1), 5);
    EXPECT_EQ(integer_trail->UpperBound(x2), 5);

    // Requiring x1 <= 7 (slack 2), x2 <= 7 (slack 2), and x3 >= 1: x1 >= 4 and
    // x2 >= 4 each relax by 2 down to their level-zero bounds of 2 (total
    // relaxation = 4), dropping both ~d1 and ~d2.
    EXPECT_FALSE(integer_trail->ReportConflict(
        {}, {IntegerLiteral::LowerOrEqual(x1, 7),
             IntegerLiteral::LowerOrEqual(x2, 7),
             IntegerLiteral::GreaterOrEqual(x3, 1)}));
    std::vector<Literal> conflict;
    std::vector<Literal> reason_used;
    icr->ComputeFirstUIPConflict(&conflict, &reason_used);
    EXPECT_THAT(conflict, ElementsAre(d3.Negated()));
  }

  // Delayed relaxation on a tie: both x1 and x2 relax from 4 to 2 via separate
  // SlackIndex entries even when neither reaches its level-zero bound of 0.
  {
    Model model;
    model.GetOrCreate<SatParameters>()->set_create_1uip_boolean_during_icr(
        false);
    auto* sat_solver = model.GetOrCreate<SatSolver>();
    auto* trail = model.GetOrCreate<Trail>();
    auto* integer_trail = model.GetOrCreate<IntegerTrail>();
    auto* icr = model.GetOrCreate<IntegerConflictResolution>();
    auto* linear = model.GetOrCreate<LinearPropagator>();

    const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 10));
    const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 10));
    const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 10));
    linear->AddConstraint({}, {x1, x2, x3}, {1, 1, 1}, 10);

    const Literal d1a(model.Add(NewBooleanVariable()), true);
    const Literal d1b(model.Add(NewBooleanVariable()), true);
    const Literal d2a(model.Add(NewBooleanVariable()), true);
    const Literal d2b(model.Add(NewBooleanVariable()), true);
    const Literal d3(model.Add(NewBooleanVariable()), true);
    ASSERT_TRUE(sat_solver->Propagate());

    trail->EnqueueSearchDecision(d1a);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x1, 2),
                                       {d1a.Negated()}, {}));

    trail->EnqueueSearchDecision(d1b);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x1, 4),
                                       {d1b.Negated()}, {}));

    trail->EnqueueSearchDecision(d2a);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x2, 2),
                                       {d2a.Negated()}, {}));

    trail->EnqueueSearchDecision(d2b);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x2, 4),
                                       {d2b.Negated()}, {}));

    trail->EnqueueSearchDecision(d3);
    ASSERT_TRUE(integer_trail->Propagate(trail));
    ASSERT_TRUE(integer_trail->Enqueue(IntegerLiteral::GreaterOrEqual(x3, 1),
                                       {d3.Negated()}, {}));
    ASSERT_TRUE(sat_solver->Propagate());
    EXPECT_EQ(integer_trail->UpperBound(x1), 5);
    EXPECT_EQ(integer_trail->UpperBound(x2), 5);

    EXPECT_FALSE(integer_trail->ReportConflict(
        {}, {IntegerLiteral::LowerOrEqual(x1, 7),
             IntegerLiteral::LowerOrEqual(x2, 7),
             IntegerLiteral::GreaterOrEqual(x3, 1)}));
    std::vector<Literal> conflict;
    std::vector<Literal> reason_used;
    icr->ComputeFirstUIPConflict(&conflict, &reason_used);
    EXPECT_THAT(conflict,
                ElementsAre(d3.Negated(), d2a.Negated(), d1a.Negated()));
  }
}

}  // namespace
}  // namespace operations_research::sat
