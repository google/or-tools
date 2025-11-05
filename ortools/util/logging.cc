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

#include "ortools/util/logging.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <ostream>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

namespace operations_research {

std::string FormatCounter(int64_t num) {
  std::string s = absl::StrCat(num);
  std::string out;
  const int size = s.size();
  for (int i = 0; i < size; ++i) {
    if (i > 0 && (size - i) % 3 == 0) {
      out.push_back('\'');
    }
    out.push_back(s[i]);
  }
  return out;
}

SolverLogger::SolverLogger() { timer_.Start(); }

void SolverLogger::AddInfoLoggingCallback(
    std::function<void(const std::string& message)> callback) {
  info_callbacks_.push_back(std::move(callback));
}

void SolverLogger::ClearInfoLoggingCallbacks() { info_callbacks_.clear(); }

void SolverLogger::LogInfo(const char* source_filename, int source_line,
                           const std::string& message) {
  if (log_to_stdout_) {
    std::cout << message << std::endl;
  }

  for (const auto& callback : info_callbacks_) {
    callback(message);
  }
}

int SolverLogger::GetNewThrottledId() {
  const int id = id_to_throttling_data_.size();
  id_to_throttling_data_.resize(id + 1);
  return id;
}

bool SolverLogger::RateIsOk(const ThrottlingData& data) {
  const double time = std::max(1.0, timer_.Get());
  const double rate =
      static_cast<double>(data.num_displayed_logs - throttling_threshold_) /
      time;
  return rate < throttling_rate_;
}

void SolverLogger::ThrottledLog(int id, const std::string& message) {
  if (!is_enabled_) return;
  ThrottlingData& data = id_to_throttling_data_[id];
  if (RateIsOk(data)) {
    if (data.num_last_skipped_logs > 0) {
      LogInfo("", 0,
              absl::StrCat(message,
                           " [skipped_logs=", data.num_last_skipped_logs, "]"));
    } else {
      LogInfo("", 0, message);
    }
    data.UpdateWhenDisplayed();
  } else {
    data.num_last_skipped_logs++;
    data.last_skipped_message = message;
  }
}

void SolverLogger::FlushPendingThrottledLogs(bool ignore_rates) {
  if (!is_enabled_) return;

  // TODO(user): If this is called too often, we could optimize it and do
  // nothing if there are no skipped logs.
  for (int id = 0; id < id_to_throttling_data_.size(); ++id) {
    ThrottlingData& data = id_to_throttling_data_[id];
    if (data.num_last_skipped_logs == 0) continue;
    if (ignore_rates || RateIsOk(data)) {
      // Note the -1 since we didn't skip the last log in the end.
      LogInfo("", 0,
              absl::StrCat(data.last_skipped_message, " [skipped_logs=",
                           data.num_last_skipped_logs - 1, "]"));
      data.UpdateWhenDisplayed();
    }
  }
}

PresolveTimer::~PresolveTimer() {
  time_limit_->AdvanceDeterministicTime(work_);

  std::string counter_string;
  for (const auto& [counter_name, count] : counters_) {
    absl::StrAppend(&counter_string, " #", counter_name, "=",
                    FormatCounter(count));
  }

  // We use absl::Seconds() to get a nicer display.
  if (override_logging_ ? log_when_override_ : logger_->LoggingIsEnabled()) {
    logger_->LogInfo(
        __FILE__, __LINE__,
        absl::StrCat(absl::StrFormat("  %.2es", timer_.Get()),
                     absl::StrFormat("  %.2ed", work_),
                     (WorkLimitIsReached() ? " *" : "  "), "[", name_, "]",
                     counter_string, " ", absl::StrJoin(extra_infos_, " ")));
  }
}

}  // namespace operations_research
