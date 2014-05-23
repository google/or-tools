#ifndef OR_TOOLS_BASE_TIME_SUPPORT_H_
#define OR_TOOLS_BASE_TIME_SUPPORT_H_

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

// Ideally, this should be a super-fast implementation that isn't too
// unreliable:
// - It shouldn't take more than a few nanoseconds per call, on average.
// - It shouldn't be out of sync with the "real" time by more than a few
//   milliseconds.
// It might be non-increasing though. This will depend on the implementation.
//
// The current implementation (below) is probably slower, and maybe not as
// reliable as that.
// TODO(user): implement it properly.
// One possible way to achieve this is to use the hardware cycle counter,
// coupled with automatic, periodic recalibration with the (reliable, but slow)
// system timer to avoid drifts. See
// http://en.wikipedia.org/wiki/Time_Stamp_Counter#Use.
namespace base {
int64 GetCurrentTimeNanos();
}  // namespace base

inline double WallTime_Now() { return base::GetCurrentTimeNanos() * 1e-9; }

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

#endif  // OR_TOOLS_BASE_TIME_SUPPORT_H_
