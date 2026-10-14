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

#include "ortools/util/sorted_interval_list.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/numeric/int128.h"
#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/types.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace {

using ::testing::Optional;

TEST(ClosedIntervalConstructorTest, DFatalInNonOptIfStartGreaterThanEnd) {
  if (!DEBUG_MODE) GTEST_SKIP() << "See BehavesAsEmptyIntervalInOptMode";
  EXPECT_DEBUG_DEATH(ClosedInterval(1, 0), "Invalid");
  EXPECT_DEBUG_DEATH(ClosedInterval(-55, -56), "Invalid");
  EXPECT_DEBUG_DEATH(ClosedInterval(kint64max, kint64min), "Invalid");
}

TEST(ClosedIntervaliteratorSet, TestRangeLoop) {
  static const ClosedInterval kIntervals[] = {
      {0, 0},
      {0, 1},
      {1000, 1000},
      {0, 100},
      {-10, 20},
      {kint64max, kint64max},
      {kint64max - 30, kint64max},
      {kint64min, kint64min},
      {kint64min, kint64min + 3},
  };
  for (const ClosedInterval& interval : kIntervals) {
    SCOPED_TRACE(DUMP_VARS(interval));
    bool had_at_least_one_iteration = false;
    int64_t last_value = 0;
    for (const int64_t value : interval) {
      if (!had_at_least_one_iteration) {
        EXPECT_EQ(value, interval.start);
        had_at_least_one_iteration = true;
      } else {
        EXPECT_EQ(value - last_value, 1);
      }
      last_value = value;
    }
    EXPECT_TRUE(had_at_least_one_iteration);
    EXPECT_EQ(last_value, interval.end);
  }
}

TEST(IntervalsAreSortedAndNonAdjacentTest, BasicTests) {
  EXPECT_TRUE(IntervalsAreSortedAndNonAdjacent({}));
  EXPECT_TRUE(IntervalsAreSortedAndNonAdjacent({{1, 1}}));
  EXPECT_TRUE(IntervalsAreSortedAndNonAdjacent({{1, 1}, {3, 6}}));
  EXPECT_FALSE(IntervalsAreSortedAndNonAdjacent({{3, 6}, {1, 5}}));
  EXPECT_FALSE(IntervalsAreSortedAndNonAdjacent({{1, 1}, {2, 6}}));
  EXPECT_FALSE(IntervalsAreSortedAndNonAdjacent({{1, 2}, {3, 6}}));
}

TEST(InvalidIntervalsTest, BehavesAsEmptyIntervalInOptMode) {
  if (DEBUG_MODE) GTEST_SKIP() << "See DFatalInNonOptIfStartGreaterThanEnd";
  EXPECT_LT(ClosedInterval(1, 0), ClosedInterval(3, 2));
  EXPECT_FALSE(IntervalsAreSortedAndNonAdjacent({{1, 0}}));
  EXPECT_EQ("[]", Domain(0, 10).IntersectionWith(Domain(50, -40)).ToString());
}

TEST(IntervalsAreSortedAndNonAdjacentTest, NoOverflow) {
  EXPECT_TRUE(IntervalsAreSortedAndNonAdjacent({{kint64min, kint64max}}));
  EXPECT_TRUE(IntervalsAreSortedAndNonAdjacent(
      {{kint64min, kint64min}, {kint64max, kint64max}}));
  EXPECT_FALSE(IntervalsAreSortedAndNonAdjacent(
      {{kint64min, kint64max}, {kint64max, kint64max}}));
  EXPECT_FALSE(IntervalsAreSortedAndNonAdjacent(
      {{kint64min, kint64min}, {kint64min, kint64max}}));
}

TEST(DomainTest, ToStringTest) {
  EXPECT_EQ("[]", Domain::FromValues({}).ToString());
  EXPECT_EQ("[4]", Domain::FromValues({4}).ToString());
  EXPECT_EQ("[-4]", Domain::FromValues({-4}).ToString());
  EXPECT_EQ("[1,9]",
            Domain::FromValues({1, 2, 3, 4, 5, 9, 8, 7, 6}).ToString());
  EXPECT_EQ("[1,5][7,9]",
            Domain::FromValues({9, 8, 7, 1, 2, 3, 4, 5}).ToString());
  EXPECT_EQ("[1,3][5][7][9,10]",
            Domain::FromValues({1, 2, 3, 5, 7, 9, 10}).ToString());
  EXPECT_EQ("[1,3]", Domain::FromValues({1, 1, 3, 1, 2, 3, 2, 3}).ToString());
}

TEST(DomainTest, FromIntervals) {
  EXPECT_EQ("[1,5][7,9]", Domain::FromIntervals({{1, 5}, {7, 9}}).ToString());
  EXPECT_DEBUG_DEATH(
      EXPECT_EQ("[1,5]", Domain::FromIntervals({{1, 5}, {9, 7}}).ToString()),
      "Invalid");
  EXPECT_DEBUG_DEATH(
      EXPECT_EQ("[]", Domain::FromIntervals({{5, 1}, {9, 7}}).ToString()),
      "Invalid");
}

TEST(DomainTest, FromVectorIntervals) {
  EXPECT_EQ("[1,5][7,9]",
            Domain::FromVectorIntervals({{1, 5}, {7, 9}}).ToString());
  EXPECT_DEBUG_DEATH(
      EXPECT_EQ("[1,5]",
                Domain::FromVectorIntervals({{1, 5}, {9, 7}}).ToString()),
      "Invalid");
  EXPECT_DEBUG_DEATH(
      EXPECT_EQ("[]", Domain::FromVectorIntervals({{5, 1}, {9, 7}}).ToString()),
      "Invalid");
}

TEST(DomainTest, FromFlatSpanOfIntervals) {
  EXPECT_EQ("[1,5][7,9]",
            Domain::FromFlatSpanOfIntervals({1, 5, 7, 9}).ToString());
  EXPECT_DEBUG_DEATH(
      EXPECT_EQ("[1,5]",
                Domain::FromFlatSpanOfIntervals({1, 5, 9, 7}).ToString()),
      "Invalid");
  EXPECT_DEBUG_DEATH(
      EXPECT_EQ("[]", Domain::FromFlatSpanOfIntervals({5, 1, 9, 7}).ToString()),
      "Invalid");
}

TEST(DomainTest, LeOrGe) {
  EXPECT_EQ("[1,9223372036854775807]", Domain::GreaterOrEqual(1).ToString());
  EXPECT_EQ("[-9223372036854775808,3]", Domain::LowerOrEqual(3).ToString());
}

TEST(DomainTest, Contains) {
  const Domain d = Domain::FromIntervals({{-4, -3}, {0, 0}, {7, kint64max}});
  EXPECT_TRUE(d.Contains(-4));
  EXPECT_TRUE(d.Contains(-3));
  EXPECT_TRUE(d.Contains(0));
  EXPECT_TRUE(d.Contains(7));
  EXPECT_TRUE(d.Contains(10));
  EXPECT_TRUE(d.Contains(kint64max));
  EXPECT_FALSE(d.Contains(-7));
  EXPECT_FALSE(d.Contains(-5));
  EXPECT_FALSE(d.Contains(-1));
  EXPECT_FALSE(d.Contains(1));
  EXPECT_FALSE(d.Contains(6));
}

