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

#include "ortools/util/range_query_function.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "absl/log/check.h"
#include "gtest/gtest.h"
#include "ortools/base/types.h"

namespace operations_research {
namespace {

struct InsideIntervalTest {
  int64_t range_begin;
  int64_t range_end;
  int64_t interval_begin;
  int64_t interval_end;
  int64_t first_inside_the_interval;
  int64_t last_inside_the_interval;
};

struct TestCase {
  std::function<int64_t(int64_t)> f;
  int64_t domain_start;
  int64_t domain_end;
  std::vector<InsideIntervalTest> inside_interval_tests;
};

class RangeQueryFunctionTest : public testing::Test {
 protected:
  void RunTests(const RangeIntToIntFunction* f, const TestCase& test);
  static const struct TestCase kTests[];
};

const TestCase RangeQueryFunctionTest::kTests[] = {
    {
        // TODO(user): add more inside_interval_tests
        [](int64_t x) -> int64_t { return x; },
        /*domain_start=*/12,
        /*domain_end=*/17,
        /*inside_interval_tests=*/
        {
            {14, 16, 15, 16, 15, 15},
            {12, 13, 12, 13, 12, 12},
        },
    },
    {
        [](int64_t x) -> int64_t { return x % 4; },
        /*domain_start=*/11,
        /*domain_end=*/16,
        /*inside_interval_tests=*/
        {
            {11, 16, 1, 3, 13, 14},
        },
    },
    {
        [](int64_t x) -> int64_t { return 1000. * sin(x); },
        /*domain_start=*/-3,
        /*domain_end=*/3,
        /*inside_interval_tests=*/{},
    },
    {
        [](int64_t x) -> int64_t {
          const double value = sin(x) * cosh(x) * exp(x);
          if (value > kint64max) return kint64max;
          if (value < kint64min) return kint64min;
          return value;
        },
        /*domain_start=*/1153,
        /*domain_end=*/1157,
        /*inside_interval_tests=*/{},
    },
};

void RangeQueryFunctionTest::RunTests(const RangeIntToIntFunction* f,
                                      const TestCase& test) {
  for (int64_t i = test.domain_start; i < test.domain_end; ++i) {
    EXPECT_EQ(test.f(i), f->Query(i));
  }

  for (int64_t from = test.domain_start; from < test.domain_end; ++from) {
    for (int64_t to = from + 1; to <= test.domain_end; ++to) {
      int64_t brute_force_min = kint64max;
      int64_t brute_force_max = kint64min;
      for (int64_t i = from; i < to; ++i) {
        brute_force_min = std::min(brute_force_min, test.f(i));
        brute_force_max = std::max(brute_force_max, test.f(i));
      }
      EXPECT_EQ(brute_force_min, f->RangeMin(from, to));
      EXPECT_EQ(brute_force_max, f->RangeMax(from, to));
    }
  }

  for (const InsideIntervalTest& inside_interval_test :
       test.inside_interval_tests) {
    EXPECT_EQ(inside_interval_test.first_inside_the_interval,
              f->RangeFirstInsideInterval(inside_interval_test.range_begin,
                                          inside_interval_test.range_end,
                                          inside_interval_test.interval_begin,
                                          inside_interval_test.interval_end));
    EXPECT_EQ(inside_interval_test.last_inside_the_interval,
              f->RangeLastInsideInterval(inside_interval_test.range_begin,
                                         inside_interval_test.range_end,
                                         inside_interval_test.interval_begin,
                                         inside_interval_test.interval_end));
  }
}

TEST_F(RangeQueryFunctionTest, BareIntToIntFunction) {
  for (const TestCase& test : kTests) {
    std::unique_ptr<RangeIntToIntFunction> f(MakeBareIntToIntFunction(test.f));
    RunTests(f.get(), test);
  }
}

TEST_F(RangeQueryFunctionTest, CachedIntToIntFunction) {
  for (const TestCase& test : kTests) {
    std::unique_ptr<RangeIntToIntFunction> f(
        MakeCachedIntToIntFunction(test.f, test.domain_start, test.domain_end));
    RunTests(f.get(), test);
  }
}

// The index rmq test is responsible for testing extreme sequences of values.
// This test should rather check if RangeMinMaxIndexFunction correctly calls the
// rmq, if functions with positive/negative domains work and the behaviour when
// the range is invalid.
class RangeMinMaxIndexFunctionTest : public testing::Test {
 protected:
  static int64_t CorrectResultsTestFunction(const int64_t arg) {
    static const auto kValues = std::to_array<int64_t>({6, 8, -1, 3, 6});
    static const int64_t kOffset = 2;
    CHECK(arg + kOffset < kValues.size());
    return kValues[arg + kOffset];
  }
  static int64_t Identity(int64_t arg) { return arg; }
};

TEST_F(RangeMinMaxIndexFunctionTest, CorrectResults) {
  std::unique_ptr<RangeMinMaxIndexFunction> f(
      MakeCachedRangeMinMaxIndexFunction(CorrectResultsTestFunction, -2, 3));
  EXPECT_EQ(0, f->RangeMinArgument(-2, 3));
  EXPECT_EQ(-1, f->RangeMaxArgument(-2, 3));
  EXPECT_EQ(-2, f->RangeMinArgument(-2, 0));
  EXPECT_EQ(-1, f->RangeMaxArgument(-2, 0));
  EXPECT_EQ(1, f->RangeMinArgument(1, 3));
  EXPECT_EQ(2, f->RangeMaxArgument(1, 3));
}

TEST_F(RangeMinMaxIndexFunctionTest, DomainCheck) {
  const std::function<int64_t(int64_t)> Identity = [](int64_t x) -> int64_t {
    return x;
  };
  {
    std::unique_ptr<RangeMinMaxIndexFunction> negative_domain(
        MakeCachedRangeMinMaxIndexFunction(Identity, -200, -100));
    for (int64_t range_start : {-160, -150, -180}) {
      for (int64_t range_end : {-140, -149, -100}) {
        EXPECT_EQ(range_start,
                  negative_domain->RangeMinArgument(range_start, range_end));
        EXPECT_EQ(range_end - 1,
                  negative_domain->RangeMaxArgument(range_start, range_end));
      }
    }
  }
  {
    std::unique_ptr<RangeMinMaxIndexFunction> positive_domain(
        MakeCachedRangeMinMaxIndexFunction(Identity, 100, 200));
    for (int64_t range_start : {140, 150, 100}) {
      for (int64_t range_end : {160, 151, 180}) {
        EXPECT_EQ(range_start,
                  positive_domain->RangeMinArgument(range_start, range_end));
        EXPECT_EQ(range_end - 1,
                  positive_domain->RangeMaxArgument(range_start, range_end));
      }
    }
  }
  {
    std::unique_ptr<RangeMinMaxIndexFunction> kint64min_domain(
        MakeCachedRangeMinMaxIndexFunction(Identity, kint64min, kint64min + 1));
    EXPECT_EQ(kint64min,
              kint64min_domain->RangeMinArgument(kint64min, kint64min + 1));
    EXPECT_EQ(kint64min,
              kint64min_domain->RangeMaxArgument(kint64min, kint64min + 1));

    std::unique_ptr<RangeMinMaxIndexFunction> kint64max_domain(
        MakeCachedRangeMinMaxIndexFunction(Identity, kint64max - 1, kint64max));
    EXPECT_EQ(kint64max - 1,
              kint64max_domain->RangeMinArgument(kint64max - 1, kint64max));
    EXPECT_EQ(kint64max - 1,
              kint64max_domain->RangeMaxArgument(kint64max - 1, kint64max));
  }
  EXPECT_DEATH(MakeCachedRangeMinMaxIndexFunction(Identity, 0, 0),
               "Check failed: [^ ]*");
}

#ifndef NDEBUG
// RangeMinArgument and RangeMaxArgument are performance citical functions, and
// they are guarded with DCHECKs. Their behavior is undefined in release mode,
// so we check debug mode only.
TEST_F(RangeMinMaxIndexFunctionTest, InvalidRangeQuery) {
  std::unique_ptr<RangeMinMaxIndexFunction> f(
      MakeCachedRangeMinMaxIndexFunction(Identity, 0, 10));
  EXPECT_DEATH(f->RangeMinArgument(-1, 10), "Check failed: [^ ]*");
  EXPECT_DEATH(f->RangeMinArgument(0, 11), "Check failed: [^ ]*");
  EXPECT_DEATH(f->RangeMinArgument(0, 0), "Check failed: [^ ]*");
  EXPECT_DEATH(f->RangeMinArgument(5, 5), "Check failed: [^ ]*");
  EXPECT_DEATH(f->RangeMinArgument(10, 10), "Check failed: [^ ]*");
  EXPECT_DEATH(f->RangeMinArgument(-1, -20), "Check failed: [^ ]*");

  EXPECT_DEATH(f->RangeMaxArgument(-1, 10), "Check failed: [^ ]*");
  EXPECT_DEATH(f->RangeMaxArgument(0, 11), "Check failed: [^ ]*");
  EXPECT_DEATH(f->RangeMaxArgument(0, 0), "Check failed: [^ ]*");
  EXPECT_DEATH(f->RangeMaxArgument(5, 5), "Check failed: [^ ]*");
  EXPECT_DEATH(f->RangeMaxArgument(10, 10), "Check failed: [^ ]*");
  EXPECT_DEATH(f->RangeMaxArgument(-1, -20), "Check failed: [^ ]*");
}
#endif  // NDEBUG
}  // namespace
}  // namespace operations_research
