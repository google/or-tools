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

#include "ortools/sat/intervals.h"

#include <stdint.h>

#include "gtest/gtest.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_literal.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

TEST(IntervalsRepositoryTest, Precedences) {
  Model model;
  const AffineExpression start1(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size1(model.Add(NewIntegerVariable(2, 10)));
  const AffineExpression end1(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression start2(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size2(model.Add(NewIntegerVariable(2, 10)));
  const AffineExpression end2(model.Add(NewIntegerVariable(0, 10)));

  auto* repo = model.GetOrCreate<IntervalsRepository>();
  const IntervalVariable a = repo->CreateInterval(start1, end1, size1);
  const IntervalVariable b = repo->CreateInterval(start2, end2, size2);

  // OK to call many times.
  repo->CreateDisjunctivePrecedenceLiteral(a, b);
  repo->CreateDisjunctivePrecedenceLiteral(a, b);

  EXPECT_NE(kNoLiteralIndex,
            repo->GetPrecedenceLiteral(repo->End(a), repo->Start(b)));
  EXPECT_EQ(Literal(repo->GetPrecedenceLiteral(repo->End(a), repo->Start(b))),
            Literal(repo->GetPrecedenceLiteral(repo->End(b), repo->Start(a)))
                .Negated());
}

TEST(IntervalsRepositoryTest, PrecedencesWithZeroMinSize) {
  Model model;
  const AffineExpression start1(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size1(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression end1(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression start2(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size2(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression end2(model.Add(NewIntegerVariable(0, 10)));

  auto* repo = model.GetOrCreate<IntervalsRepository>();
  const IntervalVariable a = repo->CreateInterval(start1, end1, size1);
  const IntervalVariable b = repo->CreateInterval(start2, end2, size2);

  repo->CreateDisjunctivePrecedenceLiteral(a, b);

  // When both intervals can have size 0, End(a) <= Start(b) and
  // End(b) <= Start(a) can both hold simultaneously, so the negation of one
  // must not be registered as the reified encoding of the other.
  EXPECT_NE(kNoLiteralIndex,
            repo->GetPrecedenceLiteral(repo->End(a), repo->Start(b)));
  EXPECT_EQ(kNoLiteralIndex,
            repo->GetPrecedenceLiteral(repo->End(b), repo->Start(a)));
}

TEST(IntervalsRepositoryTest, PrecedencesWithDuplicateInterval) {
  {
    Model model;
    const IntegerVariable size_var = model.Add(NewIntegerVariable(0, 10));
    const AffineExpression start(model.Add(NewIntegerVariable(0, 10)));
    const AffineExpression size(size_var);
    const AffineExpression end(model.Add(NewIntegerVariable(0, 10)));

    auto* repo = model.GetOrCreate<IntervalsRepository>();
    const IntervalVariable a = repo->CreateInterval(start, end, size);
    repo->CreateDisjunctivePrecedenceLiteral(a, a);

    auto* sat_solver = model.GetOrCreate<SatSolver>();
    EXPECT_TRUE(sat_solver->Propagate());
    EXPECT_FALSE(sat_solver->ModelIsUnsat());
    EXPECT_EQ(model.Get(UpperBound(size_var)), 0);
  }
  {
    Model model;
    const AffineExpression start(model.Add(NewIntegerVariable(0, 10)));
    const AffineExpression size(model.Add(NewIntegerVariable(2, 10)));
    const AffineExpression end(model.Add(NewIntegerVariable(0, 10)));

    auto* repo = model.GetOrCreate<IntervalsRepository>();
    const IntervalVariable a = repo->CreateInterval(start, end, size);
    repo->CreateDisjunctivePrecedenceLiteral(a, a);

    EXPECT_TRUE(model.GetOrCreate<SatSolver>()->ModelIsUnsat());
  }
}

TEST(IntervalsRepositoryTest,
     AssignedPrecedencesAtPositiveLevelDoNotMarkModelUnsat) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_new_linear_propagation(false);

  const AffineExpression start1(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size1(IntegerValue(2));
  const AffineExpression end1(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression start2(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size2(IntegerValue(2));
  const AffineExpression end2(model.Add(NewIntegerVariable(0, 10)));

  auto* repo = model.GetOrCreate<IntervalsRepository>();
  const IntervalVariable a = repo->CreateInterval(start1, end1, size1);
  const IntervalVariable b = repo->CreateInterval(start2, end2, size2);

  const Literal ab =
      repo->GetOrCreatePrecedenceLiteral(repo->End(a), repo->Start(b));
  const Literal ba =
      repo->GetOrCreatePrecedenceLiteral(repo->End(b), repo->Start(a));

  auto* sat_solver = model.GetOrCreate<SatSolver>();
  ASSERT_TRUE(sat_solver->Propagate());
  ASSERT_NE(sat_solver->EnqueueDecisionAndBackjumpOnConflict(ab.Negated()),
            kUnsatTrailIndex);
  ASSERT_NE(sat_solver->EnqueueDecisionAndBackjumpOnConflict(ba.Negated()),
            kUnsatTrailIndex);
  ASSERT_EQ(sat_solver->CurrentDecisionLevel(), 2);

  // Both precedence literals are assigned false under search decisions while
  // Linear2Bounds::GetStatus() is still IS_UNKNOWN; this must not mark the
  // feasible model globally UNSAT.
  repo->CreateDisjunctivePrecedenceLiteral(a, b);
  EXPECT_FALSE(sat_solver->ModelIsUnsat());
  EXPECT_TRUE(sat_solver->ResetToLevelZero());
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
