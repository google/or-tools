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

#include "ortools/util/time_limit.h"

#include <algorithm>
#include <atomic>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/macros.h"
#include "absl/flags/flag.h"
#include "absl/log/log_streamer.h"
#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/threadpool.h"
#include "ortools/util/testing_utils.h"

namespace operations_research {
namespace {

using ::testing::MatchesRegex;
using ::testing::UnorderedElementsAre;

const double kInfinity = std::numeric_limits<double>::infinity();
constexpr int kTimeMultiplier =
    (DEBUG_MODE ? 5 : 1) * (kAnyXsanEnabled ? 2 : 1);

TEST(TimeLimitTest, NoLimit) {
  TimeLimit time_limit(kInfinity);
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kRunning);
  }
  EXPECT_EQ(kInfinity, time_limit.GetTimeLeft());
}

TEST(TimeLimitTest, NoLimitSecondaryBoolean) {
  TimeLimit time_limit(kInfinity);
  std::atomic<bool> stop = false;
  time_limit.RegisterSecondaryExternalBooleanAsLimit(&stop);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kRunning);
  stop.store(true);
  EXPECT_EQ(time_limit.CurrentState(),
            TimeLimit::State::kSecondaryExternalBoolean);
}

TEST(TimeLimitTest, ZeroLimit) {
  TimeLimit time_limit(0.0, 0.0);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kDeterministicTime);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kDeterministicTime);
  EXPECT_EQ(0.0, time_limit.GetTimeLeft());
  EXPECT_EQ(0.0, time_limit.GetDeterministicTimeLeft());
}

TEST(TimeLimitTest, NegativeLimit) {
  TimeLimit time_limit(-100);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kTime);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kTime);
  EXPECT_EQ(0.0, time_limit.GetTimeLeft());
}

TEST(TimeLimitTest, NegativeInfiniteLimit) {
  TimeLimit time_limit(-kInfinity);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kTime);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kTime);
  EXPECT_EQ(0.0, time_limit.GetTimeLeft());
}

TEST(TimeLimitTest, SmallestLimit) {
  TimeLimit time_limit(1e-6);
  // We are defensive and return that the limit is reached if less than 0.1ms is
  // left.
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kTime);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kTime);
  // Even though the limit is reached, some time is left because we are
  // defensive in the limit enforcement.
  EXPECT_GT(1e-7, time_limit.GetTimeLeft());
}

TEST(TimeLimitTest, SmallLimit) {
  // FLAKINESS: This test is hard to de-flake, so we simply try a few times.
  constexpr int kLastAttempt = 5;
  for (int attempt = 0;; ++attempt) {
    TimeLimit time_limit(1e-3 * kTimeMultiplier);
    if (time_limit.LimitReached()) {
      ASSERT_LT(attempt, kLastAttempt);
      continue;
    }
    double sum = 0.0;
    for (int i = 0; i < 100; ++i) {
      if (time_limit.LimitReached()) break;
      sum += i * i;
    }
    if (time_limit.LimitReached() || time_limit.GetTimeLeft() < 0) {
      ASSERT_LT(attempt, kLastAttempt);
      continue;
    }
    // The rest of the test below isn't expected to be flaky.
    while (true) {
      if (time_limit.LimitReached()) break;
      sum += 123456789.0;
    }
    EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kTime);
    EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kTime);
    EXPECT_GE(sum, 0.0);
    break;
  }
}

