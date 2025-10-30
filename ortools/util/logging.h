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

#ifndef OR_TOOLS_UTIL_LOGGING_H_
#define OR_TOOLS_UTIL_LOGGING_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"  // IWYU pragma: export
#include "ortools/base/timer.h"
#include "ortools/util/time_limit.h"

namespace operations_research {

// Prints a positive number with separators for easier reading (ex: 1'348'065).
std::string FormatCounter(int64_t num);

// Custom logger class. It allows passing callbacks to process log messages.
//
// If no callbacks have been added, all logging will use the standard logging
// facilities. As soon as one callback is added, it is disabled. Unless
// ForceStandardLogging() has been called.
//
// Note that the callbacks will get the message unchanged. No '\n' will be
// added.
//
// Important: This class is currently not thread-safe, it is easy to add a mutex
// if needed. In CP-SAT, we currently make sure all access to this class do not
// happen concurrently.
class SolverLogger {
 public:
  SolverLogger();

  // Enables all logging.
  //
  // Note that this is used by the logging macro, but it actually do not
  // disable logging if LogInfo() is called directly.
  void EnableLogging(bool enable) { is_enabled_ = enable; }

  // Returns true iff logging is enabled.
  bool LoggingIsEnabled() const { return is_enabled_; }

  // Should all messages be displayed on stdout ?
  void SetLogToStdOut(bool enable) { log_to_stdout_ = enable; }

  // Add a callback listening to all information messages.
  //
  // They will be run synchronously when LogInfo() is called.
  void AddInfoLoggingCallback(
      std::function<void(const std::string& message)> callback);

  // Removes all callbacks registered via AddInfoLoggingCallback().
  void ClearInfoLoggingCallbacks();

  // Returns the number of registered callbacks.
  int NumInfoLoggingCallbacks() const { return info_callbacks_.size(); }

  // Logs a given information message and dispatch it to all callbacks.
  void LogInfo(const char* source_filename, int source_line,
               const std::string& message);

  // Facility to avoid having multi megabytes logs when it brings little
  // benefits. Logs with the same id will be kept under an average of
  // throttling_rate_ logs per second.
  int GetNewThrottledId();
  void ThrottledLog(int id, const std::string& message);

  // To not loose the last message of a throttled log, we keep it in memory and
  // when this function is called we flush logs whose rate is now under the
  // limit.
  void FlushPendingThrottledLogs(bool ignore_rates = false);

 private:
  struct ThrottlingData {
    int64_t num_displayed_logs = 0;
    int64_t num_last_skipped_logs = 0;
    std::string last_skipped_message;

    void UpdateWhenDisplayed() {
      num_displayed_logs++;
      num_last_skipped_logs = 0;
      last_skipped_message = "";
    }
  };
  bool RateIsOk(const ThrottlingData& data);

  bool is_enabled_ = false;
  bool log_to_stdout_ = false;
  std::vector<std::function<void(const std::string& message)>> info_callbacks_;

  // TODO(user): Expose? for now we never change this. We start throttling after
  // throttling_threshold_ logs of a given id, and we enforce a fixed logging
  // rate afterwards, so that latter burst can still be seen.
  const int throttling_threshold_ = 20;
  const double throttling_rate_ = 1.0;

  WallTimer timer_;
  std::vector<ThrottlingData> id_to_throttling_data_;
};

#define SOLVER_LOG(logger, ...)     \
  if ((logger)->LoggingIsEnabled()) \
  (logger)->LogInfo(__FILE__, __LINE__, absl::StrCat(__VA_ARGS__))

// Simple helper class to:
// - log in an uniform way a "time-consuming" presolve operation.
// - track a deterministic work limit.
// - update the deterministic time on finish.
//
// TODO(user): this is not presolve specific. Rename.
class PresolveTimer {
 public:
  PresolveTimer(std::string name, SolverLogger* logger, TimeLimit* time_limit)
      : name_(std::move(name)), logger_(logger), time_limit_(time_limit) {
    timer_.Start();
  }

  // Track the work done (which is also the deterministic time).
  // By default we want a limit of around 1 deterministic seconds.
  void AddToWork(double dtime) { work_ += dtime; }
  void TrackSimpleLoop(int size) { work_ += 5e-9 * size; }
  void TrackFastLoop(int size) { work_ += 1e-9 * size; }
  bool WorkLimitIsReached() const { return work_ >= 1.0; }

  // Extra stats=value to display at the end.
  // We filter value of zero to have less clutter.
  void AddCounter(std::string name, int64_t count) {
    if (count == 0) return;
    counters_.emplace_back(std::move(name), count);
  }

  // Extra info at the end of the log line.
  void AddMessage(std::string name) { extra_infos_.push_back(std::move(name)); }

  // Updates dtime and log operation summary.
  ~PresolveTimer();

  // Can be used to bypass logger_->LoggingIsEnabled() to either always disable
  // in some code path or to always log when debugging.
  void OverrideLogging(bool value) {
    override_logging_ = true;
    log_when_override_ = value;
  };

  double deterministic_time() const { return work_; }
  double wtime() const { return timer_.Get(); }

 private:
  const std::string name_;

  WallTimer timer_;
  SolverLogger* logger_;
  TimeLimit* time_limit_;

  bool override_logging_ = false;
  bool log_when_override_ = false;
  double work_ = 0.0;
  std::vector<std::pair<std::string, int64_t>> counters_;
  std::vector<std::string> extra_infos_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_LOGGING_H_
