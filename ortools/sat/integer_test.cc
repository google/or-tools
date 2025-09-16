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

#include "ortools/sat/integer.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST(AffineExpressionTest, Inequalities) {
  const IntegerVariable var(1);
  EXPECT_EQ(
      AffineExpression(var, IntegerValue(3)).LowerOrEqual(IntegerValue(8)),
      IntegerLiteral::LowerOrEqual(var, IntegerValue(2)));
  EXPECT_EQ(
      AffineExpression(var, IntegerValue(-3)).LowerOrEqual(IntegerValue(-1)),
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(1)));
  EXPECT_EQ(
      AffineExpression(var, IntegerValue(2)).GreaterOrEqual(IntegerValue(3)),
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(2)));
}

TEST(AffineExpressionTest, ValueAt) {
  const IntegerVariable var(1);
  EXPECT_EQ(AffineExpression(var, IntegerValue(3)).ValueAt(IntegerValue(8)),
            IntegerValue(3 * 8));
  EXPECT_EQ(AffineExpression(var, IntegerValue(3), IntegerValue(-2))
                .ValueAt(IntegerValue(5)),
            IntegerValue(3 * 5 - 2));
}

TEST(AffineExpressionTest, NegatedConstant) {
  const AffineExpression negated = AffineExpression(IntegerValue(3)).Negated();
  EXPECT_EQ(negated.var, kNoIntegerVariable);
  EXPECT_EQ(negated.coeff, 0);
  EXPECT_EQ(negated.constant, -3);
}

TEST(AffineExpressionTest, ApiWithoutVar) {
  const AffineExpression three(IntegerValue(3));
  EXPECT_TRUE(three.GreaterOrEqual(IntegerValue(2)).IsAlwaysTrue());
  EXPECT_TRUE(three.LowerOrEqual(IntegerValue(2)).IsAlwaysFalse());
}

TEST(ToDoubleTest, Infinities) {
  EXPECT_EQ(ToDouble(IntegerValue(100)), 100.0);

  const double kInfinity = std::numeric_limits<double>::infinity();
  EXPECT_EQ(ToDouble(kMaxIntegerValue), kInfinity);
  EXPECT_EQ(ToDouble(kMinIntegerValue), -kInfinity);

  EXPECT_LT(ToDouble(kMaxIntegerValue - IntegerValue(1)), kInfinity);
  EXPECT_GT(ToDouble(kMinIntegerValue + IntegerValue(1)), -kInfinity);
}

TEST(FloorRatioTest, AllSmallCases) {
  // Dividend can take any value.
  for (IntegerValue dividend(-100); dividend < 100; ++dividend) {
    // Divisor must be positive.
    for (IntegerValue divisor(1); divisor < 100; ++divisor) {
      const IntegerValue floor = FloorRatio(dividend, divisor);
      EXPECT_LE(floor * divisor, dividend);
      EXPECT_GT((floor + 1) * divisor, dividend);
    }
  }
}

TEST(PositiveRemainderTest, AllCasesForFixedDivisor) {
  IntegerValue divisor(17);
  for (IntegerValue dividend(-100); dividend < 100; ++dividend) {
    EXPECT_EQ(PositiveRemainder(dividend, divisor),
              dividend - divisor * FloorRatio(dividend, divisor));
  }
}

TEST(CeilRatioTest, AllSmallCases) {
  // Dividend can take any value.
  for (IntegerValue dividend(-100); dividend < 100; ++dividend) {
    // Divisor must be positive.
    for (IntegerValue divisor(1); divisor < 100; ++divisor) {
      const IntegerValue ceil = CeilRatio(dividend, divisor);
      EXPECT_GE(ceil * divisor, dividend);
      EXPECT_LT((ceil - 1) * divisor, dividend);
    }
  }
}

TEST(NegationOfTest, IsIdempotent) {
  for (int i = 0; i < 100; ++i) {
    const IntegerVariable var(i);
    EXPECT_EQ(NegationOf(NegationOf(var)), var);
  }
}

TEST(NegationOfTest, VectorArgument) {
  std::vector<IntegerVariable> vars{IntegerVariable(1), IntegerVariable(2)};
  std::vector<IntegerVariable> negated_vars = NegationOf(vars);
  EXPECT_EQ(negated_vars.size(), vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    EXPECT_EQ(negated_vars[i], NegationOf(vars[i]));
  }
}

TEST(IntegerValue, NegatedCannotOverflow) {
  EXPECT_GT(kMinIntegerValue - 1, std::numeric_limits<int64_t>::min());
}

TEST(IntegerLiteral, OverflowValueAreCapped) {
  const IntegerVariable var(0);
  EXPECT_EQ(IntegerLiteral::GreaterOrEqual(var, kMaxIntegerValue + 1),
            IntegerLiteral::GreaterOrEqual(
                var, IntegerValue(std::numeric_limits<int64_t>::max())));
  EXPECT_EQ(IntegerLiteral::LowerOrEqual(var, kMinIntegerValue - 1),
            IntegerLiteral::LowerOrEqual(
                var, IntegerValue(std::numeric_limits<int64_t>::min())));
}

TEST(IntegerLiteral, NegatedIsIdempotent) {
  for (const IntegerValue value :
       {kMinIntegerValue, kMaxIntegerValue, kMaxIntegerValue + 1,
        IntegerValue(0), IntegerValue(1), IntegerValue(2)}) {
    const IntegerLiteral literal =
        IntegerLiteral::GreaterOrEqual(IntegerVariable(0), value);
    CHECK_EQ(literal, literal.Negated().Negated());
  }
}

// A bound difference of exactly kint64max is ok.
TEST(IntegerTrailDeathTest, LargeVariableDomain) {
  Model model;
  model.Add(NewIntegerVariable(-3, std::numeric_limits<int64_t>::max() - 3));

  if (DEBUG_MODE) {
    // But one of kint64max + 1 cause a check fail in debug.
    EXPECT_DEATH(model.Add(NewIntegerVariable(
                     -3, std::numeric_limits<int64_t>::max() - 2)),
                 "");
  }
}

TEST(IntegerTrailTest, ConstantIntegerVariableSharing) {
  Model model;
  const IntegerVariable a = model.Add(ConstantIntegerVariable(0));
  const IntegerVariable b = model.Add(ConstantIntegerVariable(7));
  const IntegerVariable c = model.Add(ConstantIntegerVariable(-7));
  const IntegerVariable d = model.Add(ConstantIntegerVariable(0));
  const IntegerVariable e = model.Add(ConstantIntegerVariable(3));
  EXPECT_EQ(a, d);
  EXPECT_EQ(b, NegationOf(c));
  EXPECT_NE(a, e);
  EXPECT_EQ(0, model.Get(Value(a)));
  EXPECT_EQ(7, model.Get(Value(b)));
  EXPECT_EQ(-7, model.Get(Value(c)));
  EXPECT_EQ(0, model.Get(Value(d)));
  EXPECT_EQ(3, model.Get(Value(e)));
}

