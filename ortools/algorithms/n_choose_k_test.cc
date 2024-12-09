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

#include "ortools/algorithms/n_choose_k.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
//#include "ortools/base/fuzztest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/mathutil.h"
#include "ortools/util/flat_matrix.h"

namespace operations_research {
namespace {
//using ::fuzztest::NonNegative;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr int64_t kint64max = std::numeric_limits<int64_t>::max();

TEST(NChooseKTest, TrivialErrorCases) {
  absl::BitGen random;
  constexpr int kNumTests = 100'000;
  for (int t = 0; t < kNumTests; ++t) {
    const int64_t x = absl::LogUniform<int64_t>(random, 0, kint64max);
    EXPECT_THAT(NChooseK(-1, x), StatusIs(absl::StatusCode::kInvalidArgument,
                                          HasSubstr("n is negative")));
    EXPECT_THAT(NChooseK(x, -1), StatusIs(absl::StatusCode::kInvalidArgument,
                                          HasSubstr("k is negative")));
    if (x != kint64max) {
      EXPECT_THAT(NChooseK(x, x + 1),
                  StatusIs(absl::StatusCode::kInvalidArgument,
                           HasSubstr("greater than n")));
    }
    ASSERT_FALSE(HasFailure()) << DUMP_VARS(t, x);
  }
}

TEST(NChooseKTest, Symmetry) {
  absl::BitGen random;
  constexpr int kNumTests = 1'000'000;
  for (int t = 0; t < kNumTests; ++t) {
    const int64_t n = absl::LogUniform<int64_t>(random, 0, kint64max);
    const int64_t k = absl::LogUniform<int64_t>(random, 0, n);
    const absl::StatusOr<int64_t> result1 = NChooseK(n, k);
    const absl::StatusOr<int64_t> result2 = NChooseK(n, n - k);
    if (result1.ok()) {
      ASSERT_THAT(result2, IsOkAndHolds(result1.value())) << DUMP_VARS(t, n, k);
    } else {
      ASSERT_EQ(result2.status().code(), result1.status().code())
          << DUMP_VARS(t, n, k, result1, result2);
    }
  }
}

TEST(NChooseKTest, Invariant) {
  absl::BitGen random;
  constexpr int kNumTests = 1'000'000;
  int num_tested_invariants = 0;
  for (int t = 0; t < kNumTests; ++t) {
    const int64_t n = absl::LogUniform<int64_t>(random, 2, 100);
    const int64_t k = absl::LogUniform<int64_t>(random, 1, n - 1);
    const absl::StatusOr<int64_t> n_k = NChooseK(n, k);
    const absl::StatusOr<int64_t> nm1_k = NChooseK(n - 1, k);
    const absl::StatusOr<int64_t> nm1_km1 = NChooseK(n - 1, k - 1);
    if (n_k.ok()) {
      ++num_tested_invariants;
      ASSERT_OK(nm1_k);
      ASSERT_OK(nm1_km1);
      ASSERT_EQ(n_k.value(), nm1_k.value() + nm1_km1.value())
          << DUMP_VARS(t, n, k, n_k, nm1_k, nm1_km1);
    }
  }
  EXPECT_GE(num_tested_invariants, kNumTests / 10);
}

TEST(NChooseKTest, ComparisonAgainstClosedFormsForK0) {
  for (int64_t n : {int64_t{0}, int64_t{1}, kint64max}) {
    EXPECT_THAT(NChooseK(n, 0), IsOkAndHolds(1)) << n;
  }
  absl::BitGen random;
  constexpr int kNumTests = 1'000'000;
  for (int t = 0; t < kNumTests; ++t) {
    const int64_t n = absl::LogUniform<int64_t>(random, 0, kint64max);
    ASSERT_THAT(NChooseK(n, 0), IsOkAndHolds(1)) << DUMP_VARS(n, t);
  }
}

TEST(NChooseKTest, ComparisonAgainstClosedFormsForK1) {
  for (int64_t n : {int64_t{1}, kint64max}) {
    EXPECT_THAT(NChooseK(n, 1), IsOkAndHolds(n));
  }
  absl::BitGen random;
  constexpr int kNumTests = 1'000'000;
  for (int t = 0; t < kNumTests; ++t) {
    const int64_t n = absl::LogUniform<int64_t>(random, 1, kint64max);
    ASSERT_THAT(NChooseK(n, 1), IsOkAndHolds(n)) << DUMP_VARS(t);
  }
}

TEST(NChooseKTest, ComparisonAgainstClosedFormsForK2) {
  // 2^32 Choose 2 = 2^32 × (2^32-1) / 2 = 2^63 - 2^31 < kint64max,
  // but (2^32+1) Choose 2 = 2^63 + 2^31 overflows.
  constexpr int64_t max_n = int64_t{1} << 32;
  for (int64_t n : {int64_t{2}, max_n}) {
    const int64_t n_choose_2 =
        static_cast<int64_t>(absl::uint128(n) * (n - 1) / 2);
    EXPECT_THAT(NChooseK(n, 2), IsOkAndHolds(n_choose_2)) << DUMP_VARS(n);
  }
  EXPECT_THAT(NChooseK(max_n + 1, 2),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("overflows int64")));

