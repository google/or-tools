// Copyright 2010-2022 Google LLC
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

#include <functional>
#include <string>
#include <vector>

namespace operations_research {

// Custom logger class. It allows passing callbacks to process log messages.
// It also enables separating logs from multiple solvers running concurrently in
// the same process.
//
// If no callbacks have been added, all logging will use the standard logging
// facilities. As soon as one callback is added, it is disabled. Unless
// ForceStandardLogging() has been called.
//
// Note that the callbacks will get the message unchanged. No '\n' will be
// added.
class SolverLogger {
 public:
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

 private:
  bool is_enabled_ = false;
  bool log_to_stdout_ = false;
  std::vector<std::function<void(const std::string& message)>> info_callbacks_;
};

#define SOLVER_LOG(logger, ...)     \
  if ((logger)->LoggingIsEnabled()) \
  (logger)->LogInfo(__FILE__, __LINE__, absl::StrCat(__VA_ARGS__))

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_LOGGING_H_
