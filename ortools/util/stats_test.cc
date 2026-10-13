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

#include <cmath>
#include <limits>
#include <string>
#include <type_traits>

#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/util/fp_utils_testing.h"
#include "ortools/util/testing_utils.h"

// We need to do this *before* including util/stats.h, so that the
// SCOPED_TIME_STAT() macro does the actual tracking of stats.
#ifndef OR_STATS
#define OR_STATS
#endif  // OR_STATS
#include "ortools/util/stats.h"

namespace operations_research {
namespace {

// Make sure that the disabled stats classes are trivially destructible and
// empty, so that the compiler can fully remove them when placed in the stack.
static_assert(std::is_trivially_destructible_v<DisabledScopedTimeStats>);
static_assert(std::is_empty_v<DisabledScopedTimeStats>);
static_assert(
    std::is_trivially_destructible_v<DisabledScopedTimeDistributionUpdater>);
static_assert(std::is_empty_v<DisabledScopedTimeDistributionUpdater>);

TEST(DistributionStatTest, MathCorrectness) {
  IntegerDistribution stat("test");
  const double n = 1 << 20;
  for (int i = 0; i < n; ++i) {
    stat.Add(i);
  }
  EXPECT_EQ(n, stat.Num());
  EXPECT_EQ(0.0, stat.Min());
  EXPECT_EQ(static_cast<double>((1 << 20) - 1), stat.Max());
  EXPECT_EQ(n * (n - 1) / 2, stat.Sum());
  EXPECT_EQ((n - 1) / 2, stat.Average());

  // std_deviation = sqrt(avg_square - avg^2), where avg = (n-1)/2 and
  // avg_square = sum_square/n = (n-1)*(2n-1)/6.
  const double exact_std_deviation =
      sqrt((n - 1) * ((2 * n - 1) / 6 - (n - 1) / 4));
  EXPECT_NEAR(exact_std_deviation, stat.StdDeviation(),
              exact_std_deviation * std::numeric_limits<double>::epsilon());
}

TEST(DistributionStatTest, EmptyDistribution) {
  IntegerDistribution stat("test");
  EXPECT_EQ(0, stat.Num());
  EXPECT_EQ(0.0, stat.Min());
  EXPECT_EQ(0.0, stat.Max());
  EXPECT_EQ(0.0, stat.Sum());
  EXPECT_EQ(0.0, stat.Average());
  EXPECT_EQ(0.0, stat.StdDeviation());
}

TEST(DistributionStatTest, Reset) {
  IntegerDistribution stat("test");
  stat.Add(1);
  EXPECT_EQ(1.0, stat.Num());
  EXPECT_EQ(1.0, stat.Min());
  EXPECT_EQ(1.0, stat.Average());
  stat.Reset();
  EXPECT_EQ("test", stat.Name());
  EXPECT_EQ(0, stat.Num());
  EXPECT_EQ(0.0, stat.Min());
  EXPECT_EQ(0.0, stat.Max());
  EXPECT_EQ(0.0, stat.Sum());
  EXPECT_EQ(0.0, stat.Average());
  EXPECT_EQ(0.0, stat.StdDeviation());
}

TEST(DistributionStatTest, PositiveOverflow) {
  DoubleDistribution stat("test");
  for (int i = 0; i < 1000; ++i) {
    stat.Add(1e308);
  }
  EXPECT_EQ(1000, stat.Num());
  EXPECT_EQ(1e308, stat.Min());
  EXPECT_EQ(1e308, stat.Max());
  EXPECT_EQ(std::numeric_limits<double>::infinity(), stat.Sum());
  EXPECT_EQ(std::numeric_limits<double>::infinity(), stat.Average());
  EXPECT_TRUE(std::isnan(stat.StdDeviation()));
  EXPECT_THAT(
      stat.StatString(),
      ::testing::MatchesRegex(
          "test:\\s\\s\\s\\s\\s1000\\s\\[1\\.0e\\+308, "
          "1\\.0e\\+308\\]\\s\\s\\s\\s\\s\\sinf\\s\\s\\s\\s\\s(-|\\s)nan\\n"));
}

TEST(DistributionStatTest, InfinityAndNaN) {
  DoubleDistribution stat("test");
  stat.Add(-std::numeric_limits<double>::infinity());
  stat.Add(50);
  stat.Add(std::numeric_limits<double>::quiet_NaN());
  EXPECT_EQ(3, stat.Num());
  EXPECT_EQ(-std::numeric_limits<double>::infinity(), stat.Min());
  EXPECT_EQ(50, stat.Max());
  EXPECT_TRUE(std::isnan(stat.Sum()));
  EXPECT_TRUE(std::isnan(stat.Average()));
  EXPECT_TRUE(std::isnan(stat.StdDeviation()));
  EXPECT_EQ("test:        3 [    -inf,  5.0e+01]      nan      nan\n",
            stat.StatString());
}

TEST(DistributionStatTest, SquareUnderflow) {
  DoubleDistribution stat("test");
  // Note(user): A naive computation of the standard deviation, eg. avg of
  // squares - square of the average, would underflow, because the sum of
  // squares would be zero (1e-324 underflows to zero).
  for (int i = 0; i < 1000000; ++i) {
    stat.Add(1e-162);
  }
  EXPECT_EQ(1000000, stat.Num());
  EXPECT_EQ(1e-162, stat.Min());
  EXPECT_EQ(1e-162, stat.Max());

  // Note(user): If you are like me, it may surprise you that the sum is not
  // exactly equals to 1e-156 in this case, but this is just because in binary
  // 1e-156 is not just 1 on the mantissa.
  EXPECT_THAT(stat.Sum(), WithinSameAbsoluteOrRelativeTolerance(1e-156, 1e-11));
  EXPECT_THAT(stat.Average(),
              WithinSameAbsoluteOrRelativeTolerance(1e-162, 1e-11));
  EXPECT_EQ(0.0, stat.StdDeviation());
  EXPECT_EQ("test:  1000000 [1.0e-162, 1.0e-162] 1.0e-162  0.0e+00\n",
            stat.StatString());
}

TEST(GroupStatTest, Formatting) {
  TimeDistribution time("A");
  RatioDistribution percent("statB");
  StatsGroup group("group");
  group.Register(&time);
  group.Register(&percent);
  DoubleDistribution accuracy("statC", &group);
  IntegerDistribution number("D", &group);

  // Only distributions with at least one element will be printed.
  percent.Add(0.5);
  number.Add(1);

  // This test the map<string, TimeDistribution*> feature.
  group.LookupOrCreateTimeDistribution("empty_distrib");
  group.LookupOrCreateTimeDistribution("name♫★☺")->AddTimeInSec(1.0);
  group.LookupOrCreateTimeDistribution("alpha♫★☺")->AddTimeInSec(1.0);
  group.LookupOrCreateTimeDistribution("beta")->AddTimeInSec(3.0);

  // Note that timings are sorted first, and by decreasing total time.
  // Also notice the alphabetical ordering of the stats by name, and that
  // unicode characters don't screw up the alignment.
  EXPECT_EQ(
      "group {\n"
      "  beta     :        1 [   3.00s,    3.00s]    3.00s   0.00ns    3.00s\n"
      "  alpha♫★☺ :        1 [   1.00s,    1.00s]    1.00s   0.00ns    1.00s\n"
      "  name♫★☺  :        1 [   1.00s,    1.00s]    1.00s   0.00ns    1.00s\n"
      "  D        :        1 [       1,        1]     1.00     0.00        1\n"
      "  statB    :        1 [  50.00%,   50.00%]   50.00%    0.00%\n"
      "}\n",
      group.StatString());

  // Tests Reset() + print empty group.
  group.Reset();
  EXPECT_EQ("", group.StatString());

  // Tests that the stats are still registered.
  number.Add(1);
  EXPECT_EQ(
      "group {\n"
      "  D :        1 [       1,        1]     1.00     0.00        1\n"
      "}\n",
      group.StatString());
}

TEST(TimeDistributionTest, BasicBehavior) {
#ifdef _WIN32
  // absl::SleepFor() on Windows uses Sleep(), which has 1ms precision. Thus it
  // is unlikely this test could work.
  //
  // https://stackoverflow.com/questions/7827062/is-there-a-windows-equivalent-of-nanosleep
  // has proposition to have short sleeps, but non is easy to deploy.
  GTEST_SKIP() << "No nanosleep() on Win32.";
#endif  // _WIN32
  TimeDistribution stat("timer stat");
  stat.StartTimer();
  absl::SleepFor(absl::Milliseconds(1));
  stat.StopTimerAndAddElapsedTime();
  EXPECT_EQ(1, stat.Num());
  EXPECT_EQ(stat.Min(), stat.Max());
  EXPECT_EQ(stat.Max(), stat.Sum());

  // Exact result is supposed to be 1ms, but I get around 1.060ms on my
  // computer. Note that the timer() is not stopped during context switch, and
  // we assume that this make us lose a bit of time.
  // FLAKINESS: On 2017-02-28, with 1000 runs on forge, the largest time
  // obtained was 15ms, in opt (we got 4ms in fastbuild). So 100ms seems
  // "safe enough" (in ASAN mode, it can be even worse, we saw 250ms happen).
  constexpr double kMaxExpectedSum =
      kAsanEnabled || kHwAsanEnabled ? 0.500 : 0.100;
  EXPECT_GE(TimeDistribution::CyclesToSeconds(stat.Sum()), 0.001);
  EXPECT_LT(TimeDistribution::CyclesToSeconds(stat.Sum()), kMaxExpectedSum);
}

TEST(ScopedTimeDistributionUpdater, BasicBehavior) {
  TimeDistribution distA("distA");
  TimeDistribution distB("distB");
  {
    ScopedTimeDistributionUpdater timer(&distA);
    timer.AlsoUpdate(&distB);
  }
  EXPECT_GT(distA.Num(), 0);
  EXPECT_GE(distA.Sum(), 0.0);
  EXPECT_EQ(distA.Num(), distB.Num());
  EXPECT_EQ(distA.Sum(), distB.Sum());
}

// By skew, we mean the error in the measure due to the time taken by the
// instrumentation code.
// Note(user): The skew on my computer in non-optimized mode is about 25
// cycles in the first case and 100 cycles in the second one. We take the min
// of kMeasurements run to be sure we measure something without any
// context-switch.
TEST(TimeDistributionTest, SkewIsSmall) {
  const int kMeasurements = 1000000;
  TimeDistribution stat("direct interface");
  StatsGroup stats("scoped interface with map<>");
  for (int i = 0; i < kMeasurements; ++i) {
    stat.StartTimer();
    stat.StopTimerAndAddElapsedTime();
    SCOPED_TIME_STAT(&stats);
  }
  EXPECT_EQ(kMeasurements, stat.Num());
  EXPECT_LT(stat.Min(), 200.0);

  TimeDistribution* p_stat = stats.LookupOrCreateTimeDistribution(__FUNCTION__);
  EXPECT_EQ(kMeasurements, p_stat->Num());
  EXPECT_LT(p_stat->Min(), 200.0);
}

TEST(TimeDistributionTest, AbseilDuration) {
  constexpr int kNumValues = 100;
  constexpr double kMaxDuration = 1000.0;
  absl::BitGen random;
  TimeDistribution stat_sec("seconds");
  TimeDistribution stat_absl("abseil");
  for (int i = 0; i < kNumValues; ++i) {
    const absl::Duration duration =
        absl::Seconds(absl::Uniform(random, 0.0, kMaxDuration));
    stat_sec.AddTimeInSec(absl::ToDoubleSeconds(duration));
    stat_absl.AddTime(duration);
  }
  EXPECT_EQ(stat_sec.Sum(), stat_absl.Sum());
  EXPECT_EQ(stat_sec.Min(), stat_absl.Min());
  EXPECT_EQ(stat_sec.Max(), stat_absl.Max());
  EXPECT_EQ(stat_sec.Num(), stat_absl.Num());
  EXPECT_EQ(stat_sec.Average(), stat_absl.Average());
  EXPECT_EQ(stat_sec.StdDeviation(), stat_absl.StdDeviation());
}

TEST(TimeDistributionTest, Formatting) {
  TimeDistribution stat("test");
  stat.AddTimeInSec(0.0);
  stat.AddTimeInSec(0.000000001);
  EXPECT_NE(std::string::npos, stat.ValueAsString().find("1.00ns"));
  stat.AddTimeInSec(15 * 0.000001);
  EXPECT_NE(std::string::npos, stat.ValueAsString().find("15.00us"));
  stat.AddTimeInSec(0.001);
  EXPECT_NE(std::string::npos, stat.ValueAsString().find("1.00ms"));
  stat.AddTimeInSec(12);
  EXPECT_NE(std::string::npos, stat.ValueAsString().find("12.00s"));
  stat.AddTimeInSec(60);
  EXPECT_NE(std::string::npos, stat.ValueAsString().find("1.00m"));
  stat.AddTimeInSec(2 * 60 * 60);
  EXPECT_NE(std::string::npos, stat.ValueAsString().find("2.00h"));
}

TEST(RatioDistributionTest, Formatting) {
  RatioDistribution stat("percent stat");
  stat.Add(0.2);
  stat.Add(0.5);
  stat.Add(0.7);
  EXPECT_EQ(3, stat.Num());
  EXPECT_EQ(1.4, stat.Sum());
  EXPECT_EQ(0.2, stat.Min());
  EXPECT_EQ(0.7, stat.Max());
  EXPECT_EQ("percent stat:        3 [  20.00%,   70.00%]   46.67%   20.55%\n",
            stat.StatString());
}

TEST(DoubleDistributionTest, Formatting) {
  DoubleDistribution stat("a");
  stat.Add(-1);
  stat.Add(0);
  stat.Add(3);
  EXPECT_EQ(3, stat.Num());
  EXPECT_EQ(2, stat.Sum());
  EXPECT_EQ(-1, stat.Min());
  EXPECT_EQ(3, stat.Max());
  EXPECT_EQ("a:        3 [-1.0e+00,  3.0e+00]  6.7e-01  1.7e+00\n",
            stat.StatString());
}

TEST(RatioDistributionDeathTest, IllegalArguments) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skip in opt mode";
  RatioDistribution stat("test");
  EXPECT_DEATH(stat.Add(-1), "Check failed: value >= 0");
}

TEST(TimeDistributionDeathTest, IllegalArguments) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skip in opt mode";
  TimeDistribution stat("test");
  EXPECT_DEATH(stat.AddTimeInSec(-1), "Check failed: seconds >= 0");
  EXPECT_DEATH(stat.AddTimeInCycles(-1), "Check failed: cycles >= 0");
}

#undef OR_STATS
}  // namespace
}  // namespace operations_research