TEST(IntegerTrailTest, VariableCreationAndBoundGetter) {
  Model model;
  IntegerTrail* p = model.GetOrCreate<IntegerTrail>();
  IntegerVariable a = model.Add(NewIntegerVariable(0, 10));
  IntegerVariable b = model.Add(NewIntegerVariable(-10, 10));
  IntegerVariable c = model.Add(NewIntegerVariable(20, 30));

  // Index are dense and contiguous, but two indices are created each time.
  // They start at zero.
  EXPECT_EQ(0, a.value());
  EXPECT_EQ(1, NegationOf(a).value());
  EXPECT_EQ(2, b.value());
  EXPECT_EQ(3, NegationOf(b).value());
  EXPECT_EQ(4, c.value());
  EXPECT_EQ(5, NegationOf(c).value());

  // Bounds matches the one we passed at creation.
  EXPECT_EQ(0, p->LowerBound(a));
  EXPECT_EQ(10, p->UpperBound(a));
  EXPECT_EQ(-10, p->LowerBound(b));
  EXPECT_EQ(10, p->UpperBound(b));
  EXPECT_EQ(20, p->LowerBound(c));
  EXPECT_EQ(30, p->UpperBound(c));

  // Test level-zero enqueue.
  EXPECT_TRUE(
      p->Enqueue(IntegerLiteral::LowerOrEqual(a, IntegerValue(20)), {}, {}));
  EXPECT_EQ(10, p->UpperBound(a));
  EXPECT_TRUE(
      p->Enqueue(IntegerLiteral::LowerOrEqual(a, IntegerValue(7)), {}, {}));
  EXPECT_EQ(7, p->UpperBound(a));
  EXPECT_TRUE(
      p->Enqueue(IntegerLiteral::GreaterOrEqual(a, IntegerValue(5)), {}, {}));
  EXPECT_EQ(5, p->LowerBound(a));
}

TEST(IntegerTrailTest, Untrail) {
  Model model;
  IntegerTrail* p = model.GetOrCreate<IntegerTrail>();
  IntegerVariable a = p->AddIntegerVariable(IntegerValue(1), IntegerValue(10));
  IntegerVariable b = p->AddIntegerVariable(IntegerValue(2), IntegerValue(10));

  Trail* trail = model.GetOrCreate<Trail>();
  trail->Resize(10);

  // We need a reason for the Enqueue():
  const Literal r(model.Add(NewBooleanVariable()), true);
  trail->EnqueueWithUnitReason(r.Negated());

  // Enqueue.
  trail->SetDecisionLevel(1);
  EXPECT_TRUE(p->Propagate(trail));
  EXPECT_TRUE(
      p->Enqueue(IntegerLiteral::GreaterOrEqual(a, IntegerValue(5)), {r}, {}));
  EXPECT_EQ(5, p->LowerBound(a));
  EXPECT_TRUE(
      p->Enqueue(IntegerLiteral::GreaterOrEqual(b, IntegerValue(7)), {r}, {}));
  EXPECT_EQ(7, p->LowerBound(b));

  trail->SetDecisionLevel(2);
  EXPECT_TRUE(p->Propagate(trail));
  EXPECT_TRUE(
      p->Enqueue(IntegerLiteral::GreaterOrEqual(b, IntegerValue(9)), {r}, {}));
  EXPECT_EQ(9, p->LowerBound(b));

  // Untrail.
  trail->SetDecisionLevel(1);
  p->Untrail(*trail, 0);
  EXPECT_EQ(7, p->LowerBound(b));

  trail->SetDecisionLevel(0);
  p->Untrail(*trail, 0);
  EXPECT_EQ(1, p->LowerBound(a));
  EXPECT_EQ(2, p->LowerBound(b));
}

TEST(IntegerTrailTest, BasicReason) {
  Model model;
  IntegerTrail* p = model.GetOrCreate<IntegerTrail>();
  IntegerVariable a = p->AddIntegerVariable(IntegerValue(1), IntegerValue(10));

  Trail* trail = model.GetOrCreate<Trail>();
  trail->Resize(10);
  trail->EnqueueWithUnitReason(Literal(-1));
  trail->EnqueueWithUnitReason(Literal(-2));
  trail->EnqueueWithUnitReason(Literal(+3));
  trail->EnqueueWithUnitReason(Literal(+4));
  trail->SetDecisionLevel(1);
  EXPECT_TRUE(p->Propagate(trail));

  // Enqueue.
  EXPECT_TRUE(p->Enqueue(IntegerLiteral::GreaterOrEqual(a, IntegerValue(2)),
                         Literals({+1}), {}));
  EXPECT_TRUE(p->Enqueue(IntegerLiteral::GreaterOrEqual(a, IntegerValue(3)),
                         Literals({+2}), {}));
  EXPECT_TRUE(p->Enqueue(IntegerLiteral::GreaterOrEqual(a, IntegerValue(5)),
                         Literals({-3}), {}));
  EXPECT_TRUE(p->Enqueue(IntegerLiteral::GreaterOrEqual(a, IntegerValue(6)),
                         Literals({-4}), {}));

  EXPECT_THAT(p->ReasonFor(IntegerLiteral::GreaterOrEqual(a, IntegerValue(6))),
              ElementsAre(Literal(-4)));
  EXPECT_THAT(p->ReasonFor(IntegerLiteral::GreaterOrEqual(a, IntegerValue(5))),
              ElementsAre(Literal(-3)));
  EXPECT_THAT(p->ReasonFor(IntegerLiteral::GreaterOrEqual(a, IntegerValue(4))),
              ElementsAre(Literal(-3)));
  EXPECT_THAT(p->ReasonFor(IntegerLiteral::GreaterOrEqual(a, IntegerValue(3))),
              ElementsAre(Literal(+2)));
  EXPECT_TRUE(
      p->ReasonFor(IntegerLiteral::GreaterOrEqual(a, IntegerValue(0))).empty());
  EXPECT_TRUE(p->ReasonFor(IntegerLiteral::GreaterOrEqual(a, IntegerValue(-10)))
                  .empty());
}

struct LazyReasonForTest : public LazyReasonInterface {
  bool called = false;

