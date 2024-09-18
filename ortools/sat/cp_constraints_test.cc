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

#include "ortools/sat/cp_constraints.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

TEST(LiteralXorIsTest, OneVariable) {
  Model model;
  const BooleanVariable a = model.Add(NewBooleanVariable());
  const BooleanVariable b = model.Add(NewBooleanVariable());
  model.Add(LiteralXorIs({Literal(a, true)}, true));
  model.Add(LiteralXorIs({Literal(b, true)}, false));
  SatSolver* solver = model.GetOrCreate<SatSolver>();
  EXPECT_TRUE(solver->Propagate());
  EXPECT_TRUE(solver->Assignment().LiteralIsTrue(Literal(a, true)));
  EXPECT_TRUE(solver->Assignment().LiteralIsFalse(Literal(b, true)));
}

// A simple macro to make the code more readable.
#define EXPECT_BOUNDS_EQ(var, lb, ub)        \
  EXPECT_EQ(model.Get(LowerBound(var)), lb); \
  EXPECT_EQ(model.Get(UpperBound(var)), ub)

TEST(PartialIsOneOfVarTest, MinMaxPropagation) {
  Model model;
  const IntegerVariable target_var = model.Add(NewIntegerVariable(-10, 20));
  std::vector<IntegerVariable> vars;
  std::vector<Literal> selectors;
  for (int i = 0; i < 10; ++i) {
    vars.push_back(model.Add(ConstantIntegerVariable(i)));
    selectors.push_back(Literal(model.Add(NewBooleanVariable()), true));
  }
  model.Add(PartialIsOneOfVar(target_var, vars, selectors));

  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_BOUNDS_EQ(target_var, 0, 9);

  model.Add(ClauseConstraint({selectors[0].Negated()}));
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_BOUNDS_EQ(target_var, 1, 9);

  model.Add(ClauseConstraint({selectors[8].Negated()}));
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_BOUNDS_EQ(target_var, 1, 9);

  model.Add(ClauseConstraint({selectors[9].Negated()}));
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_BOUNDS_EQ(target_var, 1, 7);
}

TEST(GreaterThanAtLeastOneOfPropagatorTest, BasicTest) {
  for (int i = 0; i < 2; ++i) {
    Model model;

    // We create a simple model with 3 variables and 2 conditional precedences.
    // We only add the GreaterThanAtLeastOneOfPropagator() for i == 1.
    const IntegerVariable a = model.Add(NewIntegerVariable(0, 3));
    const IntegerVariable b = model.Add(NewIntegerVariable(0, 3));
    const IntegerVariable c = model.Add(NewIntegerVariable(0, 3));
    const Literal ac = Literal(model.Add(NewBooleanVariable()), true);
    const Literal bc = Literal(model.Add(NewBooleanVariable()), true);
    model.Add(ConditionalLowerOrEqualWithOffset(a, c, 3, ac));
    model.Add(ConditionalLowerOrEqualWithOffset(b, c, 2, bc));
    model.Add(ClauseConstraint({ac, bc}));
    if (i == 1) {
      model.Add(GreaterThanAtLeastOneOf(
          c, {a, b}, {IntegerValue(3), IntegerValue(2)}, {ac, bc}, {}));
    }

    // Test that we do propagate more with the extra propagator.
    EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
    EXPECT_EQ(model.Get(LowerBound(c)), i == 0 ? 0 : 2);

    // Test that we find all solutions.
    int num_solutions = 0;
    while (true) {
      const auto status = SolveIntegerProblemWithLazyEncoding(&model);
      if (status != SatSolver::Status::FEASIBLE) break;
      ++num_solutions;
      VLOG(1) << model.Get(Value(a)) << " " << model.Get(Value(b)) << " "
              << model.Get(Value(c));
      model.Add(ExcludeCurrentSolutionAndBacktrack());
    }
    EXPECT_EQ(num_solutions, 18);
  }
}

#undef EXPECT_BOUNDS_EQ

}  // namespace
}  // namespace sat
}  // namespace operations_research