TEST(TimeLimitTest, TimeSeemsRight) {
  for (bool usertime_flag : {false, true}) {
    SCOPED_TRACE(absl::StrFormat("usertime_flag: %v", usertime_flag));
    // UserTimer is not implemented yet
    if (usertime_flag) continue;

    const absl::Duration kSleepInterval =
        absl::Milliseconds(10) * kTimeMultiplier;
    constexpr int kSafetyBufferIterations = 5;
    constexpr int kNumIterations = 10;
    const absl::Duration kTimeLimit = kSleepInterval * kNumIterations;
    // FLAKINESS: This test is hard to de-flake, so we simply re-try many times.
    constexpr int kLastAttempt = 40;
    for (int attempt = 0;; ++attempt) {
      TimeLimit time_limit(absl::ToDoubleSeconds(kTimeLimit));
      EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kRunning);
      int limit_reached_at = -1;
      for (int i = 0; i < kNumIterations - kSafetyBufferIterations; ++i) {
        absl::SleepFor(kSleepInterval);
        if (time_limit.LimitReached()) {
          limit_reached_at = i;
          break;
        }
      }
      if (limit_reached_at >= 0) {
        ASSERT_LT(attempt, kLastAttempt)
            << "Limit reached too early -- even after " << kLastAttempt + 1
            << " attempt. i=" << limit_reached_at;
        continue;  // Start a new attempt.
      }

      for (int i = 0; i < kSafetyBufferIterations; ++i) {
        absl::SleepFor(kSleepInterval);
      }
      if (absl::GetFlag(FLAGS_time_limit_use_usertime)) {
        // Sleep doesn't count towards the usertime!
        if (time_limit.LimitReached() ||
            absl::Seconds(time_limit.GetTimeLeft()) <
                kTimeLimit - absl::Milliseconds(50) * kTimeMultiplier) {
          ASSERT_LT(attempt, kLastAttempt);
          continue;
        }
      } else {
        // This part isn't flaky at all.
        ASSERT_EQ(time_limit.CurrentState(), TimeLimit::State::kTime);
      }
      break;  // We've succeeded! No need to re-attempt.
    }
  }
}

TEST(TimeLimitTest, NegativeDeterministicLimit) {
  TimeLimit time_limit(kInfinity, -100);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kDeterministicTime);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kDeterministicTime);
  EXPECT_EQ(kInfinity, time_limit.GetTimeLeft());
  EXPECT_EQ(0.0, time_limit.GetDeterministicTimeLeft());

  TimeLimit both_time_limit(-100, -100);
  EXPECT_EQ(both_time_limit.CurrentState(),
            TimeLimit::State::kDeterministicTime);
  EXPECT_EQ(both_time_limit.CurrentState(),
            TimeLimit::State::kDeterministicTime);
  EXPECT_EQ(0.0, both_time_limit.GetTimeLeft());
  EXPECT_EQ(0.0, both_time_limit.GetDeterministicTimeLeft());
}

TEST(TimeLimitTest, DeterministicTimeOnly) {
  const double kDeterministicLimit = 10.0;
  TimeLimit time_limit(kInfinity, kDeterministicLimit);
  // Consume all the deterministic time.
  for (int i = 0; i < kDeterministicLimit; ++i) {
    EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kRunning);
    EXPECT_EQ(kInfinity, time_limit.GetTimeLeft());
    EXPECT_EQ(kDeterministicLimit - i, time_limit.GetDeterministicTimeLeft());
    EXPECT_EQ(i, time_limit.GetElapsedDeterministicTime());
    time_limit.AdvanceDeterministicTime(1.0);
  }
  // The limit should be reached.
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kDeterministicTime);
  EXPECT_EQ(kInfinity, time_limit.GetTimeLeft());
  EXPECT_EQ(0.0, time_limit.GetDeterministicTimeLeft());

  // Consuming one more deterministic time.
  time_limit.AdvanceDeterministicTime(1.0);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kDeterministicTime);
  EXPECT_EQ(kInfinity, time_limit.GetTimeLeft());
  EXPECT_EQ(0.0, time_limit.GetDeterministicTimeLeft());
}

TEST(TimeLimitTest, AdvanceDeterministicTimeWithCounterName) {
  static const char kCounterFoo[] = "DoingFoo";
  static const char kCounterBar[] = "DoingBar";
  static const double kDeterministicStepFoo = 1.0;
  static const double kDeterministicStepBar = 2.0;
  static const int kNumSteps = 10;
  static const double kExpectedElapseDeterministicTime = 30.0;
  TimeLimit time_limit(kInfinity, kInfinity);
  for (int i = 0; i < kNumSteps; ++i) {
    time_limit.AdvanceDeterministicTime(kDeterministicStepFoo, kCounterFoo);
    time_limit.AdvanceDeterministicTime(kDeterministicStepBar, kCounterBar);
  }
  EXPECT_NEAR(kExpectedElapseDeterministicTime,
              time_limit.GetElapsedDeterministicTime(), 1e-10);
  const std::vector<std::string> debug_string_lines =
      absl::StrSplit(time_limit.DebugString(), '\n');
#ifdef NDEBUG
  EXPECT_THAT(
      debug_string_lines,
      UnorderedElementsAre("Time left: inf", "Deterministic time left: inf",
                           MatchesRegex("Elapsed time: [-0-9.e]*"),
                           "Elapsed deterministic time: 30"));
#else
  EXPECT_THAT(
      debug_string_lines,
      UnorderedElementsAre("Time left: inf", "Deterministic time left: inf",
                           MatchesRegex("Elapsed time: [-0-9.e]*"),
                           "Elapsed deterministic time: 30", "DoingFoo: 10",
                           "DoingBar: 20"));
#endif
}