TEST(DomainTest, Distance) {
  const Domain d = Domain::FromIntervals({{-4, -3}, {0, 0}, {7, kint64max}});
  EXPECT_EQ(d.Distance(-4), 0);
  EXPECT_EQ(d.Distance(-8), 4);
  EXPECT_EQ(d.Distance(-3), 0);
  EXPECT_EQ(d.Distance(-2), 1);
  EXPECT_EQ(d.Distance(-1), 1);
  EXPECT_EQ(d.Distance(0), 0);
  EXPECT_EQ(d.Distance(1), 1);
  EXPECT_EQ(d.Distance(2), 2);
  EXPECT_EQ(d.Distance(3), 3);
  EXPECT_EQ(d.Distance(4), 3);
  EXPECT_EQ(d.Distance(5), 2);
  EXPECT_EQ(d.Distance(6), 1);
  EXPECT_EQ(d.Distance(7), 0);
  EXPECT_EQ(d.Distance(8), 0);
}

TEST(DomainTest, ValueIteration) {
  const Domain d = Domain::FromIntervals({{-4, -3}, {0, 0}, {7, 10}});
  std::vector<int64_t> test;
  for (const int64_t v : d.Values()) test.push_back(v);
  EXPECT_THAT(test, ::testing::ElementsAre(-4, -3, 0, 7, 8, 9, 10));
}

TEST(DomainTest, EmptyValueIteration) {
  std::vector<int64_t> test;
  for (const int64_t v : Domain().Values()) {
    test.push_back(v);
  }
  EXPECT_TRUE(test.empty());
}

TEST(DomainTest, IterationOverTemporaryWorks) {
  std::vector<int64_t> test;
  for (const int64_t v : Domain::FromValues({0, 1, 3, 5, 15, 27}).Values()) {
    test.push_back(v);
  }
  EXPECT_THAT(test, ::testing::ElementsAre(0, 1, 3, 5, 15, 27));
}

TEST(DomainTest, LargerDomainIteration) {
  int num_values = 0;
  int64_t sum = 0;
  for (const int64_t v :
       Domain(1, 1000).UnionWith(Domain(2001, 4000)).Values()) {
    sum += v;
    ++num_values;
  }
  EXPECT_EQ(sum, 6501500);  // Just to use v.
  EXPECT_EQ(num_values, 3000);
}

TEST(DomainTest, ContainsLarge) {
  std::vector<ClosedInterval> intervals;
  for (int i = 0; i < 100000; ++i) {
    intervals.push_back(ClosedInterval(3 * i, 3 * i + 1));
  }
  const Domain d = Domain::FromIntervals(intervals);
  for (int i = 0; i < 300000; ++i) {
    EXPECT_EQ(i % 3 != 2, d.Contains(i));
  }
}

TEST(DomainTest, Size) {
  EXPECT_EQ(4, Domain(1, 4).Size());
  EXPECT_EQ(3, Domain::FromIntervals({{-4, -3}, {0, 0}}).Size());
  EXPECT_EQ(kint64max - 6, Domain(7, kint64max).Size());
  EXPECT_EQ(kint64max - 3,
            Domain::FromIntervals({{-4, -3}, {0, 0}, {7, kint64max}}).Size());
  EXPECT_EQ(kint64max, Domain(-5, kint64max - 2).Size());
  EXPECT_EQ(kint64max, Domain(kint64min + 4, 10).Size());
}

TEST(DomainTest, HasTwoValues) {
  EXPECT_FALSE(Domain().HasTwoValues());
  EXPECT_FALSE(Domain(1, 4).HasTwoValues());
  EXPECT_TRUE(Domain(0, 1).HasTwoValues());
  EXPECT_TRUE(Domain(1, 2).HasTwoValues());
  EXPECT_TRUE(Domain::FromValues({1, 4}).HasTwoValues());
  EXPECT_FALSE(Domain::FromValues({1, 3, 4}).HasTwoValues());
}

TEST(DomainTest, SizeIsCapped) {
  std::vector<ClosedInterval> intervals;
  intervals.push_back({kint64min, -10});
  intervals.push_back({10, kint64max});
  const Domain domain = Domain::FromIntervals(intervals);
  EXPECT_EQ(kint64max, domain.Size());
}

TEST(DomainTest, IsIncludedIn) {
  EXPECT_TRUE(Domain(0, 4).IsIncludedIn(Domain(0, 4)));
  EXPECT_FALSE(Domain(0, 4).IsIncludedIn(Domain(0, 3)));
  EXPECT_FALSE(Domain(0, 4).IsIncludedIn(Domain(1, 4)));
  EXPECT_TRUE(Domain::FromValues({1, 4, 7}).IsIncludedIn(
      Domain::FromIntervals({{0, 2}, {4, 4}, {6, 9}})));
  EXPECT_TRUE(Domain::FromValues({1, 7}).IsIncludedIn(
      Domain::FromIntervals({{0, 2}, {4, 4}, {6, 9}})));
  EXPECT_FALSE(Domain::FromValues({1, 5}).IsIncludedIn(
      Domain::FromIntervals({{0, 2}, {4, 4}, {6, 9}})));
}

TEST(DomainTest, OverlapsWith) {
  EXPECT_TRUE(Domain(0, 4).OverlapsWith(Domain(0, 4)));
  EXPECT_FALSE(Domain(0, 2).OverlapsWith(Domain(3, 5)));
  EXPECT_FALSE(Domain::FromValues({1, 3, 5}).OverlapsWith(
      Domain::FromValues({0, 2, 4})));
  EXPECT_TRUE(Domain::FromValues({1, 2, 3, 5})
                  .OverlapsWith(Domain::FromValues({0, 2, 4})));
}

TEST(DomainTest, IntersectionWithEmptyCases) {
  EXPECT_EQ("[]", Domain().IntersectionWith({}).ToString());
  EXPECT_EQ("[]", Domain().IntersectionWith({0, 10}).ToString());
  EXPECT_EQ("[]", Domain(0, 10).IntersectionWith({}).ToString());
  EXPECT_EQ("[]", Domain(0, 10).IntersectionWith(Domain(11, 15)).ToString());
}

TEST(DomainTest, IntersectionWithSimpleCases) {
  EXPECT_EQ("[0]", Domain(-10, 0).IntersectionWith(Domain(0, 10)).ToString());
  EXPECT_EQ("[0][10]", Domain::FromIntervals({{-10, 0}, {10, 20}})
                           .IntersectionWith(Domain(0, 10))
                           .ToString());
  EXPECT_EQ("[5,7]", Domain(5, 10).IntersectionWith(Domain(0, 7)).ToString());
  EXPECT_EQ("[5,10]",
            Domain(5, 10)
                .IntersectionWith(Domain::FromIntervals({{0, 15}, {18, 18}}))
                .ToString());
  EXPECT_EQ("[5,10]", Domain(0, 10).IntersectionWith(Domain(5, 15)).ToString());
  EXPECT_EQ("[1][4][6]", Domain(0, 8)
                             .IntersectionWith(Domain::FromValues({1, 4, 6, 9}))
                             .ToString());
}

TEST(DomainTest, IntersectionWithListOfValuesCase) {
  EXPECT_EQ("[0][5,6][8]",
            Domain::FromValues({-10, 0, 1, 5, 6, 7, 8, 10})
                .IntersectionWith(Domain::FromValues({-1, 0, 3, 5, 6, 8, 9}))
                .ToString());
}

