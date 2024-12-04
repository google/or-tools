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

#include "ortools/sat/probing.h"

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

TEST(ProbeBooleanVariablesTest, IntegerBoundInference) {
  Model model;
  const BooleanVariable a = model.Add(NewBooleanVariable());
  const IntegerVariable b = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable c = model.Add(NewIntegerVariable(0, 10));

  // Bound restriction.
  model.Add(Implication({Literal(a, true)},
                        IntegerLiteral::GreaterOrEqual(b, IntegerValue(2))));
  model.Add(Implication({Literal(a, false)},
                        IntegerLiteral::GreaterOrEqual(b, IntegerValue(3))));
  model.Add(Implication({Literal(a, true)},
                        IntegerLiteral::LowerOrEqual(b, IntegerValue(7))));
  model.Add(Implication({Literal(a, false)},
                        IntegerLiteral::LowerOrEqual(b, IntegerValue(9))));

  // Hole.
  model.Add(Implication({Literal(a, true)},
                        IntegerLiteral::GreaterOrEqual(c, IntegerValue(7))));
  model.Add(Implication({Literal(a, false)},
                        IntegerLiteral::LowerOrEqual(c, IntegerValue(4))));

  Prober* prober = model.GetOrCreate<Prober>();
  prober->ProbeBooleanVariables(/*deterministic_time_limit=*/1.0);
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  EXPECT_EQ("[2,9]", integer_trail->InitialVariableDomain(b).ToString());
  EXPECT_EQ("[0,4][7,10]", integer_trail->InitialVariableDomain(c).ToString());
}

TEST(FailedLiteralProbingRoundTest, TrivialExample) {
  Model model;
  const Literal a(model.Add(NewBooleanVariable()), true);
  const Literal b(model.Add(NewBooleanVariable()), true);
  const Literal c(model.Add(NewBooleanVariable()), true);

  // Setting a to false will result in a constradiction, so a must be true.
  model.Add(ClauseConstraint({a, b, c}));
  model.Add(Implication(a.Negated(), b.Negated()));
  model.Add(Implication(c, a));

  auto* sat_soler = model.GetOrCreate<SatSolver>();
  EXPECT_TRUE(sat_soler->Propagate());
  EXPECT_FALSE(sat_soler->Assignment().LiteralIsAssigned(a));

  EXPECT_TRUE(FailedLiteralProbingRound(ProbingOptions(), &model));
  EXPECT_TRUE(sat_soler->Assignment().LiteralIsTrue(a));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
