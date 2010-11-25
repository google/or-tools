// Copyright 2010 Google
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
#endif
#include <time.h>
#include "base/timer.h"

namespace operations_research {

// Walltimer

#if !defined(_MSC_VER)
namespace {
static inline int64 TimevalToUsec(const timeval &tv) {
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

void WallTimer::Stop() {   // Update total time, 1st time it's called
  if ( has_started_ ) {           // so two Stop()s is safe
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

bool WallTimer::IsRunning() const {
  return has_started_;
}

int64 WallTimer::GetInMs() const {
#if defined(_MSC_VER)
  int64 local_sum_usec = sum_usec_;
  if ( has_started_ ) {           // need to include current time too
    local_sum_usec += clock() - start_usec_;
  }
  return local_sum_usec;
#elif defined(__GNUC__)
  static const int64 kMilliSecInMicroSec = 1000LL;
  int64 local_sum_usec = sum_usec_;
  if ( has_started_ ) {           // need to include current time too
    struct timeval tv;
    gettimeofday(&tv, NULL);
    local_sum_usec += TimevalToUsec(tv) - start_usec_;
  }
  return local_sum_usec / kMilliSecInMicroSec;
#endif
}

double WallTimer::Get() const {
  return GetInMs() / 1000.0;
}
}  // namespace operations_research
