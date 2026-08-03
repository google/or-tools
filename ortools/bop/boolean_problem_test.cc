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

#include "ortools/bop/boolean_problem.h"

#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace bop {
namespace {

using ::operations_research::sat::Coefficient;
using ::operations_research::sat::kCoefficientMax;
using ::operations_research::sat::Literal;
using ::operations_research::sat::LiteralWithCoeff;

using ::testing::ContainerEq;

std::vector<LiteralWithCoeff> MakePb(
    absl::Span<const std::pair<int, Coefficient>> input) {
  std::vector<LiteralWithCoeff> result;
  result.reserve(input.size());
  for (const auto p : input) {
    result.push_back({Literal(p.first), p.second});
  }
  return result;
}

TEST(CanonicalBooleanLinearProblem, BasicTest) {
  auto cst = MakePb({{+1, 4}, {+2, -5}, {+3, 6}, {-4, 7}});
  CanonicalBooleanLinearProblem problem;
  problem.AddLinearConstraint(true, Coefficient(-5), true, Coefficient(5),
                              &cst);

  // We have just one constraint because the >= -5 is always true.
  EXPECT_EQ(1, problem.NumConstraints());
  const auto result0 = MakePb({{+1, 4}, {-2, 5}, {+3, 6}, {-4, 7}});
  EXPECT_EQ(problem.Rhs(0), 10);
  EXPECT_THAT(problem.Constraint(0), ContainerEq(result0));

  // So lets restrict it and only use the lower bound
  // Note that the API destroy the input so we have to reconstruct it.
  cst = MakePb({{+1, 4}, {+2, -5}, {+3, 6}, {-4, 7}});
  problem.AddLinearConstraint(true, Coefficient(-4), false,
                              /*unused*/ Coefficient(-10), &cst);

  // Now we have another constraint corresponding to the >= -4 constraint.
  EXPECT_EQ(2, problem.NumConstraints());
  const auto result1 = MakePb({{-1, 4}, {+2, 5}, {-3, 6}, {+4, 7}});
  EXPECT_EQ(problem.Rhs(1), 21);
  EXPECT_THAT(problem.Constraint(1), ContainerEq(result1));
}

TEST(CanonicalBooleanLinearProblem, BasicTest2) {
  auto cst = MakePb({{+1, 1}, {+2, 2}});
  CanonicalBooleanLinearProblem problem;
  problem.AddLinearConstraint(true, Coefficient(2), false,
                              /*unused*/ Coefficient(0), &cst);

  EXPECT_EQ(1, problem.NumConstraints());
  const auto result = MakePb({{-1, 1}, {-2, 2}});
  EXPECT_EQ(problem.Rhs(0), 1);
  EXPECT_THAT(problem.Constraint(0), ContainerEq(result));
}

TEST(CanonicalBooleanLinearProblem, OverflowCases) {
  auto cst = MakePb({});
  CanonicalBooleanLinearProblem problem;
  for (int i = 0; i < 2; ++i) {
    std::vector<LiteralWithCoeff> reference;
    if (i == 0) {
      // This is a constraint with a "bound shift" of 10.
      reference = MakePb({{+1, -10}, {+2, 10}});
    } else {
      // This is a constraint with a "bound shift" of -10 since its domain value
      // is actually [10, 10].
      reference = MakePb({{+1, 10}, {-1, 10}});
    }

    // All these constraints are trivially satisfiable, so no new constraints
    // should be added.
    cst = reference;
    EXPECT_TRUE(problem.AddLinearConstraint(true, -kCoefficientMax, true,
                                            kCoefficientMax, &cst));
    cst = reference;
    EXPECT_TRUE(problem.AddLinearConstraint(true, -kCoefficientMax - 1, true,
                                            kCoefficientMax, &cst));
    cst = reference;
    EXPECT_TRUE(problem.AddLinearConstraint(true, Coefficient(-10), true,
                                            Coefficient(10), &cst));

    // These are trivially unsat, and all AddLinearConstraint() should return
    // false.
    cst = reference;
    EXPECT_FALSE(problem.AddLinearConstraint(true, kCoefficientMax, true,
                                             kCoefficientMax, &cst));
    cst = reference;
    EXPECT_FALSE(problem.AddLinearConstraint(true, -kCoefficientMax, true,
                                             -kCoefficientMax, &cst));
    cst = reference;
    EXPECT_FALSE(problem.AddLinearConstraint(
        true, -kCoefficientMax, true, -kCoefficientMax - Coefficient(1), &cst));
  }

  // No constraints were actually added.
  EXPECT_EQ(problem.NumConstraints(), 0);
}

}  // namespace
}  // namespace bop
}  // namespace operations_research
