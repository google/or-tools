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

#ifndef ORTOOLS_UTIL_TESTING_UTILS_H_
#define ORTOOLS_UTIL_TESTING_UTILS_H_

#include "ortools/base/log_severity.h"

namespace operations_research {

inline constexpr bool kDebugMode = DEBUG_MODE;

inline constexpr bool kAsanEnabled = false;
inline constexpr bool kHwAsanEnabled = false;
inline constexpr bool kMsanEnabled = false;
inline constexpr bool kTsanEnabled = false;

// DO NOT USE: Exposed here for benchmarking. Use the cached version.
bool UncachedProbablyRunningInsideUnitTest();

// Whether the current binary seems to be running inside a test. Thread-safe.
// PERFORMANCE: In opt, this takes ~100ns the first time, and ~20ns for any
// subsequent call.
bool ProbablyRunningInsideUnitTest();

// Returns true if the program is currently enabling code coverage.
bool HACK_CodeCoverageIsEnabled();

inline constexpr bool kAnyXsanEnabled =
    kAsanEnabled || kMsanEnabled || kTsanEnabled || kHwAsanEnabled;

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_TESTING_UTILS_H_
