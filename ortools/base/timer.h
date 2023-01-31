// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_BASE_TIMER_H_
#define OR_TOOLS_BASE_TIMER_H_

#include "absl/log/check.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"

class WallTimer {
 public:
  WallTimer() { Reset(); }
  void Reset() {
    running_ = false;
    sum_ = 0;
  }
  // When Start() is called multiple times, only the most recent is used.
  void Start() {
    running_ = true;
    start_ = absl::GetCurrentTimeNanos();
  }
  void Restart() {
    sum_ = 0;
    Start();
  }
  void Stop() {
    if (running_) {
      sum_ += absl::GetCurrentTimeNanos() - start_;
      running_ = false;
    }
  }
  double Get() const { return GetNanos() * 1e-9; }
  int64_t GetInMs() const { return GetNanos() / 1000000; }
  int64_t GetInUsec() const { return GetNanos() / 1000; }
  inline absl::Duration GetDuration() const {
    return absl::Nanoseconds(GetNanos());
  }
  bool IsRunning() const { return running_; }

 protected:
  int64_t GetNanos() const {
    return running_ ? absl::GetCurrentTimeNanos() - start_ + sum_ : sum_;
  }

 private:
  bool running_;
  int64_t start_;
  int64_t sum_;
};

// This is meant to measure the actual CPU usage time.
// TODO(user): implement it properly.
typedef WallTimer UserTimer;

// This is meant to be a ultra-fast interface to the hardware cycle counter,
// without periodic recalibration, to be even faster than
// absl::GetCurrentTimeNanos().
// But this current implementation just uses GetCurrentTimeNanos().
// TODO(user): implement it.
class CycleTimer : public WallTimer {
 public:
  // This actually returns a number of nanoseconds instead of the number
  // of CPU cycles.
  int64_t GetCycles() const { return GetNanos(); }
};

typedef CycleTimer SimpleCycleTimer;

// Conversion routines between CycleTimer::GetCycles and actual times.
class CycleTimerBase {
 public:
  static int64_t SecondsToCycles(double s) {
    return static_cast<int64_t>(s * 1e9);
  }
  static double CyclesToSeconds(int64_t c) { return c * 1e-9; }
  static int64_t CyclesToMs(int64_t c) { return c / 1000000; }
  static int64_t CyclesToUsec(int64_t c) { return c / 1000; }
};
typedef CycleTimerBase CycleTimerInstance;

// A WallTimer clone meant to support SetClock(), for unit testing. But for now
// we just use WallTimer directly.
typedef WallTimer ClockTimer;

class ScopedWallTime {
 public:
  // We do not own the pointer. The pointer must be valid for the duration
  // of the existence of the ScopedWallTime instance. Not thread safe for
  // aggregate_time.
  explicit ScopedWallTime(double* aggregate_time);
  ~ScopedWallTime();

 private:
  double* aggregate_time_;

  // When the instance was created.
  WallTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWallTime);
};
#endif  // OR_TOOLS_BASE_TIMER_H_