  void Explain(int /*id*/, IntegerValue /*propagation_slack*/,
               IntegerVariable /*variable_to_explain*/, int /*trail_index*/,
               std::vector<Literal>* /*literals_reason*/,
               std::vector<int>* /*trail_indices_reason*/) final {
    called = true;
  }
};

TEST(IntegerTrailTest, LazyReason) {
  Model model;
  IntegerTrail* p = model.GetOrCreate<IntegerTrail>();
  IntegerVariable a = p->AddIntegerVariable(IntegerValue(1), IntegerValue(10));

  Trail* trail = model.GetOrCreate<Trail>();
  trail->Resize(10);
  trail->SetDecisionLevel(1);
  EXPECT_TRUE(p->Propagate(trail));

  LazyReasonForTest mock;

  // Enqueue.
  EXPECT_TRUE(p->EnqueueWithLazyReason(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(2)), 0, 0, &mock));
  EXPECT_TRUE(p->Propagate(trail));
  EXPECT_FALSE(mock.called);

  // Called if needed for the conflict.
  EXPECT_FALSE(
      p->Enqueue(IntegerLiteral::LowerOrEqual(a, IntegerValue(1)), {}, {}));
  EXPECT_TRUE(mock.called);
}

TEST(IntegerTrailTest, LiteralAndBoundReason) {
  Model model;
  IntegerTrail* p = model.GetOrCreate<IntegerTrail>();
  IntegerVariable a = model.Add(NewIntegerVariable(0, 10));
  IntegerVariable b = model.Add(NewIntegerVariable(0, 10));
  IntegerVariable c = model.Add(NewIntegerVariable(0, 10));

  Trail* trail = model.GetOrCreate<Trail>();
  trail->Resize(10);
  trail->EnqueueWithUnitReason(Literal(-1));
  trail->EnqueueWithUnitReason(Literal(-2));
  trail->EnqueueWithUnitReason(Literal(-3));
  trail->EnqueueWithUnitReason(Literal(-4));
  trail->SetDecisionLevel(1);
  EXPECT_TRUE(p->Propagate(trail));

  // Enqueue.
  EXPECT_TRUE(p->Enqueue(IntegerLiteral::GreaterOrEqual(a, IntegerValue(1)),
                         Literals({+1}), {}));
  EXPECT_TRUE(p->Enqueue(IntegerLiteral::GreaterOrEqual(a, IntegerValue(2)),
                         Literals({+2}), {}));
  EXPECT_TRUE(p->Enqueue(IntegerLiteral::GreaterOrEqual(b, IntegerValue(3)),
                         Literals({+3}),
                         {IntegerLiteral::GreaterOrEqual(a, IntegerValue(1))}));
  EXPECT_TRUE(p->Enqueue(IntegerLiteral::GreaterOrEqual(c, IntegerValue(5)),
                         Literals({+4, +3}),
                         {IntegerLiteral::GreaterOrEqual(a, IntegerValue(2)),
                          IntegerLiteral::GreaterOrEqual(b, IntegerValue(3))}));

  EXPECT_THAT(p->ReasonFor(IntegerLiteral::GreaterOrEqual(b, IntegerValue(2))),
              UnorderedElementsAre(Literal(+1), Literal(+3)));
  EXPECT_THAT(p->ReasonFor(IntegerLiteral::GreaterOrEqual(c, IntegerValue(3))),
              UnorderedElementsAre(Literal(+2), Literal(+3), Literal(+4)));
}

TEST(IntegerTrailTest, LevelZeroBounds) {
  Model model;
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  IntegerVariable x = model.Add(NewIntegerVariable(0, 10));

  Trail* trail = model.GetOrCreate<Trail>();
  trail->Resize(10);
  trail->SetDecisionLevel(1);
  trail->EnqueueWithUnitReason(Literal(-1));
  trail->EnqueueWithUnitReason(Literal(-2));
  EXPECT_TRUE(integer_trail->Propagate(trail));

  // Enqueue.
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(x, IntegerValue(1)), Literals({+1}), {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::LowerOrEqual(x, IntegerValue(2)), Literals({+2}), {}));

  // TEST.
  EXPECT_EQ(integer_trail->LowerBound(x), IntegerValue(1));
  EXPECT_EQ(integer_trail->UpperBound(x), IntegerValue(2));
  EXPECT_EQ(integer_trail->LevelZeroLowerBound(x), IntegerValue(0));
  EXPECT_EQ(integer_trail->LevelZeroUpperBound(x), IntegerValue(10));
}

TEST(IntegerTrailTest, RelaxLinearReason) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  const IntegerVariable a = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable b = model.Add(NewIntegerVariable(0, 10));
  const Literal reason = Literal(model.Add(NewBooleanVariable()), true);

  auto* sat_solver = model.GetOrCreate<SatSolver>();
  EXPECT_TRUE(sat_solver->EnqueueDecisionIfNotConflicting(reason.Negated()));
  EXPECT_TRUE(sat_solver->Propagate());

  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(1)), {reason}, {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(2)), {reason}, {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(b, IntegerValue(1)), {reason}, {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(3)), {reason}, {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(b, IntegerValue(3)), {reason}, {}));

  std::vector<IntegerValue> coeffs(2, IntegerValue(1));
  std::vector<IntegerLiteral> reasons{
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(3)),
      IntegerLiteral::GreaterOrEqual(b, IntegerValue(3))};

  // No slack, nothing happens.
  integer_trail->RelaxLinearReason(IntegerValue(0), coeffs, &reasons);
  EXPECT_THAT(reasons,
              ElementsAre(IntegerLiteral::GreaterOrEqual(a, IntegerValue(3)),
                          IntegerLiteral::GreaterOrEqual(b, IntegerValue(3))));

  // Some slack, we find the "lowest" possible reason in term of trail index.
  integer_trail->RelaxLinearReason(IntegerValue(3), coeffs, &reasons);
  EXPECT_THAT(reasons,
              ElementsAre(IntegerLiteral::GreaterOrEqual(a, IntegerValue(2)),
                          IntegerLiteral::GreaterOrEqual(b, IntegerValue(1))));
}

TEST(IntegerTrailTest, LiteralIsTrueOrFalse) {
  Model model;
  const IntegerVariable a = model.Add(NewIntegerVariable(1, 9));

  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  EXPECT_TRUE(integer_trail->IntegerLiteralIsTrue(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(0))));
  EXPECT_TRUE(integer_trail->IntegerLiteralIsTrue(
      IntegerLiteral::LowerOrEqual(a, IntegerValue(10))));

  EXPECT_TRUE(integer_trail->IntegerLiteralIsTrue(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(1))));
  EXPECT_FALSE(integer_trail->IntegerLiteralIsFalse(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(1))));

  EXPECT_FALSE(integer_trail->IntegerLiteralIsTrue(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(2))));
  EXPECT_FALSE(integer_trail->IntegerLiteralIsFalse(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(2))));

  EXPECT_FALSE(integer_trail->IntegerLiteralIsTrue(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(10))));
  EXPECT_TRUE(integer_trail->IntegerLiteralIsFalse(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(10))));
}