TEST(TimeLimitTest, ElapsedTimeReachedFirst) {
  const double kLimit = 0.01;  // 10ms
  const double kUnitTime = 0.001;
  TimeLimit time_limit(kLimit, kLimit);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kRunning);

  // Sleeps 100ms.
  absl::SleepFor(absl::Milliseconds(100));
  time_limit.AdvanceDeterministicTime(kUnitTime);
  EXPECT_GE(0.0, time_limit.GetTimeLeft());
  EXPECT_EQ(kLimit - kUnitTime, time_limit.GetDeterministicTimeLeft());
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kTime);
  EXPECT_EQ(0.0, time_limit.GetTimeLeft());
  EXPECT_EQ(kLimit - kUnitTime, time_limit.GetDeterministicTimeLeft());
}

TEST(TimeLimitTest, DeterniministicTimeReachedFirst) {
  const double kLimit = 0.2 * kTimeMultiplier;  // 200ms in opt.
  TimeLimit time_limit(kLimit, kLimit);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kRunning);

  absl::SleepFor(absl::Milliseconds(1) * kTimeMultiplier);
  time_limit.AdvanceDeterministicTime(1.0 * kTimeMultiplier);
  EXPECT_LT(0.0, time_limit.GetTimeLeft());
  EXPECT_EQ(0.0, time_limit.GetDeterministicTimeLeft());
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kDeterministicTime);
  EXPECT_LT(0.0, time_limit.GetTimeLeft());
  EXPECT_EQ(0.0, time_limit.GetDeterministicTimeLeft());
}

TEST(TimeLimitTest, ExternalLimitReached) {
  std::atomic<bool> external_boolean_as_limit(false);
  TimeLimit time_limit(kInfinity, kInfinity);
  time_limit.RegisterExternalBooleanAsLimit(&external_boolean_as_limit);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kRunning);

  absl::SleepFor(absl::Milliseconds(10));
  time_limit.AdvanceDeterministicTime(1.0);
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kRunning);

  external_boolean_as_limit = true;
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kExternalBoolean);

  external_boolean_as_limit = false;
  EXPECT_EQ(time_limit.CurrentState(), TimeLimit::State::kRunning);
}

TEST(TimeLimitTest, Infinite) {
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  EXPECT_EQ(kInfinity, time_limit->GetTimeLeft());
  EXPECT_EQ(kInfinity, time_limit->GetDeterministicTimeLeft());
}

TEST(TimeLimitTest, FromDeterministicTime) {
  const double kDeterministicTimeLimit = 2.0;
  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromDeterministicTime(kDeterministicTimeLimit);
  EXPECT_EQ(kInfinity, time_limit->GetTimeLeft());
  EXPECT_EQ(kDeterministicTimeLimit, time_limit->GetDeterministicTimeLeft());
}

class TestParameters {
 public:
  TestParameters(double max_time_in_seconds, double max_deterministic_time)
      : max_time_in_seconds_(max_time_in_seconds),
        max_deterministic_time_(max_deterministic_time) {}

  double max_time_in_seconds() const { return max_time_in_seconds_; }
  double max_deterministic_time() const { return max_deterministic_time_; }

 private:
  double max_time_in_seconds_;
  double max_deterministic_time_;
};

TEST(TimeLimitTest, FromParameters) {
  const double kTimeLimitSeconds = 1.0;
  const double kDeterministicTimeLimit = 2.0;
  TestParameters parameters(kTimeLimitSeconds, kDeterministicTimeLimit);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::FromParameters(parameters);
  EXPECT_NEAR(kTimeLimitSeconds, time_limit->GetTimeLeft(),
              1e-3 * kTimeMultiplier);
  EXPECT_EQ(kDeterministicTimeLimit, time_limit->GetDeterministicTimeLeft());
}

