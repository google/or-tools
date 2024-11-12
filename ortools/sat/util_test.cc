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

#include "ortools/sat/util.h"

#include <math.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <tuple>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/random/random.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/sorted_interval_list.h"

using ::testing::UnorderedElementsAre;

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;

TEST(CompactVectorVectorTest, EmptyCornerCases) {
  CompactVectorVector<int, int> storage;
  EXPECT_EQ(storage.size(), 0);

  const int index = storage.Add(std::vector<int>());
  EXPECT_EQ(storage.size(), 1);
  EXPECT_EQ(storage[index].size(), 0);
}

TEST(CompactVectorVectorTest, ResetFromFlatMapping) {
  CompactVectorVector<int, int> storage;
  EXPECT_EQ(storage.size(), 0);

  const std::vector<int> input = {2, 2, 1, 1, 1, 0, 0, 2, 2};
  storage.ResetFromFlatMapping(input, IdentityMap<int>());

  EXPECT_EQ(storage.size(), 3);
  EXPECT_THAT(storage[0], ElementsAre(5, 6));
  EXPECT_THAT(storage[1], ElementsAre(2, 3, 4));
  EXPECT_THAT(storage[2], ElementsAre(0, 1, 7, 8));
}

TEST(CompactVectorVectorTest, RemoveBySwap) {
  CompactVectorVector<int, int> storage;
  EXPECT_EQ(storage.size(), 0);

  const std::vector<int> input = {2, 2, 1, 1, 1, 0, 0, 2, 2};
  storage.ResetFromFlatMapping(input, IdentityMap<int>());

  EXPECT_EQ(storage.size(), 3);
  EXPECT_THAT(storage[0], ElementsAre(5, 6));
  EXPECT_THAT(storage[1], ElementsAre(2, 3, 4));
  EXPECT_THAT(storage[2], ElementsAre(0, 1, 7, 8));

  storage.RemoveBySwap(1, 1);
  EXPECT_THAT(storage[1], ElementsAre(2, 4));

  storage.RemoveBySwap(2, 1);
  EXPECT_THAT(storage[2], ElementsAre(0, 8, 7));
}

TEST(CompactVectorVectorTest, ShrinkValues) {
  CompactVectorVector<int, int> storage;
  EXPECT_EQ(storage.size(), 0);

  const std::vector<int> input = {2, 2, 1, 1, 1, 0, 0, 2, 2};
  storage.ResetFromFlatMapping(input, IdentityMap<int>());

  EXPECT_EQ(storage.size(), 3);
  EXPECT_THAT(storage[0], ElementsAre(5, 6));
  EXPECT_THAT(storage[1], ElementsAre(2, 3, 4));
  EXPECT_THAT(storage[2], ElementsAre(0, 1, 7, 8));

  storage.ReplaceValuesBySmallerSet(2, {3, 4, 5});
  EXPECT_THAT(storage[2], ElementsAre(3, 4, 5));
}

TEST(CompactVectorVectorTest, ResetFromTranspose) {
  CompactVectorVector<int, int> storage;
  EXPECT_EQ(storage.size(), 0);

  const std::vector<int> keys = {2, 2, 1, 1, 1, 0, 0, 2, 2};
  const std::vector<int> values = {3, 4, 0, 0, 1, 5, 1, 2, 2};
  storage.ResetFromFlatMapping(keys, values);

  ASSERT_EQ(storage.size(), 3);
  EXPECT_THAT(storage[0], ElementsAre(5, 1));
  EXPECT_THAT(storage[1], ElementsAre(0, 0, 1));
  EXPECT_THAT(storage[2], ElementsAre(3, 4, 2, 2));

  CompactVectorVector<int, int> transpose;
  transpose.ResetFromTranspose(storage);

  ASSERT_EQ(transpose.size(), 6);
  EXPECT_THAT(transpose[0], ElementsAre(1, 1));
  EXPECT_THAT(transpose[1], ElementsAre(0, 1));
  EXPECT_THAT(transpose[2], ElementsAre(2, 2));
  EXPECT_THAT(transpose[3], ElementsAre(2));
  EXPECT_THAT(transpose[4], ElementsAre(2));
  EXPECT_THAT(transpose[5], ElementsAre(0));

  // Note that retransposing sorts !
  CompactVectorVector<int, int> second_transpose;
  second_transpose.ResetFromTranspose(transpose);

  ASSERT_EQ(second_transpose.size(), 3);
  EXPECT_THAT(second_transpose[0], ElementsAre(1, 5));
  EXPECT_THAT(second_transpose[1], ElementsAre(0, 0, 1));
  EXPECT_THAT(second_transpose[2], ElementsAre(2, 2, 3, 4));
}

TEST(FormatCounterTest, BasicCases) {
  EXPECT_EQ("12", FormatCounter(12));
  EXPECT_EQ("12'345", FormatCounter(12345));
  EXPECT_EQ("123'456'789", FormatCounter(123456789));
}

