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

#include "ortools/sat/deterministic_time.h"

#ifdef OR_TOOLS_SAT_DETERMINISTIC_TIME_PROFILING
#include <cstdint>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/debugging/stacktrace.h"

#endif  // OR_TOOLS_SAT_DETERMINISTIC_TIME_PROFILING

namespace operations_research::sat {

#ifdef OR_TOOLS_SAT_DETERMINISTIC_TIME_PROFILING

namespace profiling {
std::vector<void*> GetTimerStackTrace() {
  constexpr int kMaxDepth = 50;
  std::vector<void*> result(kMaxDepth);
  int depth = absl::GetStackTrace(result.data(), kMaxDepth, /*skip_count=*/2);
  for (int i = 0; i < depth; ++i) {
    // This seems necessary to get the exact line number of the deterministic
    // timer constructor in the profile (otherwise we get the line of the next
    // statement, possibly after some local variable declarations).
    result[i] =
        reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(result[i]) - 1);
  }
  result.resize(depth);
  return result;
}

}  // namespace profiling

AllDeterministicTimeStats::~AllDeterministicTimeStats() {
  for (const auto& [source_location, stats] : all_stats_) {
    const auto& [file, line] = source_location;
    std::cout << "dtime stats: " << file << "," << line << ","
              << stats.ToString() << std::endl;
  }
  for (const auto& [source_location, stats] : all_stats2_) {
    const auto& [file, line] = source_location;
    std::cout << "dtime2 stats: " << file << "," << line << ","
              << stats.ToString() << std::endl;
  }
}

thread_local std::stack<AbstractDeterministicTimer*>
    AbstractDeterministicTimer::timers_stack_;

#endif  // OR_TOOLS_SAT_DETERMINISTIC_TIME_PROFILING

}  // namespace operations_research::sat