TEST(DomainTest, UniqueValueNotIn) {
  // Difference is {0, 5} (multiple values) -> returns std::nullopt
  EXPECT_EQ(Domain(0, 5).UniqueValueNotIn(Domain(1, 4)), std::nullopt);

  // Difference is {0} (exactly one value) -> returns 0
  EXPECT_THAT(Domain(0, 5).UniqueValueNotIn(Domain(1, 7)), Optional(0));

  // Difference is {8} (exactly one value) -> returns 8
  EXPECT_THAT(Domain::FromValues({2, 8}).UniqueValueNotIn(Domain(1, 7)),
              Optional(8));

  // Fully covered / Subset: difference is empty -> returns std::nullopt
  EXPECT_EQ(Domain(2, 3).UniqueValueNotIn(Domain(1, 5)), std::nullopt);

  // Identical domains: difference is empty -> returns std::nullopt
  EXPECT_EQ(Domain(1, 5).UniqueValueNotIn(Domain(1, 5)), std::nullopt);

  // Exactly one value at the upper bound not covered
  EXPECT_THAT(Domain(1, 5).UniqueValueNotIn(Domain(1, 4)), Optional(5));

  // Exactly one value at the lower bound not covered
  EXPECT_THAT(Domain(1, 5).UniqueValueNotIn(Domain(2, 5)), Optional(1));

  // Disjoint domain with a single value -> returns that value
  EXPECT_THAT(Domain(0, 0).UniqueValueNotIn(Domain(1, 5)), Optional(0));

  // Disjoint domain with multiple values -> returns std::nullopt
  EXPECT_EQ(Domain(8, 9).UniqueValueNotIn(Domain(1, 5)), std::nullopt);

  // Fragmented Domain (built from values) with exactly one value not in other
  EXPECT_THAT(Domain::FromValues({1, 5, 9}).UniqueValueNotIn(Domain(0, 7)),
              Optional(9));

  // Fragmented Domain with multiple values not in other ({1, 9})
  EXPECT_EQ(Domain::FromValues({1, 5, 9}).UniqueValueNotIn(Domain(3, 7)),
            std::nullopt);

  // Empty domain vs empty domain (assuming Domain() or similar creates an empty
  // domain)
  EXPECT_EQ(Domain().UniqueValueNotIn(Domain()), std::nullopt);
}

TEST(Domain, UniqueValueNotInOverflowCase) {
  // 1. Excluded interval ends exactly at kint64max.
  EXPECT_EQ(Domain(kint64max - 5, kint64max)
                .UniqueValueNotIn(Domain(kint64max - 5, kint64max)),
            std::nullopt);

  // 2. The only remaining value is exactly kint64max.
  EXPECT_THAT(Domain(kint64max - 5, kint64max)
                  .UniqueValueNotIn(Domain(kint64max - 5, kint64max - 1)),
              Optional(kint64max));

  // 3. The remaining value is right next to kint64max.
  EXPECT_THAT(Domain(kint64max - 1, kint64max)
                  .UniqueValueNotIn(Domain(kint64max, kint64max)),
              Optional(kint64max - 1));

  // 4. Two values remaining at the extreme upper edge.
  // Ensures the `return std::nullopt` optimization fires safely at the
  // boundary.
  EXPECT_EQ(Domain(kint64max - 1, kint64max).UniqueValueNotIn(Domain(0, 0)),
            std::nullopt);

  // 5. Exclude the entire representable integer space.
  EXPECT_EQ(Domain(kint64max, kint64max)
                .UniqueValueNotIn(Domain(kint64min, kint64max)),
            std::nullopt);

  // 6. Lower bound safety (INT64_MIN).
  // Ensures subtraction or logic doesn't underflow at the bottom boundary.
  EXPECT_THAT(Domain(kint64min, kint64min + 5)
                  .UniqueValueNotIn(Domain(kint64min + 1, kint64min + 5)),
              Optional(kint64min));
}

TEST(DomainTest, UnionWithOverflowCase) {
  EXPECT_EQ(Domain(1, kint64max).UnionWith(Domain({18, kint64max})),
            Domain(1, kint64max));
}

TEST(DomainTest, UnionWithListOfValuesCase) {
  EXPECT_EQ("[-1,0][5,6][8][10]",
            Domain::FromValues({0, 6, 8})
                .UnionWith(Domain::FromValues({-1, 0, 5, 10}))
                .ToString());
}

TEST(DomainTest, ComplementEmptyCases) {
  EXPECT_EQ(Domain().Complement(), Domain::AllValues());
  EXPECT_EQ(Domain::AllValues().Complement(), Domain());
}

TEST(DomainTest, ComplementCornerCases) {
  EXPECT_EQ(Domain(kint64min + 1, kint64max),
            Domain::FromValues({kint64min}).Complement());
  EXPECT_EQ(Domain(kint64min + 1, kint64max).Complement(),
            Domain::FromValues({kint64min}));
  EXPECT_EQ(Domain(kint64min, kint64max - 1),
            Domain::FromValues({kint64max}).Complement());
  EXPECT_EQ(Domain(kint64min, kint64max - 1).Complement(),
            Domain::FromValues({kint64max}));
}

TEST(DomainTest, ComplementNormalCases) {
  EXPECT_EQ(Domain::FromIntervals({{kint64min, -1}, {1, kint64max}}),
            Domain::FromValues({0}).Complement());

  // The function is involutive.
  EXPECT_EQ(Domain::FromIntervals({{kint64min, -3}, {3, 3}, {7, kint64max}}),
            Domain::FromIntervals({{-2, 2}, {4, 6}}).Complement());
  EXPECT_EQ(Domain::FromIntervals({{-2, 2}, {4, 6}}),
            Domain::FromIntervals({{kint64min, -3}, {3, 3}, {7, kint64max}})
                .Complement());
}

TEST(DomainTest, NegationNormalCases) {
  EXPECT_EQ(Domain().Negation(), Domain());
  EXPECT_EQ(Domain::FromIntervals({{-5, 0}, {7, 7}}).Negation(),
            Domain::FromIntervals({{-7, -7}, {0, 5}}));
}

TEST(DomainTest, NegationCornerCases) {
  EXPECT_EQ(Domain::AllValues().Negation(), Domain(-kint64max, kint64max));
  EXPECT_EQ(Domain::FromValues({kint64min}).Negation(), Domain());
  EXPECT_EQ(Domain(kint64max).Negation(), Domain(-kint64max));
  EXPECT_EQ(
      Domain::FromIntervals({{kint64min, kint64min}, {-5, -2}}).Negation(),
      Domain(2, 5));
}

TEST(DomainTest, ContinuousMultiplicationByBasicCases) {
  EXPECT_EQ(Domain().ContinuousMultiplicationBy(3), Domain());
  EXPECT_EQ(Domain(5, 7).ContinuousMultiplicationBy(3), Domain(15, 21));
  EXPECT_EQ(Domain(5, 7).ContinuousMultiplicationBy(-2), Domain(-14, -10));
  EXPECT_EQ(Domain::AllValues().ContinuousMultiplicationBy(-2),
            Domain(-kint64max, kint64max));
}

TEST(DomainTest, ContinuousMultiplicationBetweenDomains) {
  EXPECT_EQ(Domain().ContinuousMultiplicationBy(Domain(2, 3)), Domain());
  EXPECT_EQ(Domain(5, 7).ContinuousMultiplicationBy(Domain(2, 3)),
            Domain(10, 21));
  EXPECT_EQ(Domain(5, 7).ContinuousMultiplicationBy(Domain(-2, 5)),
            Domain(-14, 35));
  EXPECT_EQ(Domain(-5, 7).ContinuousMultiplicationBy(Domain(-10, 5)),
            Domain(-70, 50));
}

TEST(DomainTest, DivisionByBasicCases) {
  EXPECT_EQ(Domain().DivisionBy(3), Domain());
  EXPECT_EQ(Domain(5, 7).DivisionBy(3), Domain(1, 2));
  EXPECT_EQ(Domain(5, 7).DivisionBy(-2), Domain(-3, -2));
  EXPECT_EQ(Domain::AllValues().DivisionBy(-2),
            Domain(-(kint64max / 2), -(kint64min / 2)));
}