TEST(FormatTable, BasicAlign) {
  std::vector<std::vector<std::string>> table{
      {"x", "x", "x", "x", "x"},
      {FormatName("xx"), "xx", "xx", "xx", "xx"},
      {FormatName("xxx"), "xxx", "xxx", "xxx", "xxx"}};

  EXPECT_EQ(
      "x           x    x    x    x\n"
      "   'xx':   xx   xx   xx   xx\n"
      "  'xxx':  xxx  xxx  xxx  xxx\n",
      FormatTable(table));
}

TEST(ModularInverseTest, AllSmallValues) {
  for (int64_t m = 1; m < 1000; ++m) {
    for (int64_t x = 1; x < m; ++x) {
      const int64_t inverse = ModularInverse(x, m);
      ASSERT_GE(inverse, 0);
      ASSERT_LT(inverse, m);
      if (inverse == 0) {
        ASSERT_NE(std::gcd(x, m), 1);
      } else {
        ASSERT_EQ(x * inverse % m, 1);
      }
    }
  }
}

TEST(ModularInverseTest, BasicOverflowTest) {
  absl::BitGen random;
  const int64_t max = std::numeric_limits<int64_t>::max();
  for (int i = 0; i < 100000; ++i) {
    const int64_t m = max - absl::LogUniform<int64_t>(random, 0, max);
    const int64_t x = absl::Uniform(random, 0, m);
    const int64_t inverse = ModularInverse(x, m);
    ASSERT_GE(inverse, 0);
    ASSERT_LT(inverse, m);
    if (inverse == 0) {
      ASSERT_NE(std::gcd(x, m), 1);
    } else {
      absl::int128 test_x = x;
      absl::int128 test_inverse = inverse;
      absl::int128 test_m = m;
      ASSERT_EQ(test_x * test_inverse % test_m, 1);
    }
  }
}

TEST(ProductWithodularInverseTest, FewSmallValues) {
  const int limit = 50;
  for (int64_t mod = 1; mod < limit; ++mod) {
    for (int64_t coeff = -limit; coeff < limit; ++coeff) {
      if (coeff == 0 || std::gcd(mod, std::abs(coeff)) != 1) continue;
      for (int64_t rhs = -mod; rhs < mod; ++rhs) {
        const int64_t result = ProductWithModularInverse(coeff, mod, rhs);
        for (int64_t test = -limit; test < limit; ++test) {
          const int64_t x = test * mod + result;
          ASSERT_EQ(PositiveMod(x * coeff, mod), PositiveMod(rhs, mod));
        }
      }
    }
  }
}

TEST(SolveDiophantineEquationOfSizeTwoTest, FewSmallValues) {
  const int limit = 50;
  for (int64_t a = -limit; a < limit; ++a) {
    if (a == 0) continue;
    for (int64_t b = -limit; b < limit; ++b) {
      if (b == 0) continue;
      for (int64_t c = -limit; c < limit; ++c) {
        int64_t ca = a;
        int64_t cb = b;
        int64_t cc = c;
        int64_t x0, y0;
        const bool r = SolveDiophantineEquationOfSizeTwo(ca, cb, cc, x0, y0);
        if (!r) {
          // This is the only case.
          const int gcd = std::gcd(std::abs(a), std::abs(b));
          CHECK_GT(gcd, 1);
          ASSERT_NE(c % gcd, 0);
          continue;
        }
        ASSERT_EQ(ca * x0 + cb * y0, cc);
      }
    }
  }
}

TEST(SolveDiophantineEquationOfSizeTwoTest, BasicOverflowTest) {
  absl::BitGen random;
  const int64_t max = std::numeric_limits<int64_t>::max();
  for (int i = 0; i < 100000; ++i) {
    int64_t a = max - absl::LogUniform<int64_t>(random, 0, max);
    int64_t b = max - absl::LogUniform<int64_t>(random, 0, max);
    int64_t cte = absl::Uniform(random, 0, max);
    if (absl::Bernoulli(random, 0.5)) a = -a;
    if (absl::Bernoulli(random, 0.5)) b = -b;
    if (absl::Bernoulli(random, 0.5)) cte = -cte;

    int64_t x0, y0;
    if (!SolveDiophantineEquationOfSizeTwo(a, b, cte, x0, y0)) {
      // This is the only case.
      const int64_t gcd = std::gcd(std::abs(a), std::abs(b));
      CHECK_GT(gcd, 1);
      ASSERT_NE(cte % gcd, 0);
      continue;
    }
    ASSERT_EQ(
        absl::int128{a} * absl::int128{x0} + absl::int128{b} * absl::int128{y0},
        absl::int128{cte});
  }
}

