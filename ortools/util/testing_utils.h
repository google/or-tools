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

#ifndef OR_TOOLS_UTIL_TESTING_UTILS_H_
#define OR_TOOLS_UTIL_TESTING_UTILS_H_

namespace operations_research {

inline constexpr bool kAsanEnabled = false;
inline constexpr bool kMsanEnabled = false;
inline constexpr bool kTsanEnabled = false;
inline bool ProbablyRunningInsideUnitTest() { return false; }

inline constexpr bool kAnyXsanEnabled =
    kAsanEnabled || kMsanEnabled || kTsanEnabled;

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_TESTING_UTILS_H_