TEST(DomainTest, MultiplicationByBasicCase) {
  bool exact;

  EXPECT_EQ(Domain().MultiplicationBy(3, &exact), Domain());
  EXPECT_TRUE(exact);

  EXPECT_EQ(Domain::FromIntervals({{-2, 2}, {4, 5}})
                .MultiplicationBy(3, &exact)
                .ToString(),
            "[-6][-3][0][3][6][12][15]");
  EXPECT_TRUE(exact);

  EXPECT_EQ(Domain(-20000, 10000).MultiplicationBy(3, &exact),
            Domain(-60000, 30000));
  EXPECT_FALSE(exact);

  EXPECT_EQ(Domain(-20000, 10000).MultiplicationBy(-1, &exact),
            Domain(-10000, 20000));
  EXPECT_TRUE(exact);
}

TEST(DomainTest, MultiplicationByZero) {
  bool exact;
  EXPECT_EQ(Domain(5, 7).MultiplicationBy(0, &exact), Domain(0, 0));
  EXPECT_TRUE(exact);
  EXPECT_EQ(Domain(0, 0).MultiplicationBy(0, &exact), Domain(0, 0));
  EXPECT_TRUE(exact);
  EXPECT_EQ(Domain().MultiplicationBy(0, &exact), Domain());
  EXPECT_TRUE(exact);
}

TEST(DomainTest, MultiplicationByAndOverflow) {
  bool exact;

  // Interestingly kint64max = 7 * 7 * 73 * 127 * 337 * 92737 * 649657
  EXPECT_EQ(Domain(kint64max / 7).MultiplicationBy(7, &exact),
            Domain(kint64max));
  EXPECT_EQ(Domain(kint64max / 337).MultiplicationBy(337, &exact),
            Domain(kint64max));

  // And for kint64min it is just -2^63. But if we multiply by a negative
  // number, we will always loose kint64min due to the implementation. It is
  // probably not a big deal in our context.
  EXPECT_EQ(Domain(kint64min / 8).MultiplicationBy(8, &exact),
            Domain(kint64min));
  EXPECT_EQ(Domain(-(kint64min / 8)).MultiplicationBy(-8, &exact), Domain());

  // Depending on how we iteration on a domain value, it is easy to cause an
  // overflow. This used to fail because of the value++.
  EXPECT_EQ(Domain(kint64max).MultiplicationBy(2), Domain());
}

TEST(DomainTest, AdditionWithBasicCases) {
  EXPECT_EQ(Domain().AdditionWith({2, 3}), Domain());
  EXPECT_EQ(Domain(3, 4).AdditionWith({}), Domain());
  EXPECT_EQ(Domain::FromIntervals({{1, 2}, {4, 4}}).AdditionWith({0, 3}),
            Domain(1, 7));
}

TEST(DomainTest, AdditionWithPositiveOverflow) {
  EXPECT_EQ(Domain(-3, 4).AdditionWith(Domain(kint64max)),
            Domain(kint64max - 3, kint64max));
  EXPECT_EQ(Domain(0).AdditionWith(Domain(kint64max)), Domain(kint64max));
  EXPECT_TRUE(Domain(1).AdditionWith(Domain(kint64max)).IsEmpty());
}

TEST(DomainTest, AdditionWithNegativeOverflow) {
  EXPECT_EQ(Domain(-3, 4).AdditionWith(Domain(kint64min)),
            Domain(kint64min, kint64min + 4));
  EXPECT_EQ(Domain(0).AdditionWith(Domain(kint64min)), Domain(kint64min));
  EXPECT_TRUE(Domain(-1).AdditionWith(Domain(kint64min)).IsEmpty());
}

TEST(DomainTest, InverseMultiplicationByEmptyCases) {
  EXPECT_EQ(Domain().InverseMultiplicationBy(3), Domain());
  EXPECT_EQ(Domain(5, 7).InverseMultiplicationBy(4), Domain());
  EXPECT_EQ(Domain(5, 7).InverseMultiplicationBy(8), Domain());
}

TEST(DomainTest, InverseMultiplicationByZeroCoeff) {
  EXPECT_EQ(Domain(-1, 0).InverseMultiplicationBy(0), Domain::AllValues());
  EXPECT_EQ(Domain(1, 7).InverseMultiplicationBy(0), Domain());
}

TEST(DomainTest, InverseMultiplicationByNormalCases) {
  EXPECT_EQ(Domain::FromIntervals({{-5, -1}, {1, 1}, {3, 8}})
                .InverseMultiplicationBy(2),
            Domain::FromIntervals({{-2, -1}, {2, 4}}));
  EXPECT_EQ(Domain::FromIntervals({{-5, -1}, {1, 1}, {3, 8}})
                .InverseMultiplicationBy(3),
            Domain::FromIntervals({{-1, -1}, {1, 2}}));
  EXPECT_EQ(Domain::FromIntervals({{2, 2}}).InverseMultiplicationBy(2),
            Domain::FromIntervals({{1, 1}}));
  EXPECT_EQ(Domain::FromIntervals({{2, 2}, {4, 4}, {6, 7}})
                .InverseMultiplicationBy(2),
            Domain::FromIntervals({{1, 3}}));
}

TEST(DomainTest, InverseMultiplicationByNegativeCases) {
  EXPECT_EQ(Domain(-5, -1).InverseMultiplicationBy(-2), Domain(1, 2));
  EXPECT_EQ(Domain(-5, 8).InverseMultiplicationBy(-2), Domain(-4, 2));
  EXPECT_EQ(Domain(4, 8).InverseMultiplicationBy(-2), Domain(-4, -2));
}

TEST(DomainTest, InverseMultiplicationByNoOverflowCases) {
  EXPECT_EQ(Domain::AllValues().InverseMultiplicationBy(1000),
            Domain(kint64min / 1000, kint64max / 1000));
  EXPECT_EQ(Domain::FromValues({kint64max}).InverseMultiplicationBy(2),
            Domain());
  EXPECT_EQ(Domain::FromValues({kint64min}).InverseMultiplicationBy(2),
            Domain::FromValues({kint64min / 2}));
}

TEST(DomainTest, SimplifyUsingImpliedDomain) {
  EXPECT_TRUE(Domain(-5, 7).SimplifyUsingImpliedDomain(Domain()).IsEmpty());
  EXPECT_TRUE(Domain(-5, 7).SimplifyUsingImpliedDomain(Domain(8, 9)).IsEmpty());
  EXPECT_EQ(Domain::FromIntervals({{-5, -1}, {1, 1}})
                .SimplifyUsingImpliedDomain(
                    Domain::FromIntervals({{-5, -1}, {1, 1}})),
            Domain(-5, 1));
  EXPECT_EQ(Domain(-10, 10).SimplifyUsingImpliedDomain(
                Domain::FromIntervals({{-5, -1}, {1, 1}})),
            Domain(-5, 1));
  EXPECT_EQ(Domain(1, 2).SimplifyUsingImpliedDomain(Domain(-10, 10)),
            Domain(1, 2));
  EXPECT_EQ(
      Domain::FromIntervals({{1, 2}, {7, 9}})
          .SimplifyUsingImpliedDomain(Domain::FromIntervals({{0, 4}, {6, 10}})),
      Domain::FromIntervals({{1, 2}, {7, 9}}));
  EXPECT_EQ(
      Domain::FromIntervals({{1, 2}, {7, 9}})
          .SimplifyUsingImpliedDomain(Domain::FromIntervals({{0, 2}, {7, 10}})),
      Domain(1, 9));
  EXPECT_EQ(
      Domain::FromIntervals({{1, 2}, {7, 9}})
          .SimplifyUsingImpliedDomain(Domain::FromIntervals({{0, 2}, {7, 8}})),
      Domain(1, 8));
}