TEST(ClosestMultipleTest, BasicCases) {
  EXPECT_EQ(ClosestMultiple(9, 10), 10);
  EXPECT_EQ(ClosestMultiple(-9, 10), -10);
  EXPECT_EQ(ClosestMultiple(5, 10), 0);
  EXPECT_EQ(ClosestMultiple(-5, 10), 0);
  EXPECT_EQ(ClosestMultiple(6, 10), 10);
  EXPECT_EQ(ClosestMultiple(-6, 10), -10);
  EXPECT_EQ(ClosestMultiple(789, 10), 790);
}

TEST(LinearInequalityCanBeReducedWithClosestMultipleTest, BasicCase) {
  std::vector<int64_t> coeffs = {99, 101};
  std::vector<int64_t> lbs = {-10, -10};
  std::vector<int64_t> ubs = {10, 10};

  // Trivially true case.
  int64_t new_rhs;
  EXPECT_TRUE(LinearInequalityCanBeReducedWithClosestMultiple(
      100, coeffs, lbs, ubs, 10000000, &new_rhs));
  EXPECT_EQ(new_rhs, 20);

  // X + Y <= 3 case
  EXPECT_TRUE(LinearInequalityCanBeReducedWithClosestMultiple(
      100, coeffs, lbs, ubs, 350, &new_rhs));
  EXPECT_EQ(new_rhs, 3);

  // X + Y <= 3, limit case.
  //
  // It doesn't work with 316 since 10 * 101 - 7 * 99 = 317.
  EXPECT_TRUE(LinearInequalityCanBeReducedWithClosestMultiple(
      100, coeffs, lbs, ubs, 317, &new_rhs));
  EXPECT_EQ(new_rhs, 3);

  // False case: we cannot reduce the equation to a multiple of 100.
  EXPECT_FALSE(LinearInequalityCanBeReducedWithClosestMultiple(
      100, coeffs, lbs, ubs, 316, &new_rhs));
}

TEST(LinearInequalityCanBeReducedWithClosestMultipleTest, Random) {
  absl::BitGen random;
  int num_reductions = 0;
  const int num_tests = 100;
  const int num_terms = 5;
  const int64_t base = 10000;
  for (int test = 0; test < num_tests; ++test) {
    // We generate a random expression around 10 k + [-10, 10]
    std::vector<int64_t> coeffs;
    std::vector<int64_t> lbs(num_terms, -1);
    std::vector<int64_t> ubs(num_terms, +1);
    int64_t max_activity = 0;
    for (int i = 0; i < num_terms; ++i) {
      coeffs.push_back(base + absl::Uniform(random, -10, 10));
      max_activity +=
          std::max(coeffs.back() * ubs.back(), coeffs.back() * lbs.back());
    }
    const int64_t target = max_activity - 2 * base;
    const int64_t rhs = absl::Uniform(random, target - 50, target + 50);

    int64_t new_rhs;
    const bool ok = LinearInequalityCanBeReducedWithClosestMultiple(
        base, coeffs, lbs, ubs, rhs, &new_rhs);
    if (!ok) continue;

    VLOG(2) << absl::StrJoin(coeffs, ", ") << " <= " << rhs << " new "
            << new_rhs;

    // Test that the set of solutions is the same.
    for (int number = 0; number < pow(3, num_terms); ++number) {
      int temp = number;
      int64_t activity = 0;
      int64_t new_activity = 0;
      for (int i = 0; i < num_terms; ++i) {
        const int x = (temp % 3) - 1;
        temp /= 3;
        activity += coeffs[i] * x;
        new_activity += x;
      }
      if (activity <= rhs) {
        ASSERT_LE(new_activity, new_rhs);
      } else {
        ASSERT_GT(new_activity, new_rhs);
      }
    }

    ++num_reductions;
  }

  // Over 10k runs, this worked. So we simplify sometimes but not always.
  VLOG(2) << num_reductions;
  EXPECT_GE(num_reductions, 10);
  EXPECT_LT(num_reductions, num_tests);
}

TEST(MoveOneUnprocessedLiteralLastTest, CorrectBehavior) {
  absl::btree_set<LiteralIndex> moved_last;
  std::vector<Literal> literals;
  for (int i = 0; i < 100; ++i) {
    literals.push_back(Literal(BooleanVariable(i), true));
  }

  int i = 0;
  while (MoveOneUnprocessedLiteralLast(moved_last, literals.size(),
                                       &literals) != -1) {
    ++i;
    EXPECT_FALSE(moved_last.contains(literals.back().Index()));
    moved_last.insert(literals.back().Index());
  }
  EXPECT_EQ(i, literals.size());

  // No change in the actual literals.
  std::sort(literals.begin(), literals.end());
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(literals[i], Literal(BooleanVariable(i), true));
  }
}

