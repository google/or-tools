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

#include "base/util.h"
#include "base/stringpiece.h"

namespace operations_research {

// Walltimer

#if !defined(_MSC_VER)
namespace {
static inline int64 TimevalToUsec(const timeval &tv) {
  return static_cast<int64>(1000000) * tv.tv_sec + tv.tv_usec;
}
}  // namespace
#endif

WallTimer::WallTimer() :
    start_usec_(0LL), sum_usec_(0LL), has_started_(false) {}

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
  Reset(); Start();
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

// GetUsedMemory

#if defined(__APPLE__) && defined(__GNUC__)
#include <mach/mach_init.h>
#include <mach/task.h>

int64 GetMemoryUsage () {
  task_t task = MACH_PORT_NULL;
  struct task_basic_info t_info;
  mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

  if (KERN_SUCCESS != task_info(mach_task_self(),
                                TASK_BASIC_INFO,
                                (task_info_t)&t_info,
                                &t_info_count)) {
    return -1;
  }
  int64 resident_memory = t_info.resident_size;
  //  int64 virtual_memory = t_info.virtual_size;
  return resident_memory;
}
#else
int64 GetMemoryUsage() {
  return 0;
}
#endif

}  // namespace operations_research
