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

#ifndef OR_TOOLS_BASE_MOCK_LOG_H_
#define OR_TOOLS_BASE_MOCK_LOG_H_

#include <string>

#include "absl/base/log_severity.h"
#include "gmock/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/raw_logging.h"
// #include "ortools/base/threadlocal.h"

namespace testing {

// A ScopedMockLog object intercepts LOG() messages issued during its
// lifespan.  Using this together with gMock, it's very easy to test
// how a piece of code calls LOG().

// Used only for invoking the single-argument ScopedMockLog
// constructor.  Do not use the type LogCapturingState_ directly.
// It's OK and expected for a user to use the enum value
// kDoNotCaptureLogsYet.
enum LogCapturingState_ { kDoNotCaptureLogsYet };

class ScopedMockLog : public google::LogSink {
 public:
  // A user can use the syntax
  //   ScopedMockLog log(kDoNotCaptureLogsYet);
  // to invoke this constructor.  A ScopedMockLog object created this way
  // does not start capturing logs until StartCapturingLogs() is called.
  explicit ScopedMockLog(LogCapturingState_ /* dummy */)
      : is_capturing_logs_(false) {}

  // When the object is destructed, it stops intercepting logs.
  ~ScopedMockLog() override {
    if (is_capturing_logs_) StopCapturingLogs();
  }

  // Starts log capturing if the object isn't already doing so.
  // Otherwise crashes.  Usually this method is called in the same
  // thread that created this object.  It is the user's responsibility
  // to not call this method if another thread may be calling it or
  // StopCapturingLogs() at the same time.
  void StartCapturingLogs() {
    // We don't use CHECK(), which can generate a new LOG message, and
    // thus can confuse ScopedMockLog objects or other registered
    // LogSinks.
    ABSL_RAW_CHECK(
        !is_capturing_logs_,
        "StartCapturingLogs() can be called only when the ScopedMockLog "
        "object is not capturing logs.");

    is_capturing_logs_ = true;
    google::AddLogSink(this);
  }

  // Stops log capturing if the object is capturing logs.  Otherwise
  // crashes.  Usually this method is called in the same thread that
  // created this object.  It is the user's responsibility to not call
  // this method if another thread may be calling it or
  // StartCapturingLogs() at the same time.
  void StopCapturingLogs() {
    // We don't use CHECK(), which can generate a new LOG message, and
    // thus can confuse ScopedMockLog objects or other registered
    // LogSinks.
    ABSL_RAW_CHECK(
        is_capturing_logs_,
        "StopCapturingLogs() can be called only when the ScopedMockLog "
        "object is capturing logs.");

    is_capturing_logs_ = false;
    google::RemoveLogSink(this);
  }

  // Implements the mock method:
  //
  //   void Log(LogSeverity severity, const string& file_path,
  //            const string& message);
  //
  // The second argument to Log() is the full path of the source file
  // in which the LOG() was issued.
  //
  // Note, that in a multi-threaded environment, all LOG() messages from a
  // single thread will be handled in sequence, but that cannot be guaranteed
  // for messages from different threads. In fact, if the same or multiple
  // expectations are matched on two threads concurrently, their actions will
  // be executed concurrently as well and may interleave.
  MOCK_METHOD(void, Log,
              (LogSeverity severity, const std::string& file_path,
               const std::string& message));

 private:
  // Implements the Send() virtual function in class LogSink.
  // Whenever a LOG() statement is executed, this function will be
  // invoked with information presented in the LOG().
  void Send(const google::LogEntry& entry) override {
    // We are only interested in the log severity, full file name, and
    // log message.
    Log(static_cast<LogSeverity>(entry.log_severity()),
        std::string(entry.source_filename()),
        std::string(entry.text_message()));
  }

  bool is_capturing_logs_;
};

}  // namespace testing

#endif  // OR_TOOLS_BASE_MOCK_LOG_H_