TEST(IntegerTrailTest, VariableWithHole) {
  Model model;
  IntegerVariable a =
      model.Add(NewIntegerVariable(Domain::FromIntervals({{1, 3}, {6, 7}})));
  model.Add(GreaterOrEqual(a, 4));
  EXPECT_EQ(model.Get(LowerBound(a)), 6);
}

TEST(GenericLiteralWatcherTest, LevelZeroModifiedVariablesCallbackTest) {
  Model model;
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* watcher = model.GetOrCreate<GenericLiteralWatcher>();
  IntegerVariable a = model.Add(NewIntegerVariable(0, 10));
  IntegerVariable b = model.Add(NewIntegerVariable(-10, 10));
  IntegerVariable c = model.Add(NewIntegerVariable(20, 30));

  std::vector<IntegerVariable> collector;
  watcher->RegisterLevelZeroModifiedVariablesCallback(
      [&collector](const std::vector<IntegerVariable>& modified_vars) {
        collector = modified_vars;
      });

  // No propagation.
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_EQ(0, collector.size());

  // Modify 1 variable.
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::LowerOrEqual(c, IntegerValue(27)), {}, {}));
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_EQ(1, collector.size());
  EXPECT_EQ(NegationOf(c), collector[0]);

  // Modify 2 variables.
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(a, IntegerValue(10)), {}, {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::LowerOrEqual(b, IntegerValue(7)), {}, {}));
  EXPECT_TRUE(sat_solver->Propagate());
  ASSERT_EQ(2, collector.size());
  EXPECT_EQ(a, collector[0]);
  EXPECT_EQ(NegationOf(b), collector[1]);

  // Modify 1 variable at level 1.
  model.GetOrCreate<Trail>()->SetDecisionLevel(1);
  EXPECT_TRUE(sat_solver->Propagate());
  collector.clear();
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::LowerOrEqual(b, IntegerValue(6)), {}, {}));
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_TRUE(collector.empty());
}

TEST(GenericLiteralWatcherTest, RevIsInDiveUpdate) {
  Model model;
  bool is_in_dive = false;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* watcher = model.GetOrCreate<GenericLiteralWatcher>();
  const Literal a(sat_solver->NewBooleanVariable(), true);
  const Literal b(sat_solver->NewBooleanVariable(), true);

  // First decision.
  EXPECT_TRUE(sat_solver->EnqueueDecisionIfNotConflicting(a));
  EXPECT_FALSE(is_in_dive);
  watcher->SetUntilNextBacktrack(&is_in_dive);

  // Second decision.
  EXPECT_TRUE(sat_solver->EnqueueDecisionIfNotConflicting(b));
  EXPECT_TRUE(is_in_dive);
  watcher->SetUntilNextBacktrack(&is_in_dive);

  // If we backtrack, it should be set to false.
  EXPECT_TRUE(sat_solver->ResetToLevelZero());
  EXPECT_FALSE(is_in_dive);

  // We can redo the same.
  EXPECT_FALSE(is_in_dive);
  watcher->SetUntilNextBacktrack(&is_in_dive);

  EXPECT_TRUE(sat_solver->EnqueueDecisionIfNotConflicting(a));
  EXPECT_TRUE(is_in_dive);
}

TEST(IntegerEncoderTest, BasicInequalityEncoding) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 10));
  const Literal l3 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(3)));
  const Literal l7 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(7)));
  const Literal l5 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(5)));

  // Test SearchForLiteralAtOrBefore().
  for (IntegerValue v(0); v < 10; ++v) {
    IntegerValue unused;
    const LiteralIndex lb_index = encoder->SearchForLiteralAtOrBefore(
        IntegerLiteral::GreaterOrEqual(var, v), &unused);
    const LiteralIndex ub_index = encoder->SearchForLiteralAtOrBefore(
        IntegerLiteral::LowerOrEqual(var, v), &unused);
    if (v < 3) {
      EXPECT_EQ(lb_index, kNoLiteralIndex);
      EXPECT_EQ(ub_index, l3.NegatedIndex());
    } else if (v < 5) {
      EXPECT_EQ(lb_index, l3.Index());
      EXPECT_EQ(ub_index, l5.NegatedIndex());
    } else if (v < 7) {
      EXPECT_EQ(lb_index, l5.Index());
      EXPECT_EQ(ub_index, l7.NegatedIndex());
    } else {
      EXPECT_EQ(lb_index, l7.Index());
      EXPECT_EQ(ub_index, kNoLiteralIndex);
    }
  }

  // Test the propagation from the literal to the bounds.
  // By default the polarity of the literal are false.
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_FALSE(model.Get(Value(l3)));
  EXPECT_FALSE(model.Get(Value(l5)));
  EXPECT_FALSE(model.Get(Value(l7)));
  EXPECT_EQ(0, model.Get(LowerBound(var)));
  EXPECT_EQ(2, model.Get(UpperBound(var)));

  // Test the other way around.
  model.GetOrCreate<SatSolver>()->Backtrack(0);
  model.Add(GreaterOrEqual(var, 4));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_TRUE(model.Get(Value(l3)));
  EXPECT_FALSE(model.Get(Value(l5)));
  EXPECT_FALSE(model.Get(Value(l7)));
  EXPECT_EQ(4, model.Get(LowerBound(var)));
  EXPECT_EQ(4, model.Get(UpperBound(var)));
}

TEST(IntegerEncoderTest, GetOrCreateTrivialAssociatedLiteral) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 10));
  EXPECT_EQ(encoder->GetTrueLiteral(),
            encoder->GetOrCreateAssociatedLiteral(
                IntegerLiteral::GreaterOrEqual(var, IntegerValue(0))));
  EXPECT_EQ(encoder->GetTrueLiteral(),
            encoder->GetOrCreateAssociatedLiteral(
                IntegerLiteral::GreaterOrEqual(var, IntegerValue(-1))));
  EXPECT_EQ(encoder->GetTrueLiteral(),
            encoder->GetOrCreateAssociatedLiteral(
                IntegerLiteral::LowerOrEqual(var, IntegerValue(10))));
  EXPECT_EQ(encoder->GetFalseLiteral(),
            encoder->GetOrCreateAssociatedLiteral(
                IntegerLiteral::GreaterOrEqual(var, IntegerValue(11))));
  EXPECT_EQ(encoder->GetFalseLiteral(),
            encoder->GetOrCreateAssociatedLiteral(
                IntegerLiteral::GreaterOrEqual(var, IntegerValue(12))));
  EXPECT_EQ(encoder->GetFalseLiteral(),
            encoder->GetOrCreateAssociatedLiteral(
                IntegerLiteral::LowerOrEqual(var, IntegerValue(-1))));
}