TEST(DomainTest, SumOfKMinValueInDomain) {
  const Domain d = Domain::FromIntervals({{1, 3}, {5, 5}, {7, 9}});
  EXPECT_EQ(0, SumOfKMinValueInDomain(d, -1));
  EXPECT_EQ(0, SumOfKMinValueInDomain(d, 0));
  EXPECT_EQ(1, SumOfKMinValueInDomain(d, 1));
  EXPECT_EQ(3, SumOfKMinValueInDomain(d, 2));
  EXPECT_EQ(11, SumOfKMinValueInDomain(d, 4));
  EXPECT_EQ(18, SumOfKMinValueInDomain(d, 5));
  EXPECT_EQ(35, SumOfKMinValueInDomain(d, 7));
  EXPECT_EQ(35, SumOfKMinValueInDomain(d, 8));
}

TEST(DomainTest, SumOfKMaxValueInDomain) {
  const Domain d = Domain::FromIntervals({{1, 3}, {5, 5}, {7, 9}});
  EXPECT_EQ(0, SumOfKMaxValueInDomain(d, -1));
  EXPECT_EQ(0, SumOfKMaxValueInDomain(d, 0));
  EXPECT_EQ(9, SumOfKMaxValueInDomain(d, 1));
  EXPECT_EQ(17, SumOfKMaxValueInDomain(d, 2));
  EXPECT_EQ(29, SumOfKMaxValueInDomain(d, 4));
  EXPECT_EQ(32, SumOfKMaxValueInDomain(d, 5));
  EXPECT_EQ(35, SumOfKMaxValueInDomain(d, 7));
  EXPECT_EQ(35, SumOfKMaxValueInDomain(d, 8));
}

TEST(DomainTest, SmallestValue) {
  EXPECT_EQ(0, Domain(-2, 3).SmallestValue());
  EXPECT_EQ(0, Domain(0, 3).SmallestValue());
  EXPECT_EQ(1, Domain::FromIntervals({{-2, -1}, {1, 2}}).SmallestValue());
  EXPECT_EQ(-1, Domain::FromIntervals({{-2, -1}, {2, 2}}).SmallestValue());
}

TEST(DomainTest, PartAroundZero) {
  EXPECT_EQ(Domain(-2, 3), Domain(-2, 3).PartAroundZero());
  EXPECT_EQ(Domain(), Domain(1, 3).PartAroundZero());
  EXPECT_EQ(
      Domain(0, 1),
      Domain::FromIntervals({{-2, -1}, {2, 2}}).Complement().PartAroundZero());
}

TEST(DomainTest, ClosestValue) {
  EXPECT_EQ(1, Domain(-2, 3).ClosestValue(1));
  EXPECT_EQ(-2, Domain(-2, 3).ClosestValue(-2));
  EXPECT_EQ(-2, Domain(-2, 3).ClosestValue(-10));
  EXPECT_EQ(3, Domain(-2, 3).ClosestValue(7));
  EXPECT_EQ(1, Domain::FromIntervals({{-2, -1}, {1, 2}}).ClosestValue(0));
}

TEST(DomainTest, ValueAtOrBefore) {
  EXPECT_EQ(1, Domain(-2, 3).ValueAtOrBefore(1));
  EXPECT_EQ(3, Domain(-2, 3).ValueAtOrBefore(7));
  EXPECT_EQ(-1, Domain::FromIntervals({{-2, -1}, {1, 2}}).ValueAtOrBefore(0));
  EXPECT_EQ(1, Domain::FromIntervals({{-2, -1}, {1, 2}}).ValueAtOrBefore(1));

  // Error = no change.
  EXPECT_EQ(-3, Domain::FromIntervals({{-2, -1}, {1, 2}}).ValueAtOrBefore(-3));
}

TEST(DomainTest, ValueAtOrAfter) {
  EXPECT_EQ(1, Domain(-2, 3).ValueAtOrAfter(1));
  EXPECT_EQ(-2, Domain(-2, 3).ValueAtOrAfter(-7));
  EXPECT_EQ(1, Domain::FromIntervals({{-2, -1}, {1, 2}}).ValueAtOrAfter(0));
  EXPECT_EQ(1, Domain::FromIntervals({{-2, -1}, {1, 2}}).ValueAtOrAfter(1));
  EXPECT_EQ(-2, Domain::FromIntervals({{-2, -1}, {1, 2}}).ValueAtOrAfter(-3));

  // Error = no change.
  EXPECT_EQ(8, Domain::FromIntervals({{-2, -1}, {1, 2}}).ValueAtOrAfter(8));
}

TEST(DomainTest, PositiveModuloBySuperset) {
  EXPECT_EQ(Domain().PositiveModuloBySuperset(Domain(1, 3)), Domain());
  EXPECT_EQ(Domain(-5, 2).PositiveModuloBySuperset(Domain(1, 5)),
            Domain(-4, 2));
  EXPECT_EQ(Domain(2, 3).PositiveModuloBySuperset(Domain(7)), Domain(2, 3));
}

TEST(DomainTest, SupersetWhenModuloIsNotFixed) {
  // Complexity is <= 30 ^ 3 * 10 ^ 3 = 27 Millions.
  const int64_t var_range = 30;
  const int64_t mod_range = 10;
  for (int64_t min = -var_range; min <= var_range; ++min) {
    for (int64_t max = min; max <= var_range; ++max) {
      for (int64_t mod_min = 1; mod_min < mod_range; ++mod_min) {
        for (int64_t mod_max = mod_min; mod_max < mod_range; ++mod_max) {
          const Domain result = Domain(min, max).PositiveModuloBySuperset(
              Domain(mod_min, mod_max));
          int64_t min_seen = mod_range;
          int64_t max_seen = -mod_range;
          for (int64_t v = min; v <= max; ++v) {
            for (int64_t m = mod_min; m <= mod_max; ++m) {
              const int64_t seen = v % m;
              min_seen = std::min(min_seen, seen);
              max_seen = std::max(max_seen, seen);
            }
          }
          ASSERT_NE(min_seen, mod_range);  // There is no empty domain here.

          // We are correct, but not exact.
          ASSERT_GE(min_seen, result.Min());
          ASSERT_LE(max_seen, result.Max());
        }
      }
    }
  }
}

TEST(DomainTest, ExactWhenModuloIsFixed) {
  const int64_t range = 50;
  for (int64_t min = -range; min <= range; ++min) {
    for (int64_t max = min; max <= range; ++max) {
      for (int64_t mod = 1; mod < range; ++mod) {
        const Domain result =
            Domain(min, max).PositiveModuloBySuperset(Domain(mod));
        int64_t min_seen = range;
        int64_t max_seen = -range;
        for (int64_t v = min; v <= max; ++v) {
          const int64_t seen = v % mod;
          min_seen = std::min(min_seen, seen);
          max_seen = std::max(max_seen, seen);
        }
        ASSERT_NE(min_seen, range);  // There is no empty domain here.
        ASSERT_EQ(min_seen, result.Min());
        ASSERT_EQ(max_seen, result.Max());
      }
    }
  }
}

void BruteForceCheckDivisionBySuperset(const Domain& domain,
                                       const Domain& divisor) {
  const Domain superset = domain.DivisionBySuperset(divisor);

  /// Check that all values are inside the superset.
  std::vector<int64_t> values;
  values.reserve(domain.Size() * divisor.Size());
  for (int64_t v : domain.Values()) {
    for (int64_t d : divisor.Values()) {
      if (d == 0) continue;
      absl::int128 res = absl::int128{v} / d;
      res = std::min(res, absl::int128{kint64max});
      res = std::max(res, absl::int128{kint64min});
      ASSERT_TRUE(superset.Contains(static_cast<int64_t>(res)));
      values.push_back(static_cast<int64_t>(res));
    }
  }

  // Check that the min/max is reached.
  if (values.empty()) {
    ASSERT_TRUE(superset.IsEmpty());
    return;
  }
  const int64_t min_value = *std::min_element(values.begin(), values.end());
  const int64_t max_value = *std::max_element(values.begin(), values.end());
  ASSERT_EQ(min_value, superset.Min());
  ASSERT_EQ(max_value, superset.Max());
}