  absl::BitGen random;
  constexpr int kNumTests = 100'000;
  // Random valid results.
  for (int t = 0; t < kNumTests; ++t) {
    const int64_t n = absl::LogUniform<int64_t>(random, 2, max_n);
    const int64_t n_choose_2 =
        static_cast<int64_t>(absl::uint128(n) * (n - 1) / 2);
    ASSERT_THAT(NChooseK(n, 2), IsOkAndHolds(n_choose_2)) << DUMP_VARS(t, n);
  }
  // Random overflows.
  for (int t = 0; t < kNumTests; ++t) {
    const int64_t n = absl::LogUniform<int64_t>(random, max_n + 1, kint64max);
    ASSERT_THAT(NChooseK(n, 2), StatusIs(absl::StatusCode::kInvalidArgument,
                                         HasSubstr("overflows int64")))
        << DUMP_VARS(t, n);
  }
}

TEST(NChooseKTest, ComparisonAgainstClosedFormsForK3) {
  // This is 1 + ∛6×2^21. Checked manually on Google's scientific calculator.
  const int64_t max_n =
      static_cast<int64_t>(1 + std::pow(6, 1.0 / 3) * std::pow(2, 21));
  for (int64_t n : {int64_t{3}, max_n}) {
    const int64_t n_choose_3 =
        static_cast<int64_t>(absl::uint128(n) * (n - 1) * (n - 2) / 6);
    EXPECT_THAT(NChooseK(n, 3), IsOkAndHolds(n_choose_3)) << DUMP_VARS(n);
  }
  EXPECT_THAT(NChooseK(max_n + 1, 3),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("overflows int64")));

  absl::BitGen random;
  constexpr int kNumTests = 100'000;
  // Random valid results.
  for (int t = 0; t < kNumTests; ++t) {
    const int64_t n = absl::LogUniform<int64_t>(random, 3, max_n);
    const int64_t n_choose_3 =
        static_cast<int64_t>(absl::uint128(n) * (n - 1) * (n - 2) / 6);
    ASSERT_THAT(NChooseK(n, 3), IsOkAndHolds(n_choose_3)) << DUMP_VARS(t, n);
  }
  // Random overflows.
  for (int t = 0; t < kNumTests; ++t) {
    const int64_t n = absl::LogUniform<int64_t>(random, max_n + 1, kint64max);
    ASSERT_THAT(NChooseK(n, 3), StatusIs(absl::StatusCode::kInvalidArgument,
                                         HasSubstr("overflows int64")))
        << DUMP_VARS(t, n);
  }
}

TEST(NChooseKTest, ComparisonAgainstClosedFormsForK4) {
  // This is 1.5 + ∜24 × 2^(63/4).
  // Checked manually on Google's scientific calculator.
  const int64_t max_n =
      static_cast<int64_t>(1.5 + std::pow(24, 1.0 / 4) * std::pow(2, 63.0 / 4));
  for (int64_t n : {int64_t{4}, max_n}) {
    const int64_t n_choose_4 = static_cast<int64_t>(absl::uint128(n) * (n - 1) *
                                                    (n - 2) * (n - 3) / 24);
    EXPECT_THAT(NChooseK(n, 4), IsOkAndHolds(n_choose_4)) << DUMP_VARS(n);
  }
  EXPECT_THAT(NChooseK(max_n + 1, 4),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("overflows int64")));

  absl::BitGen random;
  constexpr int kNumTests = 100'000;
  // Random valid results.
  for (int t = 0; t < kNumTests; ++t) {
    const int64_t n = absl::LogUniform<int64_t>(random, 4, max_n);
    const int64_t n_choose_4 = static_cast<int64_t>(absl::uint128(n) * (n - 1) *
                                                    (n - 2) * (n - 3) / 24);
    ASSERT_THAT(NChooseK(n, 4), IsOkAndHolds(n_choose_4)) << DUMP_VARS(t, n);
  }
  // Random overflows.
  for (int t = 0; t < kNumTests; ++t) {
    const int64_t n = absl::LogUniform<int64_t>(random, max_n + 1, kint64max);
    ASSERT_THAT(NChooseK(n, 4), StatusIs(absl::StatusCode::kInvalidArgument,
                                         HasSubstr("overflows int64")))
        << DUMP_VARS(t, n);
  }
}