int SumOfPrefixesForSize(int n) {
  absl::btree_set<LiteralIndex> moved_last;
  std::vector<Literal> literals;
  for (int i = 0; i < n; ++i) {
    literals.push_back(Literal(BooleanVariable(i), true));
  }
  int s = 0;
  int result = 0;
  std::vector<Literal> before, after;
  while (true) {
    before = literals;
    s = MoveOneUnprocessedLiteralLast(moved_last, literals.size(), &literals);
    if (s == -1) return result;

    moved_last.insert(literals.back().Index());
    result += n - s;

    // Check that s is an actual prefix size.
    after = literals;
    before.resize(s);
    after.resize(s);
    EXPECT_EQ(before, after);
  }
  return result;
}

TEST(MoveOneUnprocessedLiteralLastTest, CorrectComplexity) {
  EXPECT_EQ(SumOfPrefixesForSize(0), 0);
  EXPECT_EQ(SumOfPrefixesForSize(1), 0);
  EXPECT_EQ(SumOfPrefixesForSize(2), 2);
  EXPECT_EQ(SumOfPrefixesForSize(3), 5);
  EXPECT_EQ(SumOfPrefixesForSize(4), 4 * log2(4));

  // Note that this one can be done in 12 with S(5) = S(2) + 5 + S(3), so our
  // algorithm is suboptimal for non power of 2 sizes starting from here.
  EXPECT_EQ(SumOfPrefixesForSize(5), 13);
  EXPECT_EQ(SumOfPrefixesForSize(6), 16);
  EXPECT_EQ(SumOfPrefixesForSize(7), 20);
  EXPECT_EQ(SumOfPrefixesForSize(8), 8 * log2(8));
  EXPECT_EQ(SumOfPrefixesForSize(9), 33);
  EXPECT_EQ(SumOfPrefixesForSize(10), 36);
  EXPECT_EQ(SumOfPrefixesForSize(100), 688);
  EXPECT_EQ(SumOfPrefixesForSize(1000), 9984);
  EXPECT_EQ(SumOfPrefixesForSize(1024), 1024 * log2(1024));
}

constexpr double kTolerance = 1e-6;

TEST(IncrementalAverage, PositiveData) {
  IncrementalAverage positive_data;
  for (int i = 1; i < 101; ++i) {
    positive_data.AddData(i);
    EXPECT_EQ(i, positive_data.NumRecords());
    EXPECT_NEAR((i + 1) / 2.0, positive_data.CurrentAverage(), kTolerance);
  }
}

TEST(IncrementalAverage, NegativeData) {
  IncrementalAverage negative_data;
  for (int i = 1; i < 101; ++i) {
    negative_data.AddData(-i);
    EXPECT_EQ(i, negative_data.NumRecords());
    EXPECT_NEAR(-(i + 1) / 2.0, negative_data.CurrentAverage(), kTolerance);
  }
}

TEST(IncrementalAverage, MixedData) {
  IncrementalAverage data;
  EXPECT_EQ(0, data.NumRecords());
  EXPECT_EQ(0.0, data.CurrentAverage());

  data.AddData(-1);
  EXPECT_EQ(1, data.NumRecords());
  EXPECT_EQ(-1.0, data.CurrentAverage());

  data.AddData(0);
  EXPECT_EQ(2, data.NumRecords());
  EXPECT_NEAR(-1.0 / 2.0, data.CurrentAverage(), kTolerance);

  data.AddData(1);
  EXPECT_EQ(3, data.NumRecords());
  EXPECT_NEAR(0.0, data.CurrentAverage(), kTolerance);
}

TEST(IncrementalAverage, InitialFeed) {
  IncrementalAverage data(5.0);
  EXPECT_EQ(0, data.NumRecords());
  EXPECT_EQ(5.0, data.CurrentAverage());
}

TEST(IncrementalAverage, Reset) {
  IncrementalAverage data;
  data.AddData(5.0);
  EXPECT_EQ(1, data.NumRecords());
  EXPECT_EQ(5.0, data.CurrentAverage());

  data.Reset(3.0);
  EXPECT_EQ(0, data.NumRecords());
  EXPECT_EQ(3.0, data.CurrentAverage());
}

TEST(ExponentialMovingAverage, Average) {
  ExponentialMovingAverage data(/*decaying_factor=*/0.1);
  EXPECT_EQ(0, data.NumRecords());
  EXPECT_EQ(0.0, data.CurrentAverage());

  data.AddData(10.0);
  EXPECT_EQ(1, data.NumRecords());
  EXPECT_EQ(10.0, data.CurrentAverage());

  data.AddData(20.0);
  EXPECT_EQ(2, data.NumRecords());
  EXPECT_NEAR(19.0, data.CurrentAverage(), kTolerance);

  data.AddData(30);
  EXPECT_EQ(3, data.NumRecords());
  EXPECT_NEAR(28.9, data.CurrentAverage(), kTolerance);
}