TEST(IntegerEncoderTest, ShiftedBinary) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(1, 2));

  encoder->FullyEncodeVariable(var);
  EXPECT_EQ(encoder->FullDomainEncoding(var).size(), 2);
  const std::vector<ValueLiteralPair> var_encoding =
      encoder->FullDomainEncoding(var);

  const Literal g2 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(2)));
  const Literal l1 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::LowerOrEqual(var, IntegerValue(1)));

  EXPECT_EQ(g2, var_encoding[1].literal);
  EXPECT_EQ(l1, var_encoding[0].literal);
  EXPECT_EQ(g2, l1.Negated());
}

TEST(IntegerEncoderTest, SizeTwoDomains) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromValues({1, 3})));

  encoder->FullyEncodeVariable(var);
  EXPECT_EQ(encoder->FullDomainEncoding(var).size(), 2);
  const std::vector<ValueLiteralPair> var_encoding =
      encoder->FullDomainEncoding(var);

  const Literal g2 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(2)));
  const Literal g3 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(3)));
  const Literal l1 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::LowerOrEqual(var, IntegerValue(1)));
  const Literal l2 = encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::LowerOrEqual(var, IntegerValue(2)));

  EXPECT_EQ(g3, var_encoding[1].literal);
  EXPECT_EQ(l1, var_encoding[0].literal);
  EXPECT_EQ(g3, l1.Negated());
  EXPECT_EQ(g2, g3);
  EXPECT_EQ(l1, l2);
}

TEST(IntegerEncoderDeathTest, NegatedIsNotCreatedTwice) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 10));
  const IntegerLiteral l = IntegerLiteral::GreaterOrEqual(var, IntegerValue(3));
  const Literal associated = encoder->GetOrCreateAssociatedLiteral(l);
  EXPECT_EQ(associated.Negated(),
            encoder->GetOrCreateAssociatedLiteral(l.Negated()));
}

TEST(IntegerEncoderTest, AutomaticallyDetectFullEncoding) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromValues({3, -4, 0})));

  // Adding <= min should automatically also add == min.
  encoder->GetOrCreateAssociatedLiteral(
      IntegerLiteral::LowerOrEqual(var, IntegerValue(-4)));

  // We still miss one value.
  EXPECT_FALSE(encoder->VariableIsFullyEncoded(var));
  EXPECT_FALSE(encoder->VariableIsFullyEncoded(NegationOf(var)));

  // This is enough to fully encode, because not(<=0) is >=3 which is ==3, and
  // we do have all values.
  encoder->GetOrCreateLiteralAssociatedToEquality(var, IntegerValue(0));
  EXPECT_TRUE(encoder->VariableIsFullyEncoded(var));
  EXPECT_TRUE(encoder->VariableIsFullyEncoded(NegationOf(var)));

  std::vector<int64_t> values;
  for (const auto pair : encoder->FullDomainEncoding(var)) {
    values.push_back(pair.value.value());
  }
  EXPECT_THAT(values, ElementsAre(-4, 0, 3));
}

TEST(IntegerEncoderTest, BasicFullEqualityEncoding) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromValues({3, -4, 0})));
  encoder->FullyEncodeVariable(var);

  // Normal var.
  {
    const auto& result = encoder->FullDomainEncoding(var);
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], ValueLiteralPair({IntegerValue(-4),
                                           Literal(BooleanVariable(0), true)}));
    EXPECT_EQ(result[1], ValueLiteralPair({IntegerValue(0),
                                           Literal(BooleanVariable(1), true)}));
    EXPECT_EQ(result[2],
              ValueLiteralPair(
                  {IntegerValue(3), Literal(BooleanVariable(2), false)}));
  }

  // Its negation.
  {
    const auto& result = encoder->FullDomainEncoding(NegationOf(var));
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0],
              ValueLiteralPair(
                  {IntegerValue(-3), Literal(BooleanVariable(2), false)}));
    EXPECT_EQ(result[1], ValueLiteralPair({IntegerValue(0),
                                           Literal(BooleanVariable(1), true)}));
    EXPECT_EQ(result[2], ValueLiteralPair({IntegerValue(4),
                                           Literal(BooleanVariable(0), true)}));
  }
}

TEST(IntegerEncoderTest, PartialEncodingOfBinaryVarIsFull) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromValues({0, 5})));
  const Literal lit(model.Add(NewBooleanVariable()), true);

  // Initially empty.
  EXPECT_TRUE(encoder->PartialDomainEncoding(var).empty());

  // Normal var.
  encoder->AssociateToIntegerEqualValue(lit, var, IntegerValue(0));
  {
    const auto& result = encoder->PartialDomainEncoding(var);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], ValueLiteralPair({IntegerValue(0), lit}));
    EXPECT_EQ(result[1], ValueLiteralPair({IntegerValue(5), lit.Negated()}));
  }

  // Its negation.
  {
    const auto& result = encoder->PartialDomainEncoding(NegationOf(var));
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], ValueLiteralPair({IntegerValue(-5), lit.Negated()}));
    EXPECT_EQ(result[1], ValueLiteralPair({IntegerValue(0), lit}));
  }
}

TEST(IntegerEncoderTest, PartialEncodingOfLargeVar) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 1e12));
  for (const int value : {50, 1000, 1}) {
    const Literal lit(model.Add(NewBooleanVariable()), true);
    encoder->AssociateToIntegerEqualValue(lit, var, IntegerValue(value));
  }
  const auto& result = encoder->PartialDomainEncoding(var);
  EXPECT_EQ(result.size(), 4);
  // Zero is created because encoding (== 1) requires (>= 1  and <= 1), but the
  // negation of (>= 1) is also (== 0).
  EXPECT_EQ(result[0].value, IntegerValue(0));
  EXPECT_EQ(result[1].value, IntegerValue(1));
  EXPECT_EQ(result[2].value, IntegerValue(50));
  EXPECT_EQ(result[3].value, IntegerValue(1000));
}