TEST(NChooseKTest, ComparisonAgainstPascalTriangleForK5OrAbove) {
  // Fill the Pascal triangle. Use -1 for int64_t overflows. We go up to n =
  // 17000 because (17000 Choose 5) ≈ 1.2e19 which overflows an int64_t.
  constexpr int max_n = 17000;
  FlatMatrix<int64_t> triangle(max_n + 1, max_n + 1);
  for (int n = 0; n <= max_n; ++n) {
    triangle[n][0] = 1;
    triangle[n][n] = 1;
    for (int i = 1; i < n; ++i) {
      const int64_t a = triangle[n - 1][i - 1];
      const int64_t b = triangle[n - 1][i];
      if (a < 0 || b < 0 || absl::int128(a) + b > kint64max) {
        triangle[n][i] = -1;
      } else {
        triangle[n][i] = a + b;
      }
    }
  }
  // Checking all 17000²/2 slots would be too expensive, so we check each
  // "column" downwards until the first 10 overflows, and stop.
  for (int k = 5; k < max_n; ++k) {
    int num_overflows = 0;
    for (int n = k + 5; n < max_n; ++n) {
      if (num_overflows > 0) EXPECT_EQ(triangle[n][k], -1);
      if (triangle[n][k] < 0) {
        ++num_overflows;
        EXPECT_THAT(NChooseK(n, k), StatusIs(absl::StatusCode::kInvalidArgument,
                                             HasSubstr("overflows int64")));
        if (num_overflows > 10) break;
      } else {
        EXPECT_THAT(NChooseK(n, k), IsOkAndHolds(triangle[n][k]));
      }
    }
  }
}

void MatchesLogCombinations(int n, int k) {
  if (n < k) {
    std::swap(k, n);
  }
  const auto exact = NChooseK(n, k);
  const double log_approx = MathUtil::LogCombinations(n, k);
  if (exact.ok()) {
    // We accepted to compute the exact value, make sure that it matches the
    // approximation.
    ASSERT_NEAR(log(exact.value()), log_approx, 0.0001);
  } else {
    // We declined to compute the exact value, make sure that we had a good
    // reason to, i.e. that the result did indeed overflow.
    ASSERT_THAT(exact, StatusIs(absl::StatusCode::kInvalidArgument,
                                HasSubstr("overflows int64")));
    const double approx = exp(log_approx);
    ASSERT_GE(std::nextafter(approx, std::numeric_limits<double>::infinity()),
              std::numeric_limits<int64_t>::max())
        << "we declined to compute the exact value of NChooseK(" << n << ", "
        << k << "), but the log value is " << log_approx
        << " (value: " << approx << "), which fits in int64_t";
  }
}
/*
FUZZ_TEST(NChooseKTest, MatchesLogCombinations)
    // Ideally we'd test with `uint64_t`, but `LogCombinations` only accepts
    // `int`.
    .WithDomains(NonNegative<int>(), NonNegative<int>());
*/
template <int kMaxN, auto algo>
void BM_NChooseK(benchmark::State& state) {
  static constexpr int kNumInputs = 1000;
  // Use deterministic random numbers to avoid noise.
  std::mt19937 gen(42);
  std::uniform_int_distribution<int64_t> random(0, kMaxN);
  std::vector<std::pair<int64_t, int64_t>> inputs;
  inputs.reserve(kNumInputs);
  for (int i = 0; i < kNumInputs; ++i) {
    int64_t n = random(gen);
    int64_t k = random(gen);
    if (n < k) {
      std::swap(n, k);
    }
    inputs.emplace_back(n, k);
  }
  // Force the one-time, costly static initializations of NChooseK() to happen
  // before the benchmark starts.
  auto result = NChooseK(62, 31);
  benchmark::DoNotOptimize(result);

  // Start the benchmark.
  for (auto _ : state) {
    for (const auto [n, k] : inputs) {
      auto result = algo(n, k);
      benchmark::DoNotOptimize(result);
    }
  }
  state.SetItemsProcessed(state.iterations() * kNumInputs);
}
BENCHMARK(BM_NChooseK<30, operations_research::NChooseK>);  // int32_t domain.
BENCHMARK(
    BM_NChooseK<60, operations_research::NChooseK>);  // int{32,64} domain.
BENCHMARK(
    BM_NChooseK<100, operations_research::NChooseK>);  // int{32,64,128} domain.
BENCHMARK(
    BM_NChooseK<100, MathUtil::LogCombinations>);  // int{32,64,128} domain.

}  // namespace
}  // namespace operations_research
