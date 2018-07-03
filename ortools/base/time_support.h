// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_BASE_TIME_SUPPORT_H_
#define OR_TOOLS_BASE_TIME_SUPPORT_H_

#include "ortools/base/integral_types.h"

namespace absl {

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
int64 GetCurrentTimeNanos();

inline double WallTime_Now() { return GetCurrentTimeNanos() * 1e-9; }

typedef double Duration;
typedef double Time;

inline Time Now() { return WallTime_Now(); }

inline Duration Seconds(double x) { return x; }
inline Duration Milliseconds(double x) { return x * 1e-3; }
inline double ToDoubleSeconds(Duration x) { return x; }
inline int64 ToInt64Milliseconds(Duration x) { return x * 1e3; }
inline Duration ZeroDuration() { return Duration(0); }

}  // namespace absl

// Temporary support for the legacy "base::" namespace
namespace base {
using absl::Duration;             // NOLINT(readability/namespace)
using absl::Milliseconds;         // NOLINT(readability/namespace)
using absl::Seconds;              // NOLINT(readability/namespace)
using absl::Time;                 // NOLINT(readability/namespace)
using absl::ToDoubleSeconds;      // NOLINT(readability/namespace)
using absl::ToInt64Milliseconds;  // NOLINT(readability/namespace)
using absl::ZeroDuration;         // NOLINT(readability/namespace)
}  // namespace base

inline double ToWallTime(int64 nanos) { return 1e-9 * nanos; }

#endif  // OR_TOOLS_BASE_TIME_SUPPORT_H_