TEST(DomainTest, DivisionBySuperset) {
  // Empty.
  EXPECT_EQ(Domain().DivisionBySuperset(Domain(1, 3)), Domain());
  EXPECT_EQ(Domain(-5, 2).DivisionBySuperset(Domain()), Domain());

  // ++/++
  EXPECT_EQ(Domain(2, 5).DivisionBySuperset(Domain(1, 5)), Domain(0, 5));
  EXPECT_EQ(Domain(4, 7).DivisionBySuperset(Domain(2, 3)), Domain(1, 3));

  // -+/++
  EXPECT_EQ(Domain(-5, 2).DivisionBySuperset(Domain(1, 5)), Domain(-5, 2));
  EXPECT_EQ(Domain(-7, 4).DivisionBySuperset(Domain(2, 3)), Domain(-3, 2));

  // ++/--
  EXPECT_EQ(Domain(2, 5).DivisionBySuperset(Domain(-5, -1)), Domain(-5, 0));
  EXPECT_EQ(Domain(4, 7).DivisionBySuperset(Domain(-3, -2)), Domain(-3, -1));

  // -+/--
  EXPECT_EQ(Domain(-5, 2).DivisionBySuperset(Domain(-5, -1)), Domain(-2, 5));
  EXPECT_EQ(Domain(-7, 4).DivisionBySuperset(Domain(-3, -2)), Domain(-2, 3));

  // ++/-+, with 0 in the divisor domain.
  EXPECT_EQ(Domain(2, 5).DivisionBySuperset(Domain(-5, 1)),
            Domain::FromFlatSpanOfIntervals({-5, 0, 2, 5}));
  EXPECT_EQ(Domain(4, 7).DivisionBySuperset(Domain(-3, 2)),
            Domain::FromFlatSpanOfIntervals({-7, -1, 2, 7}));

  // ++/-+, with 0 not in the divisor domain.
  EXPECT_EQ(Domain(4, 7).DivisionBySuperset(
                Domain::FromFlatSpanOfIntervals({-5, -2, 2, 3})),
            Domain(-3, 3));

  // --/-0
  EXPECT_EQ(Domain(-7, -4).DivisionBySuperset(Domain(-5, 0)), Domain(0, 7));

  // --/0+
  EXPECT_EQ(Domain(-7, -4).DivisionBySuperset(Domain(0, 5)), Domain(-7, 0));

  // --/00
  EXPECT_EQ(Domain(kint64min, kint64min).DivisionBySuperset(Domain(0, 0)),
            Domain());

  // Edge case: INT_MIN/-1 overflows INT_MAX.
  EXPECT_EQ(Domain(kint64min, kint64min).DivisionBySuperset(Domain(-1, -1)),
            Domain(kint64max, kint64max));

  for (int64_t a = -10; a < 10; ++a) {
    for (int64_t b = -10; b < 10; ++b) {
      for (int64_t c = -10; c < 10; ++c) {
        for (int64_t d = -10; d < 10; ++d) {
          BruteForceCheckDivisionBySuperset(Domain(a, b), Domain(c, d));
        }
      }
    }
  }
}

TEST(DomainTest, SquareSuperSetTest) {
  EXPECT_EQ(
      Domain(-40, 70).SquareSuperset(),
      Domain::FromValues({0,    1,    4,    9,    16,   25,   36,   49,   64,
                          81,   100,  121,  144,  169,  196,  225,  256,  289,
                          324,  361,  400,  441,  484,  529,  576,  625,  676,
                          729,  784,  841,  900,  961,  1024, 1089, 1156, 1225,
                          1296, 1369, 1444, 1521, 1600, 1681, 1764, 1849, 1936,
                          2025, 2116, 2209, 2304, 2401, 2500, 2601, 2704, 2809,
                          2916, 3025, 3136, 3249, 3364, 3481, 3600, 3721, 3844,
                          3969, 4096, 4225, 4356, 4489, 4624, 4761, 4900}));
  EXPECT_EQ(Domain::FromFlatIntervals({5, 10, 100, 200}).SquareSuperset(),
            Domain::FromFlatIntervals({25, 100, 10000, 40000}));
  EXPECT_EQ(Domain::FromFlatIntervals({5, 10, 100, int64_t{1} << 35})
                .SquareSuperset(),
            Domain::FromFlatIntervals({25, 100, 10000, kint64max}));
  EXPECT_EQ(Domain::FromValues({100, int64_t{1} << 35}).SquareSuperset(),
            Domain::FromValues({10000, kint64max}));
}

Domain NaiveQuadraticSuperset(absl::int128 a, absl::int128 b, absl::int128 c,
                              absl::int128 d, const Domain& domain) {
  std::vector<int64_t> values;
  values.reserve(domain.Size());
  for (const int64_t value : domain.Values()) {
    absl::int128 res = (a * value + b) * (c * value + d);
    res = std::min(res, absl::int128{kint64max});
    res = std::max(res, absl::int128{kint64min});
    values.push_back(static_cast<int64_t>(res));
  }
  return Domain::FromValues(std::move(values)).RelaxIfTooComplex();
}

TEST(DomainTest, QuadraticSupersetTest) {
  std::array<int64_t, 4> values[] = {
      {0, 1, 4, 9}, {1, 1, 1, -1}, {-2, 2, -2, 2}, {-50, -50, 50, 50}};
  for (int start = -10; start < 10; start++) {
    for (int size = 0; size < 10; size++) {
      const Domain domain =
          Domain::FromFlatIntervals({100 * start, 100 * start + 100 * size});
      for (const auto& value : values) {
        const Domain expected = NaiveQuadraticSuperset(
            value[0], value[1], value[2], value[3], domain);
        const Domain actual =
            domain.QuadraticSuperset(value[0], value[1], value[2], value[3]);
        EXPECT_EQ(expected.Min(), actual.Min());
        EXPECT_EQ(expected.Max(), actual.Max());
      }
    }
  }

  for (const auto& value : values) {
    const int64_t int_max = kint32max;
    const Domain domain = Domain::FromFlatIntervals({int_max - 200, int_max});
    const Domain expected =
        NaiveQuadraticSuperset(value[0], value[1], value[2], value[3], domain);
    const Domain actual =
        domain.QuadraticSuperset(value[0], value[1], value[2], value[3]);
    EXPECT_EQ(expected.IsEmpty(), actual.IsEmpty());
    if (actual.IsEmpty()) continue;
    EXPECT_EQ(expected.Min(), actual.Min());
    EXPECT_EQ(expected.Max(), actual.Max());
  }
}

TEST(DomainTest, MultipleIntMax) {
  const int64_t int_max = kint64max;
  const Domain with_int_max =
      Domain::FromValues({0, 3, 5, int_max, 2, int_max});
  EXPECT_EQ(with_int_max.Size(), 5);
  EXPECT_EQ(with_int_max, Domain::FromIntervals(
                              {{0, 0}, {2, 3}, {5, 5}, {int_max, int_max}}));
}

TEST(SortedDisjointIntervalListTest, ConstructionFromVector) {
  std::vector<ClosedInterval> domain_intervals;
  for (const ClosedInterval& interval :
       Domain::FromValues({1, 2, 3, 7, 8, -4})) {
    domain_intervals.push_back(interval);
  }
  SortedDisjointIntervalList intervals(domain_intervals);
  EXPECT_EQ("[-4][1,3][7,8]", intervals.DebugString());
}