TEST(TimeLimitTest, ResetLimitFromParameters) {
  TestParameters parameters(kInfinity, 1.0);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::FromParameters(parameters);
  EXPECT_EQ(time_limit->CurrentState(), TimeLimit::State::kRunning);
  time_limit->AdvanceDeterministicTime(2.0);
  EXPECT_EQ(time_limit->CurrentState(), TimeLimit::State::kDeterministicTime);
  EXPECT_EQ(time_limit->GetElapsedDeterministicTime(), 2.0);
  EXPECT_EQ(time_limit->GetDeterministicTimeLeft(), 0.0);

  time_limit->ResetLimitFromParameters(parameters);
  EXPECT_EQ(time_limit->CurrentState(), TimeLimit::State::kRunning);
  EXPECT_EQ(time_limit->GetElapsedDeterministicTime(), 0.0);
  EXPECT_EQ(time_limit->GetDeterministicTimeLeft(), 1.0);

  time_limit->AdvanceDeterministicTime(2.0);
  EXPECT_EQ(time_limit->CurrentState(), TimeLimit::State::kDeterministicTime);
}

TEST(TimeLimitTest, ResetDeterministicTimeLimit) {
  TimeLimit limit(/*limit_in_seconds=*/1.0, /*deterministic_limit=*/1.0);
  EXPECT_EQ(limit.CurrentState(), TimeLimit::State::kRunning);
  limit.AdvanceDeterministicTime(2.0);
  EXPECT_EQ(limit.CurrentState(), TimeLimit::State::kDeterministicTime);
  limit.ChangeDeterministicLimit(4.0);
  EXPECT_EQ(limit.CurrentState(), TimeLimit::State::kRunning);
  EXPECT_EQ(limit.GetElapsedDeterministicTime(), 2.0);
  EXPECT_EQ(limit.GetDeterministicTimeLeft(), 2.0);
  EXPECT_EQ(limit.GetDeterministicLimit(), 4.0);
}

TEST(SharedTimeLimitTest, IsThreadSafe) {
  absl::BitGen gen;

  const int num_workers = 100;
  const int num_repeats = 10;

  int total_time = 0;
  TimeLimit time_limit;
  SharedTimeLimit shared_time_limit(&time_limit);
  {
    ThreadPool pool("SharedTimeLimitTest", num_workers);

    for (int i = 0; i < num_workers; ++i) {
      const int deterministic_ticks =
          absl::Uniform(absl::IntervalClosedClosed, gen, 1, 100);
      total_time += num_repeats * deterministic_ticks;
      pool.Schedule([&shared_time_limit, deterministic_ticks]() {
        for (int j = 0; j < num_repeats; ++j) {
          shared_time_limit.AdvanceDeterministicTime(deterministic_ticks);
          absl::SleepFor(absl::Milliseconds(deterministic_ticks));
        }
      });
    }
  }
  EXPECT_EQ(total_time, shared_time_limit.GetElapsedDeterministicTime());
}

TEST(TimeLimitCheckEveryNCallsTest, WaitNBeforeFirstCheck) {
  TimeLimit limit;
  std::atomic<bool> stop = false;
  limit.RegisterExternalBooleanAsLimit(&stop);
  TimeLimitCheckEveryNCalls checker(10, &limit);
  stop = true;

  for (int i = 0; i < 10; ++i) {
    EXPECT_FALSE(checker.LimitReached());
  }
  EXPECT_TRUE(checker.LimitReached());
}

TEST(TimeLimitCheckEveryNCallsTest, StickyTrue) {
  TimeLimit limit;
  std::atomic<bool> stop = false;
  limit.RegisterExternalBooleanAsLimit(&stop);
  TimeLimitCheckEveryNCalls checker(10, &limit);
  EXPECT_FALSE(checker.LimitReached());
  stop = true;

  for (int i = 0; i < 9; ++i) {
    EXPECT_FALSE(checker.LimitReached());
  }

  // Stays true forwever.
  EXPECT_TRUE(checker.LimitReached());
  EXPECT_TRUE(checker.LimitReached());
  EXPECT_TRUE(checker.LimitReached());
}

}  // namespace
}  // namespace operations_research