TEST(IntegerEncoderTest, UpdateInitialDomain) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromValues({3, -4, 0})));
  encoder->FullyEncodeVariable(var);
  EXPECT_TRUE(model.GetOrCreate<IntegerTrail>()->UpdateInitialDomain(
      var, Domain::FromIntervals({{-4, -4}, {0, 0}, {5, 5}})));

  // Note that we return the filtered encoding.
  {
    const auto& result = encoder->FullDomainEncoding(var);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], ValueLiteralPair({IntegerValue(-4),
                                           Literal(BooleanVariable(0), true)}));
    EXPECT_EQ(result[1], ValueLiteralPair({IntegerValue(0),
                                           Literal(BooleanVariable(1), true)}));
  }
}

TEST(IntegerEncoderTest, Canonicalize) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromIntervals({{1, 4}, {7, 9}})));

  EXPECT_EQ(encoder->Canonicalize(
                IntegerLiteral::GreaterOrEqual(var, IntegerValue(2))),
            std::make_pair(IntegerLiteral::GreaterOrEqual(var, IntegerValue(2)),
                           IntegerLiteral::LowerOrEqual(var, IntegerValue(1))));
  EXPECT_EQ(encoder->Canonicalize(
                IntegerLiteral::GreaterOrEqual(var, IntegerValue(4))),
            std::make_pair(IntegerLiteral::GreaterOrEqual(var, IntegerValue(4)),
                           IntegerLiteral::LowerOrEqual(var, IntegerValue(3))));
  EXPECT_EQ(
      encoder->Canonicalize(IntegerLiteral::LowerOrEqual(var, IntegerValue(4))),
      std::make_pair(IntegerLiteral::LowerOrEqual(var, IntegerValue(4)),
                     IntegerLiteral::GreaterOrEqual(var, IntegerValue(7))));
  EXPECT_EQ(
      encoder->Canonicalize(IntegerLiteral::LowerOrEqual(var, IntegerValue(6))),
      std::make_pair(IntegerLiteral::LowerOrEqual(var, IntegerValue(4)),
                     IntegerLiteral::GreaterOrEqual(var, IntegerValue(7))));
}

TEST(IntegerEncoderDeathTest, CanonicalizeDoNotAcceptTrivialLiterals) {
  if (!DEBUG_MODE) GTEST_SKIP() << "Moot in opt mode";

  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromIntervals({{1, 4}, {7, 9}})));

  EXPECT_DEATH(encoder->Canonicalize(
                   IntegerLiteral::GreaterOrEqual(var, IntegerValue(1))),
               "");
  EXPECT_DEATH(encoder->Canonicalize(
                   IntegerLiteral::GreaterOrEqual(var, IntegerValue(0))),
               "");
  EXPECT_DEATH(
      encoder->Canonicalize(IntegerLiteral::LowerOrEqual(var, IntegerValue(0))),
      "");
  EXPECT_DEATH(encoder->Canonicalize(
                   IntegerLiteral::GreaterOrEqual(var, IntegerValue(0))),
               "");

  EXPECT_DEATH(
      encoder->Canonicalize(IntegerLiteral::LowerOrEqual(var, IntegerValue(9))),
      "");
  EXPECT_DEATH(encoder->Canonicalize(
                   IntegerLiteral::LowerOrEqual(var, IntegerValue(15))),
               "");
}

TEST(IntegerEncoderTest, TrivialAssociation) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromIntervals({{1, 1}, {5, 5}})));

  {
    const Literal l(model.Add(NewBooleanVariable()), true);
    encoder->AssociateToIntegerLiteral(
        l, IntegerLiteral::GreaterOrEqual(var, IntegerValue(1)));
    EXPECT_EQ(model.Get(Value(l)), true);
  }
  {
    const Literal l(model.Add(NewBooleanVariable()), true);
    encoder->AssociateToIntegerLiteral(
        l, IntegerLiteral::GreaterOrEqual(var, IntegerValue(6)));
    EXPECT_EQ(model.Get(Value(l)), false);
  }
  {
    const Literal l(model.Add(NewBooleanVariable()), true);
    encoder->AssociateToIntegerEqualValue(l, var, IntegerValue(4));
    EXPECT_EQ(model.Get(Value(l)), false);
  }
}

TEST(IntegerEncoderTest, TrivialAssociationWithFixedVariable) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(Domain(1)));
  {
    const Literal l(model.Add(NewBooleanVariable()), true);
    encoder->AssociateToIntegerEqualValue(l, var, IntegerValue(1));
    EXPECT_EQ(model.Get(Value(l)), true);
  }
}

TEST(IntegerEncoderTest, FullEqualityEncodingForTwoValuesWithDuplicates) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      model.Add(NewIntegerVariable(Domain::FromValues({3, 5, 3})));
  encoder->FullyEncodeVariable(var);

  // Normal var.
  {
    const auto& result = encoder->FullDomainEncoding(var);
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], ValueLiteralPair({IntegerValue(3),
                                           Literal(BooleanVariable(0), true)}));
    EXPECT_EQ(result[1],
              ValueLiteralPair(
                  {IntegerValue(5), Literal(BooleanVariable(0), false)}));
  }

  // Its negation.
  {
    const auto& result = encoder->FullDomainEncoding(NegationOf(var));
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0],
              ValueLiteralPair(
                  {IntegerValue(-5), Literal(BooleanVariable(0), false)}));
    EXPECT_EQ(result[1], ValueLiteralPair({IntegerValue(-3),
                                           Literal(BooleanVariable(0), true)}));
  }
}

#define EXPECT_BOUNDS_EQ(var, lb, ub)        \
  EXPECT_EQ(model.Get(LowerBound(var)), lb); \
  EXPECT_EQ(model.Get(UpperBound(var)), ub)

TEST(IntegerEncoderTest, IntegerTrailToEncodingPropagation) {
  Model model;
  SatSolver* sat_solver = model.GetOrCreate<SatSolver>();
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  Trail* trail = model.GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();

  const IntegerVariable var = model.Add(
      NewIntegerVariable(Domain::FromIntervals({{3, 4}, {7, 7}, {9, 9}})));
  model.Add(FullyEncodeVariable(var));

  // We copy this because Enqueue() might change it.
  const auto encoding = encoder->FullDomainEncoding(var);

  // Initial propagation is correct.
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_BOUNDS_EQ(var, 3, 9);

  // Note that the bounds snap to the possible values.
  const VariablesAssignment& assignment = trail->Assignment();
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::LowerOrEqual(var, IntegerValue(8)), {}, {}));
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_TRUE(assignment.LiteralIsFalse(encoding[3].literal));
  EXPECT_FALSE(assignment.VariableIsAssigned(encoding[0].literal.Variable()));
  EXPECT_FALSE(assignment.VariableIsAssigned(encoding[1].literal.Variable()));
  EXPECT_FALSE(assignment.VariableIsAssigned(encoding[2].literal.Variable()));
  EXPECT_BOUNDS_EQ(var, 3, 7);

  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(5)), {}, {}));
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_TRUE(assignment.LiteralIsFalse(encoding[0].literal));
  EXPECT_TRUE(assignment.LiteralIsFalse(encoding[1].literal));
  EXPECT_TRUE(assignment.LiteralIsTrue(encoding[2].literal));
  EXPECT_BOUNDS_EQ(var, 7, 7);

  // Encoding[2] will become true on the sat solver propagation.
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_TRUE(assignment.LiteralIsTrue(encoding[2].literal));
}