TEST(SortedDisjointIntervalListTest, Simplification) {
  // Insert in the correct order.
  std::vector<int64_t> s1 = {0, 20};
  std::vector<int64_t> e1 = {10, 30};
  SortedDisjointIntervalList i1(s1, e1);
  EXPECT_EQ(i1.DebugString(), "[0,10][20,30]");

  // Reverse intervals.
  std::vector<int64_t> s2 = {20, 0};
  std::vector<int64_t> e2 = {30, 10};
  SortedDisjointIntervalList i2;
  i2.InsertIntervals(s2, e2);
  EXPECT_EQ(i2.DebugString(), "[0,10][20,30]");

  SortedDisjointIntervalList i2b;
  i2b.InsertInterval(20, 30);
  i2b.InsertInterval(0, 10);
  EXPECT_EQ(i2b.DebugString(), "[0,10][20,30]");

  // Overlapping intervals (first case).
  std::vector<int64_t> s3 = {0, 10, 50};
  std::vector<int64_t> e3 = {20, 30, 60};
  SortedDisjointIntervalList i3(s3, e3);
  EXPECT_EQ(i3.DebugString(), "[0,30][50,60]");

  // Overlapping intervals (second case).
  std::vector<int64_t> s4 = {50, 10, 0};
  std::vector<int64_t> e4 = {60, 30, 20};
  SortedDisjointIntervalList i4;
  i4.InsertIntervals(s4, e4);
  EXPECT_EQ(i4.DebugString(), "[0,30][50,60]");
  EXPECT_EQ(2, i4.NumIntervals());
  SortedDisjointIntervalList::Iterator it = i4.begin();
  EXPECT_EQ("[0,30]", it->DebugString());
  it++;
  EXPECT_EQ("[50,60]", it->DebugString());

  // Adjacent intervals should be merged.
  std::vector<int64_t> s5 = {0, 15};
  std::vector<int64_t> e5 = {14, 30};
  SortedDisjointIntervalList i5;
  i5.InsertIntervals(s5, e5);
  EXPECT_EQ(i5.DebugString(), "[0,30]");
  EXPECT_EQ(1, i5.NumIntervals());
  EXPECT_EQ("[0,30]", i5.begin()->DebugString());
}

TEST(SortedDisjointIntervalListTest, SimplificationInt) {
  // Insert in the correct order.
  std::vector<int> s1 = {0, 20};
  std::vector<int> e1 = {10, 30};
  SortedDisjointIntervalList i1(s1, e1);
  EXPECT_EQ(i1.DebugString(), "[0,10][20,30]");

  // Reverse intervals.
  std::vector<int> s2 = {20, 0};
  std::vector<int> e2 = {30, 10};
  SortedDisjointIntervalList i2;
  i2.InsertIntervals(s2, e2);
  EXPECT_EQ(i2.DebugString(), "[0,10][20,30]");

  // Overlapping intervals (first case).
  std::vector<int> s3 = {0, 10, 50};
  std::vector<int> e3 = {20, 30, 60};
  SortedDisjointIntervalList i3(s3, e3);
  EXPECT_EQ(i3.DebugString(), "[0,30][50,60]");

  // Overlapping intervals (second case).
  std::vector<int> s4 = {50, 10, 0};
  std::vector<int> e4 = {60, 30, 20};
  SortedDisjointIntervalList i4;
  i4.InsertIntervals(s4, e4);
  EXPECT_EQ(i4.DebugString(), "[0,30][50,60]");
  EXPECT_EQ(2, i4.NumIntervals());
  SortedDisjointIntervalList::Iterator it = i4.begin();
  EXPECT_EQ("[0,30]", it->DebugString());
  it++;
  EXPECT_EQ("[50,60]", it->DebugString());
}

TEST(SortedDisjointIntervalListTest, FirstIntervalGreaterOrEqual) {
  SortedDisjointIntervalList i;
  i.InsertInterval(10, 20);
  i.InsertInterval(30, 40);
  i.InsertInterval(50, 60);
  i.InsertInterval(70, 80);
  EXPECT_EQ(4, i.NumIntervals());

  EXPECT_EQ("[10,20]", i.FirstIntervalGreaterOrEqual(5)->DebugString());
  EXPECT_EQ("[10,20]", i.FirstIntervalGreaterOrEqual(10)->DebugString());
  EXPECT_EQ("[10,20]", i.FirstIntervalGreaterOrEqual(15)->DebugString());
  EXPECT_EQ("[10,20]", i.FirstIntervalGreaterOrEqual(20)->DebugString());
  EXPECT_EQ("[30,40]", i.FirstIntervalGreaterOrEqual(25)->DebugString());
  EXPECT_EQ("[30,40]", i.FirstIntervalGreaterOrEqual(30)->DebugString());
  EXPECT_EQ("[30,40]", i.FirstIntervalGreaterOrEqual(35)->DebugString());
  EXPECT_EQ("[30,40]", i.FirstIntervalGreaterOrEqual(40)->DebugString());
  EXPECT_EQ("[50,60]", i.FirstIntervalGreaterOrEqual(45)->DebugString());
  EXPECT_EQ("[50,60]", i.FirstIntervalGreaterOrEqual(50)->DebugString());
  EXPECT_EQ("[50,60]", i.FirstIntervalGreaterOrEqual(55)->DebugString());
  EXPECT_EQ("[50,60]", i.FirstIntervalGreaterOrEqual(60)->DebugString());
  EXPECT_EQ("[70,80]", i.FirstIntervalGreaterOrEqual(65)->DebugString());
  EXPECT_EQ("[70,80]", i.FirstIntervalGreaterOrEqual(70)->DebugString());
  EXPECT_EQ("[70,80]", i.FirstIntervalGreaterOrEqual(75)->DebugString());
  EXPECT_EQ("[70,80]", i.FirstIntervalGreaterOrEqual(80)->DebugString());
  EXPECT_TRUE(i.FirstIntervalGreaterOrEqual(85) == i.end());
}

TEST(SortedDisjointIntervalListTest, LastIntervalLessOrEqual) {
  SortedDisjointIntervalList i;
  i.InsertInterval(10, 20);
  i.InsertInterval(30, 40);
  i.InsertInterval(50, 60);
  i.InsertInterval(70, 80);
  EXPECT_EQ(4, i.NumIntervals());

  EXPECT_TRUE(i.LastIntervalLessOrEqual(5) == i.end());
  EXPECT_EQ("[10,20]", i.LastIntervalLessOrEqual(10)->DebugString());
  EXPECT_EQ("[10,20]", i.LastIntervalLessOrEqual(15)->DebugString());
  EXPECT_EQ("[10,20]", i.LastIntervalLessOrEqual(20)->DebugString());
  EXPECT_EQ("[10,20]", i.LastIntervalLessOrEqual(25)->DebugString());
  EXPECT_EQ("[30,40]", i.LastIntervalLessOrEqual(30)->DebugString());
  EXPECT_EQ("[30,40]", i.LastIntervalLessOrEqual(35)->DebugString());
  EXPECT_EQ("[30,40]", i.LastIntervalLessOrEqual(40)->DebugString());
  EXPECT_EQ("[30,40]", i.LastIntervalLessOrEqual(45)->DebugString());
  EXPECT_EQ("[50,60]", i.LastIntervalLessOrEqual(50)->DebugString());
  EXPECT_EQ("[50,60]", i.LastIntervalLessOrEqual(55)->DebugString());
  EXPECT_EQ("[50,60]", i.LastIntervalLessOrEqual(60)->DebugString());
  EXPECT_EQ("[50,60]", i.LastIntervalLessOrEqual(65)->DebugString());
  EXPECT_EQ("[70,80]", i.LastIntervalLessOrEqual(70)->DebugString());
  EXPECT_EQ("[70,80]", i.LastIntervalLessOrEqual(75)->DebugString());
  EXPECT_EQ("[70,80]", i.LastIntervalLessOrEqual(80)->DebugString());
  EXPECT_EQ("[70,80]", i.LastIntervalLessOrEqual(85)->DebugString());
}

