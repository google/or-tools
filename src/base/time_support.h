#ifndef OR_TOOLS_BASE_TIME_SUPPORT_H_
#define OR_TOOLS_BASE_TIME_SUPPORT_H_

#include "base/integral_types.h"

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
}


#endif  // OR_TOOLS_BASE_TIME_SUPPORT_H_
