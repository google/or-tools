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

// ############## Inline implementations below ###############

namespace base {
inline int64 GetCurrentTimeNanos() {
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