TEST(SortedDisjointIntervalListTest, Iterator) {
  SortedDisjointIntervalList i;
  i.InsertInterval(10, 20);
  i.InsertInterval(30, 40);
  i.InsertInterval(50, 60);
  i.InsertInterval(70, 80);
  int count = 0;
  for (const auto& interval : i) {
    EXPECT_EQ(10 + 20 * count, interval.start);
    EXPECT_EQ(20 + 20 * count, interval.end);
    count++;
  }
}

TEST(SortedDisjointIntervalListTest, Last) {
  SortedDisjointIntervalList i;
  i.InsertInterval(10, 20);
  EXPECT_EQ(i.last().DebugString(), "[10,20]");
  i.InsertInterval(4, 5);
  EXPECT_EQ(i.last().DebugString(), "[10,20]");
  i.InsertInterval(34, 38);
  EXPECT_EQ(i.last().DebugString(), "[34,38]");
  i.InsertInterval(21, 33);
  EXPECT_EQ(i.last().DebugString(), "[10,38]");
}

TEST(SortedDisjointIntervalListTest, Empty) {
  SortedDisjointIntervalList list;
  EXPECT_EQ(list.NumIntervals(), 0);
  int num_intervals = 0;
  for (const ClosedInterval& unused [[maybe_unused]] : list) ++num_intervals;
  EXPECT_EQ(num_intervals, 0);
  EXPECT_TRUE(list.begin() == list.end());
  for (const int64_t i : {kint64min, int64_t{0}, kint64max}) {
    EXPECT_TRUE(list.FirstIntervalGreaterOrEqual(i) == list.end()) << i;
    EXPECT_TRUE(list.LastIntervalLessOrEqual(i) == list.end()) << i;
  }
  EXPECT_EQ(list.DebugString(), "");
}

void ExpectIntervalListEqual(const SortedDisjointIntervalList& list,
                             absl::Span<const int64_t> starts,
                             absl::Span<const int64_t> ends) {
  EXPECT_EQ(starts.size(), list.NumIntervals());
  int i = 0;
  for (const auto& interval : list) {
    EXPECT_EQ(starts[i], interval.start);
    EXPECT_EQ(ends[i], interval.end);
    ++i;
  }
}

TEST(SortedDisjointIntervalListTest, ComplementInterval) {
  SortedDisjointIntervalList list;
  list.InsertInterval(10, 20);
  list.InsertInterval(30, 40);
  // Interval larger than list.
  ExpectIntervalListEqual(list.BuildComplementOnInterval(0, 50), {0, 21, 41},
                          {9, 29, 50});
  // Interval start overlaps list.
  ExpectIntervalListEqual(list.BuildComplementOnInterval(10, 50), {21, 41},
                          {29, 50});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(15, 50), {21, 41},
                          {29, 50});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(20, 50), {21, 41},
                          {29, 50});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(25, 50), {25, 41},
                          {29, 50});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(29, 50), {29, 41},
                          {29, 50});
  // Interval end overlaps list.
  ExpectIntervalListEqual(list.BuildComplementOnInterval(0, 40), {0, 21},
                          {9, 29});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(0, 35), {0, 21},
                          {9, 29});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(0, 30), {0, 21},
                          {9, 29});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(0, 29), {0, 21},
                          {9, 29});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(0, 21), {0, 21},
                          {9, 21});
  // Interval does not overlap list.
  ExpectIntervalListEqual(list.BuildComplementOnInterval(0, 9), {0}, {9});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(21, 29), {21}, {29});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(41, 50), {41}, {50});
  // Interval overlaps list intervals.
  ExpectIntervalListEqual(list.BuildComplementOnInterval(10, 20), {}, {});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(30, 40), {}, {});
  ExpectIntervalListEqual(list.BuildComplementOnInterval(15, 35), {21}, {29});
}

TEST(SortedDisjointIntervalListTest, ComplementIntervalOnEmptyList) {
  SortedDisjointIntervalList list;
  ExpectIntervalListEqual(list.BuildComplementOnInterval(0, 50), {0}, {50});
}

TEST(SortedDisjointIntervalListTest, RandomizedStressTest) {
  std::mt19937 random(12345);
  SortedDisjointIntervalList list;
  const int64_t kSpecialValues[] = {kint64min,     kint64min + 1, kint64min + 2,
                                    kint64min + 3, kint64max,     kint64max - 1,
                                    kint64max - 2, kint64max - 3};
  const int64_t kNormalValueSpan = 10000;
  for (const int num_inserts : {1, 2, 3, 4, 10, 100, 100000}) {
    // NOTE(user): Timing on 2016-05-04: 2.4s in fastbuild, 4.9s in opt.
    const int num_iters =
        std::max((DEBUG_MODE ? 100000 : 1000000) / num_inserts, 1);
    for (int iter = 0; iter < num_iters; ++iter) {
      // Insert 'num_inserts' random small intervals.
      list.clear();
      for (int i = 0; i < num_inserts; ++i) {
        const int64_t start = absl::Bernoulli(random, 1.0 / 4)
                                  ? kSpecialValues[absl::Uniform<int>(
                                        random, 0, std::size(kSpecialValues))]
                                  : absl::Uniform(random, 0, kNormalValueSpan);
        const int64_t end = CapAdd(start, absl::Uniform(random, 0, 5));
        auto it = list.InsertInterval(start, end);
        ASSERT_TRUE(it->start <= start && it->end >= end)
            << "Inserted [" << start << "," << end << "], got " << *it
            << ", list: " << list.DebugString();
      }

      // Verify that the interval list makes sense.
      int num_intervals = 0;
      const ClosedInterval* prev = nullptr;
      for (const ClosedInterval& interval : list) {
        ++num_intervals;
        ASSERT_LE(interval.start, interval.end);
        if (prev != nullptr) {
          ASSERT_GT(interval.start, prev->end + 1);
        }
        prev = &interval;
      }
      ASSERT_EQ(num_intervals, list.NumIntervals());
      ASSERT_LE(num_intervals, num_inserts);
      ASSERT_GE(num_intervals, 1);

      // Stress-test O(num_insert) lookup operations (to spend about the same
      // time as doing the insertions).
      for (int i = 0; i < 2 * num_inserts + 10; ++i) {
        const int64_t v = absl::Bernoulli(random, 1.0 / 2)
                              ? kSpecialValues[absl::Uniform<int>(
                                    random, 0, std::size(kSpecialValues))]
                              : absl::Uniform(random, 0, kNormalValueSpan) - 1;
        auto it = list.FirstIntervalGreaterOrEqual(v);
        if (it == list.end()) {
          it--;
          ASSERT_LT(it->end, v) << v << ", list: " << list.DebugString();
        } else if (it->start <= v) {
          // The returned interval contains "v".
          ASSERT_GE(it->end, v)
              << v << ", interval: " << *it << ", list: " << list.DebugString();
        } else {
          // The returned interval is strictly above "v".  Check that the
          // previous interval is strictly below v.
          if (it != list.begin()) {
            it--;
            ASSERT_LT(it->end, v) << v << ", interval: " << *it
                                  << ", list: " << list.DebugString();
          }
        }
      }
    }
  }
}

}  // namespace
}  // namespace operations_research
