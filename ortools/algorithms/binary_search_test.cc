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

#include "ortools/algorithms/binary_search.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/numeric/int128.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/hash.h"

namespace operations_research {

// Correctly picking the midpoint of two integers in all cases isn't trivial!
template <>
inline int BinarySearchMidpoint(int x, int y) {
  if (x > y) std::swap(x, y);
  if (x >= 0 || y < 0) return x + (y - x) / 2;
  return (x + y) / 2;
}

namespace {

TEST(BinarySearchTest, DoubleExample) {
  // M_PI is problematic on windows.
  // std::numbers::pi is C++20 (incompatible with OR-Tools).
  const double kPi = 3.14159265358979323846;
  const double x =
      BinarySearch<double>(/*x_true=*/0.0, /*x_false=*/kPi / 2,
                           [](double x) { return cos(x) >= 2 * sin(x); });
  EXPECT_GE(x, 0);
  EXPECT_LE(x, kPi / 2);
  EXPECT_EQ(cos(x), 2 * sin(x)) << x;
}

template <typename T>
class BinarySearchIntTest : public ::testing::Test {};

TYPED_TEST_SUITE_P(BinarySearchIntTest);

TYPED_TEST_P(BinarySearchIntTest, IntExampleWithReversedIntervalOrder) {
  EXPECT_EQ(
      BinarySearch<TypeParam>(/*x_true=*/67, /*x_false=*/23,
                              [](TypeParam x) { return x > TypeParam{42}; }),
      43);
}

TYPED_TEST_P(BinarySearchIntTest, IntOverflowStressTest) {
  const TypeParam kBounds[] = {std::numeric_limits<TypeParam>::min(),
                               std::numeric_limits<TypeParam>::min() + 1,
                               std::numeric_limits<TypeParam>::min() + 2,
                               std::numeric_limits<TypeParam>::min() + 3,
                               0,
                               1,
                               2,
                               3,
                               std::numeric_limits<TypeParam>::max() - 3,
                               std::numeric_limits<TypeParam>::max() - 2,
                               std::numeric_limits<TypeParam>::max() - 1,
                               std::numeric_limits<TypeParam>::max()};
  for (TypeParam x : kBounds) {
    for (TypeParam y : kBounds) {
      if (x == y) continue;
      ASSERT_EQ(BinarySearch<TypeParam>(/*x_true=*/x, /*x_false=*/y,
                                        [x](TypeParam t) { return t == x; }),
                x)
          << "x=" << x << ", y=" << y;
    }
  }
}

REGISTER_TYPED_TEST_SUITE_P(BinarySearchIntTest,
                            IntExampleWithReversedIntervalOrder,
                            IntOverflowStressTest);
using MyTypes = ::testing::Types<int, unsigned, int64_t, uint64_t, absl::int128,
                                 absl::uint128>;
INSTANTIATE_TYPED_TEST_SUITE_P(My, BinarySearchIntTest, MyTypes);

TEST(BinarySearchTest, LargeInt128SearchDomain) {
  absl::int128 target = -1234567890123456789;
  target <<= 50;  // Make sure it does need more than 64 or even 96 bits.
  EXPECT_EQ(BinarySearch<absl::int128>(
                /*x_true=*/std::numeric_limits<absl::int128>::min(),
                /*x_false=*/std::numeric_limits<absl::int128>::max(),
                [target](absl::int128 x) { return x < target; }),
            target - 1);
}

TEST(BinarySearchTest, VeryLongDoubleSearchDomain) {
  // Binary search for the smallest possible long double that is > 0,
  // starting with interval [0, numeric_limit::max()]. This is probably close to
  // the longest possible binary search on a widely-available numerical type.
  EXPECT_EQ(BinarySearch<long double>(
                /*x_true=*/std::numeric_limits<long double>::max(),
                /*x_false=*/0.0, [](long double x) { return x > 0; }),
            std::numeric_limits<long double>::denorm_min());
}

TEST(BinarySearchTest, InfinityCornerCases) {
  constexpr double kInfinity = std::numeric_limits<double>::infinity();
  EXPECT_THAT(BinarySearch<double>(
                  /*x_true=*/-kInfinity,
                  /*x_false=*/kInfinity, [](double x) { return x < 0; }),
              -kInfinity);
  EXPECT_EQ(BinarySearch<double>(
                /*x_true=*/-1,
                /*x_false=*/kInfinity, [](double x) { return x < 0; }),
            -1);
  EXPECT_THAT(BinarySearch<double>(
                  /*x_true=*/kInfinity,
                  /*x_false=*/0, [](double x) { return x > 0; }),
              kInfinity);
}

TEST(BinarySearchTest, NanCornerCases) {
  EXPECT_THAT(BinarySearch<double>(
                  /*x_true=*/std::numeric_limits<double>::quiet_NaN(),
                  /*x_false=*/0, [](double x) { return !(x == 0); }),
              testing::IsNan());
  EXPECT_EQ(BinarySearch<double>(
                /*x_true=*/0,
                /*x_false=*/std::numeric_limits<double>::quiet_NaN(),
                [](double x) { return x == 0; }),
            0);
}

TEST(BinarySearchTest, WithAbslDuration) {
  EXPECT_THAT(BinarySearch<absl::Duration>(
                  /*x_true=*/absl::Hours(100000),
                  /*x_false=*/absl::ZeroDuration(),
                  [](absl::Duration x) { return x > absl::ZeroDuration(); }),
              // Smallest non-zero absl::Duration.
              absl::Nanoseconds(0.25));
  EXPECT_EQ(BinarySearch<absl::Duration>(
                /*x_true=*/absl::InfiniteDuration(),
                /*x_false=*/-absl::Seconds(100),
                [](absl::Duration t) { return t > absl::Seconds(1); }),
            absl::InfiniteDuration());
}

TEST(BinarySearchDeathTest, DiesIfEitherBoundaryConditionViolatedInFastbuild) {
  if (!DEBUG_MODE) GTEST_SKIP();
  EXPECT_DEATH(BinarySearch<int>(/*x_true=*/0, /*x_false=*/42,
                                 [](int x) { return x < 999; }),
               "");
  EXPECT_DEATH(BinarySearch<int>(/*x_true=*/0, /*x_false=*/42,
                                 [](int x) { return x < 0; }),
               "");
  EXPECT_DEATH(BinarySearch<int>(/*x_true=*/0, /*x_false=*/42,
                                 [](int x) { return x > 20; }),
               "");
}

}  // namespace

// Examples of cases where one needs to override BinarySearchMidpoint() to get
// correct results.
// Note that template specializations must be exactly in the same namespace,
// hence the presence of these tests outside the unnamed namespace.

template <>
inline absl::Time BinarySearchMidpoint(absl::Time x, absl::Time y) {
  return x + (y - x) / 2;
}

TEST(BinarySearchTest, WithAbslTime) {
  const absl::Time t0 = absl::Now();
  EXPECT_EQ(BinarySearch<absl::Time>(
                /*x_true=*/t0 + absl::Hours(1),
                /*x_false=*/t0, [t0](absl::Time x) { return x > t0; }),
            t0 + absl::Nanoseconds(0.25));
  EXPECT_EQ(BinarySearch<absl::Time>(
                /*x_true=*/absl::InfinitePast(),
                /*x_false=*/absl::Now() + absl::Seconds(100),
                [](absl::Time x) { return x < absl::Now(); }),
            absl::InfinitePast());
}

TEST(BinarySearchTest, NonMonoticPredicateReachesLocalInflexionPoint_Double) {
  absl::BitGen random;
  auto generate_random_double = [&random]() {
    // We generate the sign, mantissa and exponent separately.
    return (absl::Bernoulli(random, 0.5) ? 1 : -1) *
           scalbn(absl::Uniform<double>(random, 1, 2),
                  absl::Uniform<int>(random, -1023, 1023));
  };
  constexpr double kEps = std::numeric_limits<double>::epsilon();
  const int kNumAttempts = 100000;
  for (int attempt = 0; attempt < kNumAttempts; ++attempt) {
    const uint64_t hash_seed = random();
    std::function<bool(double)> non_monotonic_predicate =
        [hash_seed](double x) {
          return fasthash64(reinterpret_cast<const char*>(&x), sizeof(x),
                            hash_seed) &
                 1;
        };
    // Pick a random [x_true, x_false] interval which verifies f(x_true) = true
    // and f(x_false) = false.
    double x_true, x_false;
    do {
      x_true = generate_random_double();
    } while (!non_monotonic_predicate(x_true));
    // x_false will either be set to a another random double, or to a small
    // perturbation from x_true.
    if (absl::Bernoulli(random, 0.5)) {
      // random double.
      do {
        x_false = generate_random_double();
      } while (non_monotonic_predicate(x_false));
    } else {
      // small perturbation from x_true.
      do {
        const double eps = absl::LogUniform(random, 1, 1000) * kEps;
        x_false = x_true * (1 + (absl::Bernoulli(random, 0.5) ? eps : -eps));
      } while (non_monotonic_predicate(x_false));
    }
    ASSERT_NE(x_true, x_false);

    // Verify that our predicate is deterministic.
    for (int i = 0; i < 20; ++i) {
      ASSERT_TRUE(non_monotonic_predicate(x_true));
    }
    for (int i = 0; i < 20; ++i) {
      ASSERT_FALSE(non_monotonic_predicate(x_false));
    }

    // Perform the binary search.
    const double solution =
        BinarySearch(x_true, x_false, non_monotonic_predicate);
    SCOPED_TRACE(absl::StrFormat("x_true=%.16g, x_false=%.16g, solution=%.16g",
                                 x_true, x_false, solution));
    // Verify that the solution is in [x_true, x_false[.
    if (x_true < x_false) {
      ASSERT_GE(solution, x_true);
      ASSERT_LT(solution, x_false);
    } else {
      ASSERT_LE(solution, x_true);
      ASSERT_GT(solution, x_false);
    }
    // Verify that f(solution')=false, where solution' is the smallest double
    // "after" solution (in the x_true->x_false direction).
    ASSERT_FALSE(non_monotonic_predicate(std::nextafter(solution, x_false)));
  }
}

TEST(BinarySearchTest, NonDeterministicPredicateStillConverges) {
  if (DEBUG_MODE) {
    GTEST_SKIP() << "DCHECKs catch f(x_true)=false or f(x_false)=true.";
  }
  absl::BitGen random;
  std::function<bool(int)> random_predicate = [&random](int) {
    return absl::Bernoulli(random, 0.5);
  };
  const int kNumAttempts = 100000;
  for (int attempt = 0; attempt < kNumAttempts; ++attempt) {
    const int x_true = random();
    const int x_false = absl::Bernoulli(random, 0.5)
                            ? x_true + (absl::Bernoulli(random, 0.5) ? 1 : -1) *
                                           absl::LogUniform(random, 0, 1000)
                            : random();
    const int solution = BinarySearch(x_true, x_false, random_predicate);
    SCOPED_TRACE(absl::StrFormat("x_true=%d, x_false=%d, solution=%d", x_true,
                                 x_false, solution));
    if (x_false == x_true) {
      ASSERT_EQ(solution, x_true);
    } else if (x_true < x_false) {
      ASSERT_GE(solution, x_true);
      ASSERT_LT(solution, x_false);
    } else {
      ASSERT_LE(solution, x_true);
      ASSERT_GT(solution, x_false);
    }
  }
}

template <typename T>
void BM_BinarySearch(benchmark::State& state) {
  auto functor = [](T x) { return x > std::numeric_limits<T>::max() / 2; };
  for (const auto s : state) {
    benchmark::DoNotOptimize(functor);
    auto result = BinarySearch<T>(std::numeric_limits<T>::max(),
                                  std::numeric_limits<T>::min(), functor);
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_BinarySearch<float>);
BENCHMARK(BM_BinarySearch<double>);
BENCHMARK(BM_BinarySearch<int>);
BENCHMARK(BM_BinarySearch<unsigned>);
BENCHMARK(BM_BinarySearch<int64_t>);
BENCHMARK(BM_BinarySearch<uint64_t>);
BENCHMARK(BM_BinarySearch<absl::int128>);

TEST(ConvexMinimumTest, ExhaustiveTest) {
  const int n = 99;
  std::vector<int> points(n);
  std::vector<int> values(n);
  for (int i = 0; i < n; ++i) points[i] = i;

  int total_num_queries = 0;
  int max_num_queries = 0;
  for (int b1 = 0; b1 < n; ++b1) {
    for (int i = b1; i >= 0; --i) values[i] = b1 - i;
    for (int b2 = b1; b2 < n; ++b2) {
      for (int i = b2; i < n; ++i) values[i] = i - b2;
      int num_queries = 0;
      const auto [point, value] = ConvexMinimum<int, int>(points, [&](int p) {
        ++num_queries;
        return values[p];
      });
      total_num_queries += num_queries;
      max_num_queries = std::max(max_num_queries, num_queries);
      EXPECT_EQ(value, 0);
      EXPECT_GE(point, b1);
      EXPECT_LE(point, b2);
      // Fail after one example.
      ASSERT_TRUE(value == 0 && b1 <= point && point <= b2)
          << "queries: " << num_queries << " opt range: [" << b1 << ", " << b2
          << "]";
    }
  }

  // TODO(user): we can probably do better.
  EXPECT_EQ(total_num_queries, 19376);
  EXPECT_EQ(max_num_queries, 12);
}

TEST(ConvexMinimumTest, OneQueryIfSizeOne) {
  std::vector<int> points{0};
  std::vector<double> values{0.0};
  int num_queries = 0;
  const auto [point, value] = ConvexMinimum<int, int>(points, [&](int p) {
    ++num_queries;
    return values[p];
  });
  EXPECT_EQ(point, 0);
  EXPECT_EQ(value, 0.0);
  EXPECT_EQ(num_queries, 1);
}

TEST(ConvexMinimumTest, TwoQueriesIfSizeTwo) {
  std::vector<int> points{0, 1};
  std::vector<double> values{0.0, 1.0};
  int num_queries = 0;
  const auto [point, value] = ConvexMinimum<int, int>(points, [&](int p) {
    ++num_queries;
    return values[p];
  });
  EXPECT_EQ(point, 0);
  EXPECT_EQ(value, 0.0);
  EXPECT_EQ(num_queries, 2);
}

TEST(ConvexMinimumTest, TwoQueriesIfSizeTwoReversed) {
  std::vector<int> points{0, 1};
  std::vector<double> values{1.0, 0.0};
  int num_queries = 0;
  const auto [point, value] = ConvexMinimum<int, int>(points, [&](int p) {
    ++num_queries;
    return values[p];
  });
  EXPECT_EQ(point, 1);
  EXPECT_EQ(value, 0.0);
  EXPECT_EQ(num_queries, 2);
}

TEST(RangeConvexMinimumTest, HugeRangeTest) {
  int total_num_queries = 0;
  int max_num_queries = 0;
  for (int b1 = -100; b1 < 100; ++b1) {
    for (int b2 = b1; b2 < b1 + 100; ++b2) {
      int num_queries = 0;
      const auto [point, value] = RangeConvexMinimum<int64_t, double>(
          std::numeric_limits<int64_t>::min() / 2,
          std::numeric_limits<int64_t>::max() / 2, [&](int64_t v) -> double {
            ++num_queries;
            if (v < b1) {
              return b1 - v;
            } else if (v > b2) {
              return v - b2;
            }
            return 0;
          });
      total_num_queries += num_queries;
      max_num_queries = std::max(max_num_queries, num_queries);
      EXPECT_EQ(value, 0);
      EXPECT_GE(point, b1);
      EXPECT_LE(point, b2);
      // Don't continue past the first failing example to limit the number of
      // errors.
      ASSERT_TRUE(value == 0 && b1 <= point && point <= b2)
          << "queries: " << num_queries << " opt range: [" << b1 << ", " << b2
          << "]";
    }
  }
  // 80 is the worst case we would expect from ternary search: 2*log_3(2^63).
  EXPECT_LE(max_num_queries, 80);
}
}  // namespace operations_research