TEST(Percentile, BasicTest) {
  // Example at https://en.wikipedia.org/wiki/Percentile
  Percentile data(/*record_limit=*/5);
  EXPECT_EQ(0, data.NumRecords());

  data.AddRecord(15.0);
  data.AddRecord(20.0);
  data.AddRecord(35.0);
  data.AddRecord(40.0);
  data.AddRecord(50.0);
  EXPECT_EQ(5, data.NumRecords());

  EXPECT_NEAR(15.0, data.GetPercentile(5), kTolerance);
  EXPECT_NEAR(20.0, data.GetPercentile(30), kTolerance);
  EXPECT_NEAR(27.5, data.GetPercentile(40), kTolerance);
  EXPECT_NEAR(50, data.GetPercentile(95), kTolerance);
}

TEST(Percentile, RecordLimit) {
  Percentile data(/*record_limit=*/2);
  EXPECT_EQ(0, data.NumRecords());

  data.AddRecord(15.0);
  EXPECT_EQ(1, data.NumRecords());
  data.AddRecord(20.0);
  EXPECT_EQ(2, data.NumRecords());
  EXPECT_NEAR(15.0, data.GetPercentile(10), kTolerance);
  EXPECT_NEAR(20.0, data.GetPercentile(90), kTolerance);
  data.AddRecord(35.0);
  data.AddRecord(40.0);
  EXPECT_EQ(2, data.NumRecords());
  EXPECT_NEAR(35.0, data.GetPercentile(10), kTolerance);
  EXPECT_NEAR(40.0, data.GetPercentile(90), kTolerance);
}

TEST(Percentile, BasicTest2) {
  Percentile data(/*record_limit=*/10);
  EXPECT_EQ(0, data.NumRecords());

  data.AddRecord(6.0753);
  data.AddRecord(8.6678);
  data.AddRecord(0.4823);
  data.AddRecord(6.7243);
  data.AddRecord(5.6375);
  data.AddRecord(2.3846);
  data.AddRecord(4.1328);
  data.AddRecord(5.6852);
  data.AddRecord(12.1568);
  data.AddRecord(10.5389);
  EXPECT_EQ(10, data.NumRecords());

  EXPECT_NEAR(5.6709, data.GetPercentile(42), 1e-5);
}

TEST(Percentile, RandomNumbers) {
  const int record_limit = 1000;
  Percentile data(record_limit);
  EXPECT_EQ(0, data.NumRecords());

  std::vector<double> records;
  absl::BitGen random;
  for (int i = 0; i < record_limit; ++i) {
    double record = absl::Uniform<double>(random, -10000, 10000);
    records.push_back(record);
    data.AddRecord(record);
  }
  EXPECT_EQ(record_limit, data.NumRecords());
  std::sort(records.begin(), records.end());
  for (int i = 0; i < record_limit; ++i) {
    EXPECT_NEAR(records[i],
                data.GetPercentile((i + 0.5) * 100.0 / record_limit),
                kTolerance);
  }
}

TEST(SafeDoubleToInt64Test, BasicCases) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  const int64_t kMax = std::numeric_limits<int64_t>::max();
  const int64_t kMin = std::numeric_limits<int64_t>::min();
  const int64_t max53 = (int64_t{1} << 53) - 1;

  // Arbitrary behavior for nans.
  EXPECT_EQ(SafeDoubleToInt64(std::numeric_limits<double>::quiet_NaN()), 0);
  EXPECT_EQ(SafeDoubleToInt64(std::numeric_limits<double>::signaling_NaN()), 0);

  EXPECT_EQ(SafeDoubleToInt64(static_cast<double>(kMax)), kMax);
  EXPECT_EQ(SafeDoubleToInt64(kInfinity), kMax);

  // Transition for max.
  for (int i = 0; i < 512; ++i) {
    ASSERT_EQ(SafeDoubleToInt64(static_cast<double>(kMax - i)), kMax);
  }
  for (int i = 512; i < 1024 + 511; ++i) {
    ASSERT_EQ(SafeDoubleToInt64(static_cast<double>(kMax - i)), kMax - 1023);
  }
  ASSERT_EQ(SafeDoubleToInt64(static_cast<double>(kMax - 1024 - 511)),
            kMax - 2047);

  // Transition for max precision.
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(SafeDoubleToInt64(static_cast<double>(max53 - i)), max53 - i);
  }
  int num_error = 0;
  for (int i = 0; i < 10; ++i) {
    if (SafeDoubleToInt64(static_cast<double>(max53 + i)) != max53 + i) {
      ++num_error;
    }
  }
  EXPECT_EQ(num_error, 4);

  // static_cast just truncate the number...
  EXPECT_EQ(SafeDoubleToInt64(0.1), 0);
  EXPECT_EQ(SafeDoubleToInt64(0.9), 0);

  EXPECT_EQ(SafeDoubleToInt64(static_cast<double>(kMin)), kMin);
  EXPECT_EQ(SafeDoubleToInt64(-kInfinity), kMin);
}

