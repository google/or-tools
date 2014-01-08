// Copyright 2010-2013 Google
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

#if !defined(_MSC_VER)
#include <sys/time.h>
#else
#include <windows.h>
#endif
#if defined(__APPLE__) && defined(__GNUC__)
#include <mach/mach_time.h>
#endif
#include <time.h>
#include <ctime>
#include "base/logging.h"
#include "base/timer.h"

namespace operations_research {

// Walltimer

#if !defined(_MSC_VER)
namespace {
static inline int64 TimevalToUsec(const timeval& tv) {
  return static_cast<int64>(1000000) * tv.tv_sec + tv.tv_usec;
}
}  // namespace
#endif

WallTimer::WallTimer()
    : start_usec_(0LL), sum_usec_(0LL), has_started_(false) {}

void WallTimer::Start() {  // Just save when we started
#if defined(_MSC_VER)
  start_usec_ = clock();
#elif defined(__GNUC__)
  struct timeval tv;
  gettimeofday(&tv, NULL);
  start_usec_ = TimevalToUsec(tv);
#endif
  has_started_ = true;
}

void WallTimer::Stop() {  // Update total time, 1st time it's called
  if (has_started_) {     // so two Stop()s is safe
#if defined(_MSC_VER)
    sum_usec_ += clock() - start_usec_;
#elif defined(__GNUC__)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    sum_usec_ += TimevalToUsec(tv) - start_usec_;
#endif
    has_started_ = false;
  }
}

bool WallTimer::Reset() {  // As if we had hit Stop() first
  start_usec_ = 0;
  sum_usec_ = 0;
  has_started_ = false;
  return true;
}

void WallTimer::Restart() {
  Reset();
  Start();
}

bool WallTimer::IsRunning() const { return has_started_; }

int64 WallTimer::GetInMs() const {
#if defined(_MSC_VER)
  int64 local_sum_usec = sum_usec_;
  if (has_started_) {  // need to include current time too
    local_sum_usec += clock() - start_usec_;
  }
  return local_sum_usec;
#elif defined(__GNUC__)
  static const int64 kMilliSecInMicroSec = 1000LL;
  int64 local_sum_usec = sum_usec_;
  if (has_started_) {  // need to include current time too
    struct timeval tv;
    gettimeofday(&tv, NULL);
    local_sum_usec += TimevalToUsec(tv) - start_usec_;
  }
  return local_sum_usec / kMilliSecInMicroSec;
#endif
}

int64 WallTimer::GetTimeInMicroSeconds() {
#if defined(_MSC_VER)
  static const int64 kSecInMicroSec = 1000000LL;
  LARGE_INTEGER now;
  LARGE_INTEGER freq;

  QueryPerformanceCounter(&now);
  QueryPerformanceFrequency(&freq);
  return now.QuadPart * kSecInMicroSec / freq.QuadPart;
#elif defined(__APPLE__) && defined(__GNUC__)
  const int64 kMicroSecondsInNanoSeconds = 1000;
  int64 time = mach_absolute_time();
  mach_timebase_info_data_t info;
  kern_return_t err = mach_timebase_info(&info);
  if (err == 0) {
    return time / kMicroSecondsInNanoSeconds * info.numer / info.denom;
  } else {
    return 0;
  }
#elif defined(__GNUC__)  // Linux
  const int64 kSecondInMicroSeconds = 1000000;
  const int64 kMicroSecondsInNanoSeconds = 1000;
  struct timespec current;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &current);
  return current.tv_sec * kSecondInMicroSeconds +
         current.tv_nsec / kMicroSecondsInNanoSeconds;
#endif
}

double WallTimer::Get() const { return GetInMs() / 1000.0; }
// ----- CycleTimer -----

CycleTimer::CycleTimer() : time_in_us_(0), state_(INIT) {}

void CycleTimer::Start() {
  DCHECK_EQ(INIT, state_);
  state_ = STARTED;
  time_in_us_ = WallTimer::GetTimeInMicroSeconds();
}

void CycleTimer::Restart() {
  Reset();
  Start();
}

void CycleTimer::Stop() {
  DCHECK_EQ(STARTED, state_);
  state_ = STOPPED;
  time_in_us_ = WallTimer::GetTimeInMicroSeconds() - time_in_us_;
}

void CycleTimer::Reset() {
  state_ = INIT;
  time_in_us_ = 0;
}

inline int64 CycleTimer::GetCycles() const {
  return CycleTimerBase::UsecToCycles(GetInUsec());
}

int64 CycleTimer::GetInUsec() const {
  DCHECK_EQ(STOPPED, state_);
  return time_in_us_;
}

int64 CycleTimer::GetInMs() const {
  const int64 kMsInUsec = 1000;
  return GetInUsec() / kMsInUsec;
}

ScopedWallTime::ScopedWallTime(double* aggregate_time)
    : aggregate_time_(aggregate_time), timer_() {
  DCHECK(aggregate_time != NULL);
  timer_.Start();
}

ScopedWallTime::~ScopedWallTime() {
  timer_.Stop();
  *aggregate_time_ += timer_.Get();
}

}  // namespace operations_research
