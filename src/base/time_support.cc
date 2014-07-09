// Copyright 2010-2014 Google
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

#include "base/time_support.h"

#if !defined(_MSC_VER)
#include <sys/time.h>
#else
#include <windows.h>
#endif
#if defined(__APPLE__) && defined(__GNUC__)
#include <mach/mach_time.h>
#endif
#include <ctime>

namespace operations_research {
namespace base {

int64 GetCurrentTimeNanos() {
#if defined(_MSC_VER)
  const int64 kSecInNanoSeconds = 1000000000LL;
  LARGE_INTEGER now;
  LARGE_INTEGER freq;

  QueryPerformanceCounter(&now);
  QueryPerformanceFrequency(&freq);
  return now.QuadPart * kSecInNanoSeconds / freq.QuadPart;
#elif defined(__APPLE__) && defined(__GNUC__)
  int64 time = mach_absolute_time();
  mach_timebase_info_data_t info;
  kern_return_t err = mach_timebase_info(&info);
  return err == 0 ? time * info.numer / info.denom : 0;
#elif defined(__GNUC__)  // Linux
  const int64 kSecondInNanoSeconds = 1000000000;
  struct timespec current;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &current);
  return current.tv_sec * kSecondInNanoSeconds + current.tv_nsec;
#endif
}

}  // namespace base
}  // namespace operations_research
