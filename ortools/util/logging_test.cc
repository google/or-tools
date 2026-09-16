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

#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/port/scoped_std_stream_capture.h"

namespace operations_research {
namespace {

TEST(FormatCounterTest, BasicCases) {
  EXPECT_EQ("12", FormatCounter(12));
  EXPECT_EQ("12'345", FormatCounter(12345));
  EXPECT_EQ("123'456'789", FormatCounter(123456789));
}

TEST(LoggingTest, StandardLogToStdout) {
  SolverLogger logger;
  logger.EnableLogging(true);
  logger.SetLogToStdOut(true);

  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  SOLVER_LOG(&logger, "output to log");
  if (ScopedStdStreamCapture::kIsSupported) {
    EXPECT_EQ(std::move(stdout_capture).StopCaptureAndReturnContents(),
              "output to log\n");
  }
}

TEST(LoggingTest, StandardLogDisabledStdout) {
  SolverLogger logger;
  logger.EnableLogging(true);
  logger.SetLogToStdOut(false);

  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  SOLVER_LOG(&logger, "output to log");
  EXPECT_EQ(std::move(stdout_capture).StopCaptureAndReturnContents(), "");
}

TEST(LoggingTest, CaptureLogCallback) {
  SolverLogger logger;
  logger.EnableLogging(true);
  logger.SetLogToStdOut(false);

  CHECK_EQ(logger.NumInfoLoggingCallbacks(), 0);

  // Custom callback method.
  std::string output_string;
  const auto write_to_string = [&output_string](absl::string_view message) {
    output_string.append(message);
    output_string.append("\n");
  };
  logger.AddInfoLoggingCallback(write_to_string);

  CHECK_EQ(logger.NumInfoLoggingCallbacks(), 1);

  SOLVER_LOG(&logger, "output to string");
  EXPECT_EQ(output_string, "output to string\n");
  SOLVER_LOG(&logger, "!!");
  EXPECT_EQ(output_string, "output to string\n!!\n");

  logger.ClearInfoLoggingCallbacks();

  CHECK_EQ(logger.NumInfoLoggingCallbacks(), 0);
  SOLVER_LOG(&logger, "we should not see that");
  EXPECT_EQ(output_string, "output to string\n!!\n");
}

TEST(LoggingTest, Throttling) {
  SolverLogger logger;

  int num_seen = 0;
  int num_skipped = 0;
  const auto callback = [&num_seen, &num_skipped](absl::string_view message) {
    ++num_seen;

    const std::string marker = "[skipped_logs=";
    const int start_pos = message.find(marker);
    if (start_pos < 0) return;
    int num = 0;

    std::string sub = std::string(message.substr(start_pos + marker.size()));
    int end_pos = sub.find(']');
    if (end_pos < 0) return;
    sub = sub.substr(0, end_pos);

    if (absl::SimpleAtoi(sub, &num)) num_skipped += num;
  };

  logger.EnableLogging(true);
  logger.AddInfoLoggingCallback(callback);

  // Hopefully, this will be above the default limit in all settings.
  const int id = logger.GetNewThrottledId();
  const int num_emitted = 1000;
  for (int i = 0; i < num_emitted; ++i) {
    logger.ThrottledLog(id, "test");
    if (i == 500) {
      // Lets output some in the middle.
      logger.FlushPendingThrottledLogs(/*ignore_rates=*/true);
    }
  }

  // Make sure we output last one.
  logger.FlushPendingThrottledLogs(/*ignore_rates=*/true);

  EXPECT_GT(num_seen, 10);  // We have a minimum limit.
  EXPECT_LT(num_seen, 500);
  EXPECT_EQ(num_seen + num_skipped, num_emitted);
}

}  // namespace
}  // namespace operations_research
