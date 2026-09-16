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

#include "ortools/linear_solver/scip_helper_macros.h"

#include <memory>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <utility>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/port/scoped_std_stream_capture.h"
#include "scip/pub_message.h"
#include "scip/type_retcode.h"

namespace operations_research::internal {
namespace {

using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::status::StatusIs;

// Calls SCIPmessagePrintError() while capturing stderr and returns the captured
// prints.
//
// Note that when ScopedStdStreamCapture::kIsSupported is false, the returned
// string is always empty.
[[nodiscard]] std::string CallSCIPmessagePrintErrorCapturingStderr(
    const char error[]) {
  ScopedStdStreamCapture stderr_capture(CapturedStream::kStderr);
  SCIPmessagePrintError(error);
  return std::move(stderr_capture).StopCaptureAndReturnContents();
}

TEST(ScopedCaptureScipErrorPrintingTest, CaptureCurrentThread) {
  // Test calling SCIPmessagePrintError() in a capture.
  {
    const ScopedCaptureScipErrorPrinting captured_errors;
    EXPECT_EQ(
        CallSCIPmessagePrintErrorCapturingStderr("inside first capture\n"), "");
    EXPECT_THAT(captured_errors.messages(),
                ElementsAre("inside first capture\n"));
  }

  // Test calling SCIPmessagePrintError() without a capture.
  EXPECT_EQ(CallSCIPmessagePrintErrorCapturingStderr(
                "SCIPmessagePrintError called outside capture\n"),
            ScopedStdStreamCapture::kIsSupported
                ? "SCIPmessagePrintError called outside capture\n"
                : "");

  // Test capturing again.
  {
    const ScopedCaptureScipErrorPrinting captured_errors;
    EXPECT_EQ(
        CallSCIPmessagePrintErrorCapturingStderr("inside second capture\n"),
        "");
    EXPECT_THAT(captured_errors.messages(),
                ElementsAre("inside second capture\n"));
  }
}

TEST(ScopedCaptureScipErrorPrintingTest, CaptureOtherThread) {
  const ScopedCaptureScipErrorPrinting captured_errors;

  ScopedStdStreamCapture stderr_capture(CapturedStream::kStderr);

  std::thread other_thread([&]() {
    SCIPmessagePrintError("SCIPmessagePrintError from other_thread\n");
  });

  SCIPmessagePrintError("one message\n");

  other_thread.join();

  if (ScopedStdStreamCapture::kIsSupported) {
    EXPECT_EQ(std::move(stderr_capture).StopCaptureAndReturnContents(),
              "SCIPmessagePrintError from other_thread\n");
  }

  EXPECT_THAT(captured_errors.messages(), ElementsAre("one message\n"));
}

// Tests that we can instantiate a second ScopedCaptureScipErrorPrinting in the
// same thread.
TEST(ScopedCaptureScipErrorPrintingTest, TwoCapturesCurrentThread) {
  {  // Limit scope of first_capture.
    const ScopedCaptureScipErrorPrinting first_capture;
    EXPECT_EQ(CallSCIPmessagePrintErrorCapturingStderr(
                  "first capture - first message\n"),
              "");
    {  // Limit scope of second_capture.
      const ScopedCaptureScipErrorPrinting second_capture;
      EXPECT_EQ(
          CallSCIPmessagePrintErrorCapturingStderr("second capture message\n"),
          "");
      EXPECT_THAT(second_capture.messages(),
                  ElementsAre("second capture message\n"));
    }
    EXPECT_EQ(CallSCIPmessagePrintErrorCapturingStderr(
                  "first capture - second message\n"),
              "");
    EXPECT_THAT(first_capture.messages(),
                ElementsAre("first capture - first message\n",
                            "first capture - second message\n"));
  }

  // Test calling SCIPmessagePrintError() without a capture.
  EXPECT_EQ(CallSCIPmessagePrintErrorCapturingStderr(
                "SCIPmessagePrintError called outside capture\n"),
            ScopedStdStreamCapture::kIsSupported
                ? "SCIPmessagePrintError called outside capture\n"
                : "");

  // Test capturing again.
  {
    const ScopedCaptureScipErrorPrinting captured_errors;
    EXPECT_EQ(
        CallSCIPmessagePrintErrorCapturingStderr("third capture message\n"),
        "");
    EXPECT_THAT(captured_errors.messages(),
                ElementsAre("third capture message\n"));
  }
}

// Tests that invalid destruction order of captures is properly detected.
TEST(ScopedCaptureScipErrorPrintingDeathTest,
     TwoCapturesCurrentThreadInterleaved) {
  auto first_capture = std::make_unique<ScopedCaptureScipErrorPrinting>();
  const ScopedCaptureScipErrorPrinting second_capture;
  EXPECT_DEATH(first_capture.reset(), "current_thread_capture_ == this");
}

TEST(ScopedCaptureScipErrorPrintingMessagesGroupedByLinesTest, Empty) {
  const ScopedCaptureScipErrorPrinting captured_errors;
  EXPECT_THAT(captured_errors.MessagesGroupedByLines(), IsEmpty());
}

TEST(ScopedCaptureScipErrorPrintingMessagesGroupedByLinesTest, OnePartialLine) {
  ScopedCaptureScipErrorPrinting captured_errors;
  captured_errors.messages() = {"partial first line"};
  EXPECT_THAT(captured_errors.MessagesGroupedByLines(),
              ElementsAre("partial first line"));
}

TEST(ScopedCaptureScipErrorPrintingMessagesGroupedByLinesTest, OneFullLine) {
  ScopedCaptureScipErrorPrinting captured_errors;
  captured_errors.messages() = {"partial first line\n"};
  EXPECT_THAT(captured_errors.MessagesGroupedByLines(),
              ElementsAre("partial first line"));
}

TEST(ScopedCaptureScipErrorPrintingMessagesGroupedByLinesTest,
     OneFullLineInTwoPieces) {
  ScopedCaptureScipErrorPrinting captured_errors;
  captured_errors.messages() = {"first ", "line\n"};
  EXPECT_THAT(captured_errors.MessagesGroupedByLines(),
              ElementsAre("first line"));
}

TEST(ScopedCaptureScipErrorPrintingMessagesGroupedByLinesTest,
     OnePartialLineInTwoPieces) {
  ScopedCaptureScipErrorPrinting captured_errors;
  captured_errors.messages() = {"first ", "line"};
  EXPECT_THAT(captured_errors.MessagesGroupedByLines(),
              ElementsAre("first line"));
}

TEST(ScopedCaptureScipErrorPrintingMessagesGroupedByLinesTest,
     ThreeLinesLastPartial) {
  ScopedCaptureScipErrorPrinting captured_errors;
  captured_errors.messages() = {
      "first ",     "line\n",      //
      "sec",        "ond line\n",  //
      "third line",
  };
  EXPECT_THAT(captured_errors.MessagesGroupedByLines(),
              ElementsAre("first line", "second line", "third line"));
}

TEST(ScopedCaptureScipErrorPrintingMessagesGroupedByLinesTest, ThreeFullLines) {
  ScopedCaptureScipErrorPrinting captured_errors;
  captured_errors.messages() = {
      "first ",       "line\n",      //
      "sec",          "ond line\n",  //
      "third line\n",
  };
  EXPECT_THAT(captured_errors.MessagesGroupedByLines(),
              ElementsAre("first line", "second line", "third line"));
}

TEST(ScipToStatusTest, OkayNoErrorMessages) {
  ScopedStdStreamCapture stderr_capture(CapturedStream::kStderr);
  // Use a variable so that we don't call EXPECT_OK while capturing stderr.
  const absl::Status status = SCIP_TO_STATUS([]() { return SCIP_OKAY; }());
  EXPECT_EQ(std::move(stderr_capture).StopCaptureAndReturnContents(), "");

  EXPECT_OK(status);
}

TEST(ScipToStatusTest, OkayWithErrorMessages) {
  ScopedStdStreamCapture stderr_capture(CapturedStream::kStderr);
  // Use a variable so that we don't call EXPECT_OK while capturing stderr.
  const absl::Status status = SCIP_TO_STATUS([]() {
    SCIPmessagePrintError("some message\n");
    return SCIP_OKAY;
  }());
  if (ScopedStdStreamCapture::kIsSupported) {
    // Note that we expect LOG(ERROR) in stderr.
    EXPECT_THAT(
        std::move(stderr_capture).StopCaptureAndReturnContents(),
        AllOf(ContainsRegex(R"re((?m)^E.*?] Scip returned SCIP_OKAY but)re"),
              ContainsRegex(R"re((?m)^E.*?]   some message$)re")));
  }

  EXPECT_OK(status);
}

TEST(ScipToStatusTest, FailureNoErrorMessages) {
  ScopedStdStreamCapture stderr_capture(CapturedStream::kStderr);
  // Use a variable so that we don't call EXPECT_OK while capturing stderr.
  const absl::Status status = SCIP_TO_STATUS([]() { return SCIP_LPERROR; }());
  EXPECT_EQ(std::move(stderr_capture).StopCaptureAndReturnContents(), "");

  EXPECT_THAT(status, StatusIs(absl::StatusCode::kInvalidArgument,
                               HasSubstr("SCIP_LPERROR")));
}

TEST(ScipToStatusTest, FailureWithErrorMessages) {
  ScopedStdStreamCapture stderr_capture(CapturedStream::kStderr);
  // Use a variable so that we don't call EXPECT_OK while capturing stderr.
  const absl::Status status = SCIP_TO_STATUS([]() {
    SCIPmessagePrintError("first message\n");
    SCIPmessagePrintError("second ");
    SCIPmessagePrintError("message\n");
    return SCIP_LPERROR;
  }());
  EXPECT_EQ(std::move(stderr_capture).StopCaptureAndReturnContents(), "");

  EXPECT_THAT(
      status,
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("SCIP_LPERROR"),
                     HasSubstr("['first message', 'second message']"))));
}

}  // namespace
}  // namespace operations_research::internal