TEST(MaxBoundedSubsetSumTest, LowMaxValue) {
  const int bound = 49;
  MaxBoundedSubsetSum bounded_subset_sum(bound);
  for (int gcd = 2; gcd < 10; ++gcd) {
    bounded_subset_sum.Reset(bound);
    for (int i = 0; i < 1000; ++i) {
      bounded_subset_sum.Add((i % 50) * gcd);
    }
    EXPECT_EQ(bounded_subset_sum.CurrentMax(), bound / gcd * gcd);
  }
}

TEST(MaxBoundedSubsetSumTest, LowNumberOfElement) {
  MaxBoundedSubsetSum bounded_subset_sum(178'979);
  bounded_subset_sum.Add(150'000);
  bounded_subset_sum.Add(28'000);
  bounded_subset_sum.Add(1000);
  EXPECT_EQ(bounded_subset_sum.CurrentMax(), 178'000);

  // Too many elements causes an "abort" and we just return the bound.
  for (int i = 0; i < 10; ++i) {
    bounded_subset_sum.Add(i);
  }
  EXPECT_EQ(bounded_subset_sum.CurrentMax(), 178'979);
}

TEST(MaxBoundedSubsetSumTest, FailBackToGcd) {
  MaxBoundedSubsetSum bounded_subset_sum(/*bound=*/10122);
  bounded_subset_sum.AddMultiples(100, 10);
  EXPECT_EQ(bounded_subset_sum.CurrentMax(), 1000);

  bounded_subset_sum.AddMultiples(200, 1000);
  EXPECT_EQ(bounded_subset_sum.CurrentMax(), 10100);

  // We could have better bounding maybe in this case.
  bounded_subset_sum.Add(1);
  EXPECT_EQ(bounded_subset_sum.CurrentMax(), 10122);
}

TEST(MaxBoundedSubsetSumTest, SimpleMultiChoice) {
  MaxBoundedSubsetSum bounded_subset_sum(34);
  bounded_subset_sum.AddChoices({3, 10, 19});
  bounded_subset_sum.AddChoices({0, 2, 4, 40});
  bounded_subset_sum.AddChoices({3, 7, 8, 16});
  EXPECT_EQ(bounded_subset_sum.CurrentMax(), 31);
}

TEST(MaxBoundedSubsetSumTest, CheckMaxIfAdded) {
  MaxBoundedSubsetSum bounded_subset_sum(34);
  bounded_subset_sum.Add(10);
  bounded_subset_sum.Add(10);
  bounded_subset_sum.Add(10);
  EXPECT_EQ(bounded_subset_sum.MaxIfAdded(12), 32);
  EXPECT_EQ(bounded_subset_sum.MaxIfAdded(15), 30);
  EXPECT_EQ(bounded_subset_sum.MaxIfAdded(34), 34);
  for (int i = 0; i < 100; ++i) {
    bounded_subset_sum.Add(18);
  }
  EXPECT_EQ(bounded_subset_sum.CurrentMax(), 30);
  EXPECT_EQ(bounded_subset_sum.MaxIfAdded(5), 33);
}

static void BM_bounded_subset_sum(benchmark::State& state) {
  random_engine_t random_;
  const int num_items = state.range(0);
  const int num_choices = state.range(1);
  const int max_capacity = state.range(2);
  const int max_size = state.range(3);

  const int num_updates = num_items * num_choices;
  const int capacity = std::uniform_int_distribution<int>(
      max_capacity / 2, max_capacity)(random_);
  MaxBoundedSubsetSum subset_sum(capacity);
  std::uniform_int_distribution<int> size_dist(0, max_size);
  std::vector<int64_t> choices(num_choices);
  for (auto _ : state) {
    subset_sum.Reset(capacity);
    for (int i = 0; i < num_items; ++i) {
      choices.clear();
      for (int j = 0; j < num_choices; ++j) {
        choices[j] = size_dist(random_);
      }
      subset_sum.AddChoices(choices);
    }
  }
  // Number of updates.
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          num_updates);
}

BENCHMARK(BM_bounded_subset_sum)
    ->Args({10, 3, 30, 5})
    ->Args({10, 4, 50, 10})
    ->Args({10, 4, 30, 20})
    ->Args({25, 3, 30, 5})
    ->Args({25, 4, 50, 10})
    ->Args({25, 4, 30, 20})
    ->Args({60, 3, 30, 5})
    ->Args({60, 4, 50, 10})
    ->Args({60, 4, 30, 20})
    ->Args({100, 3, 30, 5})
    ->Args({100, 4, 50, 10})
    ->Args({100, 4, 30, 20});

TEST(FirstFewValuesTest, Basic) {
  FirstFewValues<8> values;
  EXPECT_EQ(values.LastValue(), std::numeric_limits<int64_t>::max());
  values.Add(3);
  EXPECT_THAT(values.reachable(), ElementsAre(0, 3, 6, 9, 12, 15, 18, 21));
  values.Add(5);
  EXPECT_THAT(values.reachable(), ElementsAre(0, 3, 5, 6, 8, 9, 10, 11));

  EXPECT_TRUE(values.MightBeReachable(0));
  EXPECT_TRUE(values.MightBeReachable(100));
  EXPECT_FALSE(values.MightBeReachable(2));
  EXPECT_FALSE(values.MightBeReachable(7));
}

TEST(FirstFewValuesTest, Overflow) {
  FirstFewValues<6> values;

  const int64_t max = std::numeric_limits<int64_t>::max();
  const int64_t v = max / 3;
  values.Add(v);
  EXPECT_THAT(values.reachable(), ElementsAre(0, v, 2 * v, 3 * v, max, max));
}

TEST(BasicKnapsackSolverTest, BasicFeasibleExample) {
  std::vector<Domain> domains = {Domain(-3, 14), Domain(1, 15)};
  std::vector<int64_t> coeffs = {7, 13};
  std::vector<int64_t> costs = {5, 8};
  Domain rhs(100, 200);

  BasicKnapsackSolver solver;
  const auto& result = solver.Solve(domains, coeffs, costs, rhs);
  EXPECT_TRUE(result.solved);
  EXPECT_FALSE(result.infeasible);
  EXPECT_THAT(result.solution, ElementsAre(-2, 9));
}

TEST(BasicKnapsackSolverTest, BasicInfesibleExample) {
  std::vector<Domain> domains = {Domain(-3, 8), Domain(1, 8)};
  std::vector<int64_t> coeffs = {7, 13};
  std::vector<int64_t> costs = {5, 8};
  Domain rhs(103);

  BasicKnapsackSolver solver;
  const auto& result = solver.Solve(domains, coeffs, costs, rhs);
  EXPECT_TRUE(result.solved);
  EXPECT_TRUE(result.infeasible);
}

TEST(BasicKnapsackSolverTest, RandomComparisonWithSolver) {
  const int num_vars = 6;
  absl::BitGen random;

  BasicKnapsackSolver solver;
  for (int num_tests = 0; num_tests < 100; ++num_tests) {
    std::vector<Domain> domains;
    std::vector<int64_t> coeffs;
    std::vector<int64_t> costs;
    for (int i = 0; i < num_vars; ++i) {
      int a = absl::Uniform<int>(random, -10, 10);
      int b = absl::Uniform<int>(random, -10, 10);
      if (a > b) std::swap(a, b);

      domains.push_back(Domain(a, b));
      costs.push_back(absl::Uniform<int>(random, -10, 10));
      coeffs.push_back(absl::Uniform<int>(random, -10, 9));
      if (coeffs.back() >= 0) coeffs.back()++;
    }
    const int c = absl::Uniform<int>(random, num_vars * 5, num_vars * 10);
    Domain rhs = Domain(c, c + absl::Uniform(random, 0, 5));

    // Create corresponding proto.
    CpModelProto proto;
    auto* linear = proto.add_constraints()->mutable_linear();
    auto* objective = proto.mutable_objective();
    for (int i = 0; i < num_vars; ++i) {
      auto* var = proto.add_variables();
      FillDomainInProto(domains[i], var);
      linear->add_vars(i);
      linear->add_coeffs(coeffs[i]);
      objective->add_vars(i);
      objective->add_coeffs(costs[i]);
    }
    FillDomainInProto(rhs, linear);

    const auto& result = solver.Solve(domains, coeffs, costs, rhs);
    CHECK(result.solved);  // We should always be able to solve here.

    SatParameters params;
    params.set_cp_model_presolve(false);
    const CpSolverResponse response = SolveWithParameters(proto, params);

    if (result.infeasible) {
      EXPECT_EQ(response.status(), INFEASIBLE);
    } else {
      EXPECT_EQ(response.status(), OPTIMAL);
      int64_t objective = 0;
      for (int i = 0; i < num_vars; ++i) {
        EXPECT_TRUE(domains[i].Contains(result.solution[i]))
            << domains[i] << " " << result.solution[i] << " " << coeffs[i];
        objective += costs[i] * result.solution[i];
      }
      EXPECT_DOUBLE_EQ(objective, response.objective_value());
    }
  }
}

using CeilFloorTest = testing::TestWithParam<std::tuple<int, int>>;

TEST_P(CeilFloorTest, FloorOfRatioInt) {
  const int a = std::get<0>(GetParam());
  const int b = std::get<1>(GetParam());
  if (b == 0) return;
  int q = MathUtil::FloorOfRatio<int>(a, b);
  // a / b - 1 < q <= a / b
  EXPECT_LT(static_cast<double>(a) / static_cast<double>(b) - 1.0,
            static_cast<double>(q));
  EXPECT_LE(static_cast<double>(q),
            static_cast<double>(a) / static_cast<double>(b));
}

TEST_P(CeilFloorTest, FloorOfRatioInt128) {
  const absl::int128 a = std::get<0>(GetParam());
  const absl::int128 b = std::get<1>(GetParam());
  if (b == 0) return;
  absl::int128 q = MathUtil::FloorOfRatio(a, b);
  // a / b - 1 < q <= a / b
  EXPECT_LT(static_cast<double>(a) / static_cast<double>(b) - 1.0,
            static_cast<double>(q));
  EXPECT_LE(static_cast<double>(q),
            static_cast<double>(a) / static_cast<double>(b));
}

TEST_P(CeilFloorTest, CeilOfRatioInt) {
  const int a = std::get<0>(GetParam());
  const int b = std::get<1>(GetParam());
  if (b == 0) return;
  int q = MathUtil::CeilOfRatio(a, b);
  // a / b <= q < a / b + 1
  EXPECT_LE(static_cast<double>(a) / static_cast<double>(b),
            static_cast<double>(q));
  EXPECT_LE(static_cast<double>(q),
            static_cast<double>(a) / static_cast<double>(b) + 1.0);
}

TEST_P(CeilFloorTest, CeilOfRatioInt128) {
  const absl::int128 a = std::get<0>(GetParam());
  const absl::int128 b = std::get<1>(GetParam());
  if (b == 0) return;
  absl::int128 q = MathUtil::CeilOfRatio(a, b);
  // a / b <= q < a / b + 1
  EXPECT_LE(static_cast<double>(a) / static_cast<double>(b),
            static_cast<double>(q));
  EXPECT_LE(static_cast<double>(q),
            static_cast<double>(a) / static_cast<double>(b) + 1.0);
}

INSTANTIATE_TEST_SUITE_P(CeilFloorTests, CeilFloorTest,
                         testing::Combine(testing::Range(-10, 10),
                                          testing::Range(-10, 10)));

TEST(TopNTest, BasicBehavior) {
  TopN<int, double> top3(3);
  top3.Add(1, 1.0);
  top3.Add(2, 2.0);
  EXPECT_THAT(top3.UnorderedElements(), ElementsAre(1, 2));
  top3.Add(3, 2.0);
  EXPECT_THAT(top3.UnorderedElements(), ElementsAre(1, 2, 3));
  top3.Add(4, 7);
  EXPECT_THAT(top3.UnorderedElements(), ElementsAre(4, 2, 3));
}

TEST(TopNTest, Random) {
  TopN<int, int> topN(4);
  std::vector<int> input;
  for (int i = 0; i < 1000; ++i) input.push_back(i);
  std::shuffle(input.begin(), input.end(), absl::BitGen());
  for (const int value : input) topN.Add(value, value);
  EXPECT_THAT(topN.UnorderedElements(),
              UnorderedElementsAre(999, 998, 997, 996));
}

TEST(AtMostOneDecompositionTest, DetectFullClique) {
  std::vector<std::vector<int>> graph{
      {1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}};
  std::vector<int> buffer;
  absl::BitGen random;
  const auto decompo = AtMostOneDecomposition(graph, random, &buffer);
  EXPECT_THAT(decompo, ElementsAre(UnorderedElementsAre(0, 1, 2, 3)));
}

TEST(AtMostOneDecompositionTest, DetectDisjointCliques) {
  std::vector<std::vector<int>> graph{
      {1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2}, {5, 6}, {4, 6}, {4, 5}};
  std::vector<int> buffer;
  absl::BitGen random;
  const auto decompo = AtMostOneDecomposition(graph, random, &buffer);
  EXPECT_THAT(decompo, UnorderedElementsAre(UnorderedElementsAre(0, 1, 2, 3),
                                            UnorderedElementsAre(4, 5, 6)));
}

TEST(WeightedPickTest, SizeOne) {
  std::vector<double> weights = {123.4};
  absl::BitGen random;
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(WeightedPick(weights, random), 0);
  }
}

TEST(WeightedPickTest, SimpleTest) {
  std::vector<double> weights = {1.0, 2.0, 3.0};
  absl::BitGen random;
  const int kSample = 1e6;
  std::vector<int> counts(3, 0);
  for (int i = 0; i < kSample; ++i) {
    counts[WeightedPick(weights, random)]++;
  }
  for (int i = 0; i < weights.size(); ++i) {
    EXPECT_LE(
        std::abs(weights[i] / 6.0 - static_cast<double>(counts[i]) * 1e-6),
        1e-2);
  }
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