TEST(IntegerEncoderTest, EncodingToIntegerTrailPropagation) {
  Model model;
  SatSolver* sat_solver = model.GetOrCreate<SatSolver>();
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  Trail* trail = model.GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  const IntegerVariable var = model.Add(
      NewIntegerVariable(Domain::FromIntervals({{3, 4}, {7, 7}, {9, 9}})));
  model.Add(FullyEncodeVariable(var));
  const auto& encoding = encoder->FullDomainEncoding(var);

  // Initial propagation is correct.
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_BOUNDS_EQ(var, 3, 9);

  // We remove the value 4, nothing happen.
  trail->SetDecisionLevel(1);
  trail->EnqueueSearchDecision(encoding[1].literal.Negated());
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_BOUNDS_EQ(var, 3, 9);

  // When we remove 3, the lower bound change though.
  trail->SetDecisionLevel(2);
  trail->EnqueueSearchDecision(encoding[0].literal.Negated());
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_BOUNDS_EQ(var, 7, 9);

  // The reason for the lower bounds is that both encoding[0] and encoding[1]
  // are false. But it is captured by the literal associated to x >= 7.
  {
    const IntegerLiteral l = integer_trail->LowerBoundAsLiteral(var);
    EXPECT_EQ(integer_trail->ReasonFor(l),
              std::vector<Literal>{
                  Literal(encoder->GetAssociatedLiteral(l)).Negated()});
  }

  // Test the other direction.
  trail->SetDecisionLevel(3);
  trail->EnqueueSearchDecision(encoding[3].literal.Negated());
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_BOUNDS_EQ(var, 7, 7);
  {
    const IntegerLiteral l = integer_trail->UpperBoundAsLiteral(var);
    EXPECT_EQ(integer_trail->ReasonFor(l),
              std::vector<Literal>{
                  Literal(encoder->GetAssociatedLiteral(l)).Negated()});
  }
}

TEST(IntegerEncoderTest, IsFixedOrHasAssociatedLiteral) {
  Model model;
  SatSolver* sat_solver = model.GetOrCreate<SatSolver>();
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(
      NewIntegerVariable(Domain::FromIntervals({{3, 4}, {7, 7}, {9, 9}})));

  // Initial propagation is correct.
  EXPECT_TRUE(sat_solver->Propagate());
  EXPECT_BOUNDS_EQ(var, 3, 9);

  // These are trivially true/false.
  EXPECT_TRUE(encoder->IsFixedOrHasAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, 2)));
  EXPECT_TRUE(encoder->IsFixedOrHasAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, 3)));
  EXPECT_TRUE(encoder->IsFixedOrHasAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, 10)));

  // Not other encoding currently.
  EXPECT_FALSE(encoder->IsFixedOrHasAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, 4)));
  EXPECT_FALSE(encoder->IsFixedOrHasAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, 9)));

  // Add one encoding and test.
  encoder->GetOrCreateAssociatedLiteral(IntegerLiteral::GreaterOrEqual(var, 7));
  EXPECT_TRUE(encoder->IsFixedOrHasAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, 5)));
  EXPECT_TRUE(encoder->IsFixedOrHasAssociatedLiteral(
      IntegerLiteral::GreaterOrEqual(var, 7)));
  EXPECT_TRUE(encoder->IsFixedOrHasAssociatedLiteral(
      IntegerLiteral::LowerOrEqual(var, 6)));
  EXPECT_TRUE(encoder->IsFixedOrHasAssociatedLiteral(
      IntegerLiteral::LowerOrEqual(var, 4)));
}

TEST(IntegerEncoderTest, EncodingOfConstantVariableHasSizeOne) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(7, 7));
  model.Add(FullyEncodeVariable(var));
  const auto& encoding = encoder->FullDomainEncoding(var);
  EXPECT_EQ(encoding.size(), 1);
  EXPECT_TRUE(model.GetOrCreate<Trail>()->Assignment().LiteralIsTrue(
      encoding[0].literal));
}

TEST(IntegerEncoderTest, IntegerVariableOfAssignedLiteralIsFixed) {
  Model model;
  SatSolver* sat_solver = model.GetOrCreate<SatSolver>();

  {
    Literal literal_false = Literal(sat_solver->NewBooleanVariable(), true);
    CHECK(sat_solver->AddUnitClause(literal_false.Negated()));
    const IntegerVariable zero =
        model.Add(NewIntegerVariableFromLiteral(literal_false));
    EXPECT_EQ(model.Get(UpperBound(zero)), 0);
  }

  {
    Literal literal_true = Literal(sat_solver->NewBooleanVariable(), true);
    CHECK(sat_solver->AddUnitClause(literal_true));
    const IntegerVariable one =
        model.Add(NewIntegerVariableFromLiteral(literal_true));
    EXPECT_EQ(model.Get(LowerBound(one)), 1);
  }
}

TEST(IntegerEncoderTest, LiteralView1) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 1));
  const Literal literal(model.Add(NewBooleanVariable()), true);
  encoder->AssociateToIntegerEqualValue(literal, var, IntegerValue(1));
  EXPECT_EQ(var, encoder->GetLiteralView(literal));
  EXPECT_EQ(kNoIntegerVariable, encoder->GetLiteralView(literal.Negated()));
}

TEST(IntegerEncoderTest, LiteralView2) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 1));
  const Literal literal(model.Add(NewBooleanVariable()), true);
  encoder->AssociateToIntegerEqualValue(literal, var, IntegerValue(0));
  EXPECT_EQ(kNoIntegerVariable, encoder->GetLiteralView(literal));
  EXPECT_EQ(var, encoder->GetLiteralView(literal.Negated()));
}

