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

#include "ortools/util/random_subset.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/numeric/int128.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/n_choose_k.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/mathutil.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/testing_utils.h"

namespace operations_research {
namespace {

using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class RandomSubsetTest : public testing::Test {
 protected:
  absl::BitGen random_;
};

TEST_F(RandomSubsetTest, ZeroOfZero) {
  EXPECT_THAT(RandomSubset(0, 0, random_), IsEmpty());
}

TEST_F(RandomSubsetTest, OneOfOne) {
  EXPECT_THAT(RandomSubset(1, 1, random_), ElementsAre(0));
}

TEST_F(RandomSubsetTest, ZeroOfOne) {
  EXPECT_THAT(RandomSubset(1, 0, random_), IsEmpty());
}

TEST_F(RandomSubsetTest, ThreeOfThree) {
  EXPECT_THAT(RandomSubset(3, 3, random_), UnorderedElementsAre(0, 1, 2));
}

TEST_F(RandomSubsetTest, ZeroOfThree) {
  EXPECT_THAT(RandomSubset(3, 0, random_), IsEmpty());
}

TEST_F(RandomSubsetTest, OneOfThree) {
  EXPECT_THAT(RandomSubset(3, 1, random_), ElementsAre(AnyOf(0, 1, 2)));
}

template <typename T>
class TypedRandomSubsetTest : public testing::Test {};

TYPED_TEST_SUITE_P(TypedRandomSubsetTest);

TYPED_TEST_P(TypedRandomSubsetTest, ResilienceToOverflowForSpecialCaseK) {
  std::mt19937 random;  // Deterministic, to avoid flakiness.
  constexpr int kNumTests = 100'000;
  constexpr TypeParam n = std::numeric_limits<TypeParam>::max();
  for (int k = 0; k <= 3; ++k) {
    TypeParam min_element = 0;
    TypeParam max_element = 0;
    for (int i = 0; i < kNumTests; ++i) {
      const std::vector<TypeParam> subset =
          RandomSubset<TypeParam>(n, k, random);
      ASSERT_EQ(subset.size(), k);
      for (int i = 1; i < k; ++i) {
        for (int j = 0; j < i; ++j) {
          ASSERT_NE(subset[i], subset[j]) << DUMP_VARS(subset);
        }
      }
      for (int i = 0; i < k; ++i) {
        ASSERT_GE(subset[i], 0);
        ASSERT_LT(subset[i], n);
        min_element = std::min(min_element, subset[i]);
        max_element = std::max(max_element, subset[i]);
      }
    }
    if (k > 0) {
      // To verify that we didn't crop the high-order bit or did something else,
      // funny, we check the approximate covered range of values.
      ASSERT_LE(min_element, 5 * (1 + n / kNumTests));
      ASSERT_GE(max_element, n - 5 * (1 + n / kNumTests));
    }
  }
}

REGISTER_TYPED_TEST_SUITE_P(TypedRandomSubsetTest,
                            ResilienceToOverflowForSpecialCaseK);

typedef testing::Types<char, unsigned char, int, unsigned int, int64_t,
                       uint64_t, absl::int128, absl::uint128>
    SmallAndBigIntTypes;

INSTANTIATE_TYPED_TEST_SUITE_P(Foo, TypedRandomSubsetTest, SmallAndBigIntTypes);

TEST_F(RandomSubsetTest, PerformanceForHugeNSmallK) {
  absl::Time start_time = absl::Now();
  constexpr int num_runs = DEBUG_MODE ? 10 : 100;
  for (int iter = 0; iter < num_runs; ++iter) {
    for (uint64_t n = 10000;; n *= 2) {
      for (uint64_t k = 2; k < 1000; k *= 2) {
        std::vector<uint64_t> subset = RandomSubset(n, k, random_);
        ASSERT_EQ(subset.size(), k);
        absl::flat_hash_set<uint64_t> elements(subset.begin(), subset.end());
        ASSERT_EQ(elements.size(), k) << "There are some duplicate elements";
        for (uint64_t x : subset) {
          ASSERT_GE(x, 0);
          ASSERT_LT(x, n);
        }
      }
      // We test this limit here because the next multiplication by 2 would
      // overflow.
      if (n >= 10'000'000'000'000'000'000u) break;
    }
  }
  const absl::Duration test_duration = absl::Now() - start_time;
  if (!DEBUG_MODE && !kAnyXsanEnabled) {
    // Performance in forge, as of 2021-04, opt: a little less than 0.2s.
    EXPECT_LE(test_duration, absl::Seconds(2));
  }
}

TEST(RandomSubsetOfTest, PerformanceForSmallNMinusK) {
  absl::Time start_time = absl::Now();
  constexpr int N = 1000000;
  constexpr int K = N - 10;
  std::vector<int> elements(N, 0);
  absl::c_iota(elements, 0);
  constexpr int num_runs = DEBUG_MODE ? 25000 : 250000;
  constexpr int num_full_checks = 10;
  absl::BitGen random;
  for (int iter = 0; iter < num_runs; ++iter) {
    absl::Span<int> subset = RandomSubsetOf(K, &elements, random);
    ASSERT_EQ(subset.size(), K);
    if (iter < num_full_checks) {
      std::vector<bool> hit(N, false);
      for (int x : subset) {
        ASSERT_GE(x, 0);
        ASSERT_LT(x, N);
        ASSERT_FALSE(hit[x]) << x;
        hit[x] = true;
      }
    }
  }
  const absl::Duration test_duration = absl::Now() - start_time;
  if (!DEBUG_MODE && !kAnyXsanEnabled) {
    // Performance in forge, as of 2021-04, opt: a little more than 0.2s.
    EXPECT_LE(test_duration, absl::Seconds(2));
  }
}

TEST(RandomSubsetOfTest, MovesElementsDoesNotCopy) {
  absl::Time start_time = absl::Now();
  constexpr int payload_size = 100000;
  constexpr int N = 1000;
  std::vector<std::vector<int>> elements(N);
  for (int i = 0; i < N; ++i) elements[i].assign(payload_size, i);
  constexpr int num_runs = DEBUG_MODE ? 50000 : 500000;
  absl::BitGen random;
  for (int iter = 0; iter < num_runs; ++iter) {
    const int K = iter % 2 == 0 ? 10 : N - 10;
    absl::Span<std::vector<int>> subset = RandomSubsetOf(K, &elements, random);
    ASSERT_EQ(subset.size(), K);
  }
  const absl::Duration test_duration = absl::Now() - start_time;
  if (!DEBUG_MODE && !kAnyXsanEnabled) {
    // Performance in forge, as of 2021-04, opt: a little more than 0.2s.
    EXPECT_LE(test_duration, absl::Seconds(2));
  }
}

// The N and K of NChooseK(N, K): choose K elements among N.
// Or, in our use case: get a random subset of K elements of [0..N).
struct NK {
  int n = 0;
  int k = 0;
};

class RandomSubsetOfStressTest : public testing::TestWithParam<NK> {
 protected:
  absl::BitGen random_;
};
INSTANTIATE_TEST_SUITE_P(
    VariousNK, RandomSubsetOfStressTest,
    // The idea is to run the stress test for values of (n,k) such that
    // choose(n, k) isn't so big, so that we can do the brute-force statistical
    // test in a reasonable time.
    testing::Values(NK{1, 1}, NK{2, 1}, NK{3, 1}, NK{3, 2}, NK{10, 0}, NK{9, 9},
                    NK{23, 1}, NK{7, 3}, NK{8, 6}, NK{37, 2}),

    // Pretty-print the NK parameter in the test name.
    [](testing::TestParamInfo<NK> p) {
      return absl::StrFormat("%d_%d", p.param.n, p.param.k);
    });

template <class T>
testing::Matcher<T> IsBetween(double lo, double hi) {
  return testing::AllOf(testing::Ge<T>(lo), testing::Le<T>(hi));
}

TEST_P(RandomSubsetOfStressTest, StatisticalCorrectness) {
  // Idea: we generate X * choose(N, K) random subsets of size K among N
  // elements, and verify that the obtained distribution is similar to a
  // uniform distribution over all possible choices.
  constexpr int X = 100;
  const int N = GetParam().n;
  const int K = GetParam().k;
  const int num_choices = NChooseK(N, K).value();
  const int num_trials = X * num_choices;
  std::vector<char> elements;
  for (int i = 0; i < N; ++i) elements.push_back(static_cast<char>('A' + i));
  absl::flat_hash_map<std::string, int> subset_count;
  for (int i = 0; i < num_trials; ++i) {
    absl::Span<char> subset_span = RandomSubsetOf(K, &elements, random_);
    std::string subset(subset_span.begin(), subset_span.end());
    std::sort(subset.begin(), subset.end());  // To canonicalize it.
    ++subset_count[subset];
  }
  // Verify that all produced subsets were, in fact, valid subsets.
  for (auto [subset, _] : subset_count) {
    ASSERT_EQ(subset.size(), K);
    std::vector<bool> hit(N, false);
    for (char c : subset) {
      ASSERT_TRUE(c >= 'A' && c < 'A' + N) << subset;
      ASSERT_FALSE(hit[c - 'A']) << "Repeated '" << c << "' in " << subset;
      hit[c - 'A'] = true;
    }
  }
  // With the expected count at X, the probability for a given choice of not
  // appearing at all is approximately exp(-X), so we do expect every
  // choice to appear.
  ASSERT_EQ(subset_count.size(), num_choices);
  // When there is are few possible subsets, we make simple statistical tests.
  if (num_choices < 100) {
    for (auto [subset, count] : subset_count) {
      ASSERT_THAT(count, IsBetween<int>(0.5 * X, 2 * X)) << absl::StrFormat(
          "Subset '%s' is anormally represented: expected %d, got %d", subset,
          X, count);
    }
    return;
  }
  // Verify that we roughly have a Gaussian, with a few tests that should be
  // robust enough to never fail (given enough "safety buffer"):
  // - the stddev
  // - the 68/95/99.7 rule, see:
  //   https://en.wikipedia.org/wiki/68%E2%80%9395%E2%80%9399.7_rule
  // For a reference on the formulas, see for example:
  // en.wikipedia.org/wiki/Binomial_distribution#Expected_value_and_variance,
  // with p = 1/num_choices and n = num_trials.
  const double p = 1.0 / num_choices;
  const double expected_stddev = std::sqrt(num_trials * p * (1 - p));
  // We only test the stddev value if there are enough choices. For example,
  // with only two choices, the expected stddev isn't zero, but it can happen
  // that both choices get exactly X as count.
  int64_t sum_squared_diff = 0;
  for (auto [_, count] : subset_count) {
    sum_squared_diff += (count - X) * (count - X);
  }
  const double observed_stddev =
      std::sqrt(static_cast<double>(sum_squared_diff) / num_choices);
  // The observed stddev can be quite different from the expected one.
  EXPECT_THAT(observed_stddev / expected_stddev, IsBetween<double>(0.25, 4.0))
      << DUMP_VARS(observed_stddev, expected_stddev);
  int num_within_stddev = 0;
  int num_between_1_and_2_stddev = 0;
  int num_between_2_and_3_stddev = 0;
  int num_beyond_3_stddev = 0;
  for (auto [_, count] : subset_count) {
    const double dev = std::abs(count - X);
    if (dev < expected_stddev) {
      ++num_within_stddev;
    } else if (dev < 2 * expected_stddev) {
      ++num_between_1_and_2_stddev;
    } else if (dev < 3 * expected_stddev) {
      ++num_between_2_and_3_stddev;
    } else {
      ++num_beyond_3_stddev;
    }
  }
  EXPECT_THAT(static_cast<double>(num_within_stddev) / num_choices,
              IsBetween<double>(0.3, 0.8));  // Mean: 0.68
  EXPECT_THAT(static_cast<double>(num_between_1_and_2_stddev) / num_choices,
              // Mean: 0.27
              IsBetween<double>(0.1, 0.6));
  EXPECT_THAT(static_cast<double>(num_between_2_and_3_stddev) / num_choices,
              // Mean: 0.05
              IsBetween<double>(0.015, 0.2));
  EXPECT_THAT(static_cast<double>(num_beyond_3_stddev) / num_choices,
              IsBetween<double>(num_choices < 30000 ? 0 : 0.001,
                                num_choices < 1000    ? 0.03
                                : num_choices < 10000 ? 0.02
                                                      : 0.01));
}

TEST(RandomSortedElementsTest, NoPoints) {
  absl::BitGen random;
  EXPECT_THAT(RandomSortedElements(/*n=*/0, /*k=*/0, random,
                                   /*allow_repetition=*/false),
              IsEmpty());
}

TEST(RandomSortedElementsTest, Singleton) {
  absl::BitGen random;
  EXPECT_THAT(RandomSortedElements(/*n=*/1, /*k=*/1, random,
                                   /*allow_repetition=*/true),
              ElementsAre(0));
  EXPECT_THAT(RandomSortedElements(/*n=*/1, /*k=*/1, random,
                                   /*allow_repetition=*/false),
              ElementsAre(0));
}

TEST(RandomValuesSpanningTest, NoRepetitions) {
  absl::BitGen random;
  EXPECT_THAT(RandomValuesSpanning(0, 3, /*num_values=*/3, random,
                                   /*allow_repetition=*/false),
              ElementsAre(0, testing::AnyOf(1, 2), 3));
}

TEST(RandomValuesSpanningTest, OnlyMiniAndMaxi) {
  absl::BitGen random;
  EXPECT_THAT(RandomValuesSpanning(-12, 15, /*num_values=*/2, random,
                                   /*allow_repetition=*/false),
              ElementsAre(-12, 15));
  EXPECT_THAT(RandomValuesSpanning(0, 1, /*num_values=*/2, random,
                                   /*allow_repetition=*/false),
              ElementsAre(0, 1));
}

TEST(RandomValuesSpanningTest, OnlyMiniAndMaxiAllowRepetition) {
  absl::BitGen random;
  EXPECT_THAT(RandomValuesSpanning(-12, 15, /*num_values=*/2, random,
                                   /*allow_repetition=*/true),
              ElementsAre(-12, 15));
  EXPECT_THAT(RandomValuesSpanning(-12, -12, /*num_values=*/2, random,
                                   /*allow_repetition=*/true),
              ElementsAre(-12, -12));
}

TEST(RandomSortedElementsAndRandomValuesSpanningTest,
     VerifyInvariantsOnRandomizedInputs) {
  absl::BitGen random;
  for (bool allow_repetition : {false, true}) {
    for (bool output_0_and_max : {false, true}) {
      // Time per test: 7μs in fastbuild, 1.7μs in opt (forge, 2022-08-29).
      constexpr int kNumTests = DEBUG_MODE ? 100'000 : 1'000'000;
      for (int attempt = 0; attempt < kNumTests; ++attempt) {
        const int k = absl::LogUniform(random, output_0_and_max ? 2 : 0, 15);
        const int min_n = allow_repetition ? 1 : std::max(1, k);
        const int n = absl::LogUniform(random, min_n, min_n + 1000);
        std::vector<int> sequence =
            output_0_and_max
                ? RandomValuesSpanning(0, n, k, random, allow_repetition)
                : RandomSortedElements(n, k, random, allow_repetition);
        EXPECT_EQ(sequence.size(), k);
        EXPECT_TRUE(std::is_sorted(sequence.begin(), sequence.end()));
        if (!allow_repetition) {
          for (int i = 1; i < sequence.size(); ++i) {
            EXPECT_GT(sequence[i], sequence[i - 1]) << i;
          }
        }
        if (output_0_and_max && !sequence.empty()) {
          EXPECT_EQ(sequence[0], 0);
          EXPECT_EQ(sequence.back(), n);
        }
        ASSERT_FALSE(HasFailure()) << DUMP_VARS(
            allow_repetition, output_0_and_max, attempt, k, n, sequence);
      }
    }
  }
}

TEST(RandomSortedElementsAndRandomValuesSpanningTest, Unbiased) {
  random_engine_t random;  // This is deterministic.
  for (bool allow_repetition : {false, true}) {
    for (bool output_0_and_max : {false, true}) {
      absl::btree_map<std::vector<int>, int> sequence_count;
      // Timing of this test, per partition: ~1.5μs in opt, ~5.4μs in fastbuild.
      constexpr int kNumPartitions = DEBUG_MODE ? 1'000'000 : 2'000'000;
      for (int i = 0; i < kNumPartitions; ++i) {
        std::vector<int> sequence =
            output_0_and_max
                ? RandomValuesSpanning(0, 6, 5, random, allow_repetition)
                : RandomSortedElements(
                      /*n=*/7, /*k=*/5, random, allow_repetition);
        ++sequence_count[std::move(sequence)];
      }
      const int kNumDistinctPartitions =
          output_0_and_max   ? allow_repetition ? NChooseK(9, 3).value()
                                                : NChooseK(5, 3).value()
          : allow_repetition ? NChooseK(11, 5).value()
                             : NChooseK(7, 5).value();
      const int kAvgExpectedCount = kNumPartitions / kNumDistinctPartitions;
      EXPECT_EQ(sequence_count.size(), kNumDistinctPartitions);
      int num_counts_equal_to_avg = 0;
      if (!HasFailure()) {
        for (const auto& [partition, count] : sequence_count) {
          EXPECT_GE(count, kAvgExpectedCount * 0.9);
          EXPECT_LE(count, kAvgExpectedCount * 1.1);
          // We also verify that our generation isn't artificially
          // super-balanced: it shouldn't always be *exactly* 1/n.
          if (count == kAvgExpectedCount) ++num_counts_equal_to_avg;
        }
      }
      EXPECT_LE(num_counts_equal_to_avg, kNumDistinctPartitions / 10);
      ASSERT_FALSE(HasFailure())
          << DUMP_VARS(allow_repetition, output_0_and_max);
    }
  }
}

TEST(RandomSortedElementsTest, NegativeNOkWhenKIsZero) {
  absl::BitGen random;
  EXPECT_THAT(RandomSortedElements(/*n=*/-1, /*k=*/0, random), IsEmpty());
}

TEST(RandomSortedElementsDeathTest, NegativeN) {
  absl::BitGen random;
  EXPECT_DEATH(RandomSortedElements(/*n=*/-1, /*k=*/1, random), "-1");
}

TEST(RandomSortedElementsDeathTest, NTooSmall) {
  absl::BitGen random;
  EXPECT_DEATH(RandomSortedElements(/*n=*/0, /*k=*/1, random), "k <= n");
}

TEST(RandomSortedElementsDeathTest, NTooSmall2) {
  absl::BitGen random;
  EXPECT_DEATH(RandomSortedElements(/*n=*/3, /*k=*/4, random), "k <= n");
}

TEST(RandomSortedElementsDeathTest, NegativeK) {
  absl::BitGen random;
  EXPECT_DEATH(RandomSortedElements(/*n=*/10, /*k=*/-1, random), "-1");
}

TEST(RandomSortedElementsDeathTest, NegativeKAllowRepetition) {
  absl::BitGen random;
  EXPECT_DEATH(RandomSortedElements(/*n=*/10, /*k=*/-1, random,
                                    /*allow_repetition=*/true),
               "-1");
}

TEST(RandomValuesSpanningDeathTest, LessThanTwoValues) {
  absl::BitGen random;
  EXPECT_DEATH(RandomValuesSpanning(0, 10, /*num_values=*/1, random,
                                    /*allow_repetition=*/false),
               "1");
  EXPECT_DEATH(RandomValuesSpanning(0, 10, /*num_values=*/1, random,
                                    /*allow_repetition=*/true),
               "1");
}

TEST(RandomSplitTest, SimpleCase) {
  absl::BitGen random;
  const std::vector<int> split = RandomSplit(100, 3, random);
  ASSERT_EQ(split.size(), 3);
  EXPECT_EQ(split[0] + split[1] + split[2], 100);
}

TEST(RandomSplitTest, Singletons) {
  absl::BitGen random;
  EXPECT_THAT(RandomSplit(1, 1, random), ElementsAre(1));
  EXPECT_THAT(RandomSplit<char>(0, 1, random), ElementsAre(0));
  EXPECT_THAT(RandomSplit<uint64_t>(10'000'000'000'000'000'000U, 1, random),
              ElementsAre(10'000'000'000'000'000'000U));
  const absl::uint128 kLarge = absl::uint128{1} << 127;
  EXPECT_THAT(RandomSplit<absl::uint128>(kLarge, 1, random),
              ElementsAre(kLarge));
}

TEST(RandomSplitTest, SplitZero) {
  absl::BitGen random;
  EXPECT_THAT(RandomSplit(0, 5, random), ElementsAre(0, 0, 0, 0, 0));
}

TEST(RandomSplitTest, SplitEmpty) {
  absl::BitGen random;
  EXPECT_THAT(RandomSplit(5, 0, random), IsEmpty());
}

TEST(RandomSplitTest, SplitEmptyAndZero) {
  absl::BitGen random;
  EXPECT_THAT(RandomSplit(0, 0, random), IsEmpty());
}

TEST(RandomSplitTest, RandomizedStressTest) {
  absl::BitGen random;
  const std::vector<int> split = RandomSplit(100, 3, random);
  ASSERT_EQ(split.size(), 3);
  EXPECT_EQ(split[0] + split[1] + split[2], 100);
}

}  // namespace
}  // namespace operations_research
