// Copyright 2010-2021 Google LLC
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

#include "ortools/util/time_limit.h"

#include "absl/strings/str_cat.h"

ABSL_FLAG(bool, time_limit_use_usertime, false,
          "If true, rely on the user time in the TimeLimit class. This is "
          "only recommended for benchmarking on a non-isolated environment.");

ABSL_FLAG(bool, time_limit_use_instruction_count, false,
          "If true, measures the number of instructions executed");

namespace operations_research {

// static constants.
const double TimeLimit::kSafetyBufferSeconds = 1e-4;
const int TimeLimit::kHistorySize = 100;

std::string TimeLimit::DebugString() const {
  std::string buffer = absl::StrCat(
      "Time left: ", (GetTimeLeft()),
      "\nDeterministic time left: ", (GetDeterministicTimeLeft()),
      "\nElapsed time: ", (GetElapsedTime()),
      "\nElapsed deterministic time: ", (GetElapsedDeterministicTime()));
#ifndef NDEBUG
  for (const auto& counter : deterministic_counters_) {
    const std::string& counter_name = counter.first;
    const double counter_value = counter.second;
    absl::StrAppend(&buffer, "\n", counter_name, ": ", (counter_value));
  }
#endif
  return buffer;
}

NestedTimeLimit::NestedTimeLimit(TimeLimit* base_time_limit,
                                 double limit_in_seconds,
                                 double deterministic_limit)
    : base_time_limit_(ABSL_DIE_IF_NULL(base_time_limit)),
      time_limit_(std::min(base_time_limit_->GetTimeLeft(), limit_in_seconds),
                  std::min(base_time_limit_->GetDeterministicTimeLeft(),
                           deterministic_limit)) {
  if (base_time_limit_->external_boolean_as_limit_ != nullptr) {
    time_limit_.RegisterExternalBooleanAsLimit(
        base_time_limit_->external_boolean_as_limit_);
  }
}

NestedTimeLimit::~NestedTimeLimit() {
  base_time_limit_->AdvanceDeterministicTime(
      time_limit_.GetElapsedDeterministicTime());
}

}  // namespace operations_research