TEST(IntegerEncoderTest, LiteralView3) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 1));
  const Literal literal(model.Add(NewBooleanVariable()), true);
  encoder->AssociateToIntegerLiteral(
      literal, IntegerLiteral::GreaterOrEqual(var, IntegerValue(1)));
  EXPECT_EQ(var, encoder->GetLiteralView(literal));
  EXPECT_EQ(kNoIntegerVariable, encoder->GetLiteralView(literal.Negated()));
}

TEST(IntegerEncoderTest, LiteralView4) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var = model.Add(NewIntegerVariable(0, 1));
  const Literal literal(model.Add(NewBooleanVariable()), true);
  encoder->AssociateToIntegerLiteral(
      literal, IntegerLiteral::LowerOrEqual(var, IntegerValue(0)));
  EXPECT_EQ(kNoIntegerVariable, encoder->GetLiteralView(literal));
  EXPECT_EQ(var, encoder->GetLiteralView(literal.Negated()));
}

TEST(IntegerEncoderTest, IssueWhenNotFullyingPropagatingAtLoading) {
  Model model;
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  const IntegerVariable var =
      integer_trail->AddIntegerVariable(Domain::FromValues({0, 3, 7, 9}));
  const Literal false_literal = integer_encoder->GetFalseLiteral();
  integer_encoder->DisableImplicationBetweenLiteral();

  // This currently doesn't propagate the domain.
  integer_encoder->AssociateToIntegerLiteral(
      false_literal, IntegerLiteral::GreaterOrEqual(var, IntegerValue(5)));
  EXPECT_EQ(integer_trail->LowerBound(var), 0);
  EXPECT_EQ(integer_trail->UpperBound(var), 9);

  // And that used to fail because it does some domain propagation when it
  // detect that some value cannot be there and update the domains of var while
  // iterating over it.
  integer_encoder->FullyEncodeVariable(var);
}

#undef EXPECT_BOUNDS_EQ

TEST(SolveIntegerProblemWithLazyEncodingTest, Sat) {
  static const int kNumVariables = 10;
  Model model;
  std::vector<IntegerVariable> integer_vars;
  for (int i = 0; i < kNumVariables; ++i) {
    integer_vars.push_back(model.Add(NewIntegerVariable(0, 10)));
  }
  model.GetOrCreate<SearchHeuristics>()->fixed_search =
      FirstUnassignedVarAtItsMinHeuristic(integer_vars, &model);
  ConfigureSearchHeuristics(&model);
  ASSERT_EQ(model.GetOrCreate<IntegerSearchHelper>()->SolveIntegerProblem(),
            SatSolver::Status::FEASIBLE);
  for (const IntegerVariable var : integer_vars) {
    EXPECT_EQ(model.Get(LowerBound(var)), model.Get(UpperBound(var)));
  }
}

TEST(SolveIntegerProblemWithLazyEncodingTest, Unsat) {
  Model model;
  const IntegerVariable var = model.Add(NewIntegerVariable(-100, 100));
  model.Add(LowerOrEqual(var, -10));
  model.Add(GreaterOrEqual(var, 10));
  model.GetOrCreate<SearchHeuristics>()->fixed_search =
      FirstUnassignedVarAtItsMinHeuristic({var}, &model);
  ConfigureSearchHeuristics(&model);
  EXPECT_EQ(model.GetOrCreate<IntegerSearchHelper>()->SolveIntegerProblem(),
            SatSolver::Status::INFEASIBLE);
}

TEST(IntegerTrailTest, InitialVariableDomainIsUpdated) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  const IntegerVariable var =
      integer_trail->AddIntegerVariable(IntegerValue(0), IntegerValue(1000));
  EXPECT_EQ(integer_trail->InitialVariableDomain(var), Domain(0, 1000));
  EXPECT_EQ(integer_trail->InitialVariableDomain(NegationOf(var)),
            Domain(-1000, 0));

  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(7)), {}, {}));
  EXPECT_EQ(integer_trail->InitialVariableDomain(var), Domain(7, 1000));
  EXPECT_EQ(integer_trail->InitialVariableDomain(NegationOf(var)),
            Domain(-1000, -7));
}

TEST(IntegerTrailTest, AppendNewBounds) {
  Model model;
  const Literal l(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var(model.Add(NewIntegerVariable(0, 100)));

  // So that there is a decision.
  EXPECT_TRUE(
      model.GetOrCreate<SatSolver>()->EnqueueDecisionIfNotConflicting(l));

  // Enqueue a bunch of fact.
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(2)), {l.Negated()}, {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(4)), {l.Negated()}, {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(8)), {l.Negated()}, {}));
  EXPECT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::GreaterOrEqual(var, IntegerValue(9)), {l.Negated()}, {}));

  // Only the last bound should be present.
  std::vector<IntegerLiteral> bounds;
  integer_trail->AppendNewBounds(&bounds);
  EXPECT_THAT(bounds, ElementsAre(IntegerLiteral::GreaterOrEqual(
                          var, IntegerValue(9))));
}

static void BM_FloorRatio(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    benchmark::DoNotOptimize(test += FloorRatio(dividend, divisor));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

static void BM_PositiveRemainder(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    benchmark::DoNotOptimize(test += PositiveRemainder(dividend, divisor));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

static void BM_PositiveRemainderAlternative(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    benchmark::DoNotOptimize(test += dividend -
                                     divisor * FloorRatio(dividend, divisor));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

// What we use in the code. This is safe of integer overflow. The compiler
// should also do a single integer division to get the quotient and remainder.
static void BM_DivisionAndRemainder(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    benchmark::DoNotOptimize(test += FloorRatio(dividend, divisor));
    benchmark::DoNotOptimize(test += PositiveRemainder(dividend, divisor));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

// An alternative version, note however that divisor * f might overflow!
static void BM_DivisionAndRemainderAlternative(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    const IntegerValue f = FloorRatio(dividend, divisor);
    benchmark::DoNotOptimize(test += f);
    benchmark::DoNotOptimize(test += dividend - divisor * f);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

// The best we can hope for ?
static void BM_DivisionAndRemainderBaseline(benchmark::State& state) {
  IntegerValue divisor(654676436498);
  IntegerValue dividend(45454655155444);
  IntegerValue test(0);
  for (auto _ : state) {
    dividend++;
    divisor++;
    benchmark::DoNotOptimize(test += dividend / divisor);
    benchmark::DoNotOptimize(test += dividend % divisor);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_FloorRatio);
BENCHMARK(BM_PositiveRemainder);
BENCHMARK(BM_PositiveRemainderAlternative);
BENCHMARK(BM_DivisionAndRemainder);
BENCHMARK(BM_DivisionAndRemainderAlternative);
BENCHMARK(BM_DivisionAndRemainderBaseline);

}  // namespace
}  // namespace sat
}  // namespace operations_research
