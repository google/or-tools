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

#include "ortools/port/scoped_std_stream_capture.h"

#include <iostream>
#include <utility>

#include "gtest/gtest.h"

namespace operations_research {
namespace {

#ifdef OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED

TEST(ScopedStdStreamCaptureTest, CaptureStdout) {
  ScopedStdStreamCapture capture(CapturedStream::kStdout);
  std::cout << "something";
  EXPECT_EQ(std::move(capture).StopCaptureAndReturnContents(), "something");
}

TEST(ScopedStdStreamCaptureTest, CaptureStderr) {
  ScopedStdStreamCapture capture(CapturedStream::kStderr);
  std::cerr << "something";
  EXPECT_EQ(std::move(capture).StopCaptureAndReturnContents(), "something");
}

TEST(ScopedStdStreamCaptureTest, EarlyExitThenCaptureStdout) {
  {
    ScopedStdStreamCapture capture(CapturedStream::kStdout);
    std::cout << "some thing";
  }
  {
    ScopedStdStreamCapture capture(CapturedStream::kStdout);
    std::cout << "another thing";
    EXPECT_EQ(std::move(capture).StopCaptureAndReturnContents(),
              "another thing");
  }
}

TEST(ScopedStdStreamCaptureTest, EarlyExitThenCaptureStderr) {
  {
    ScopedStdStreamCapture capture(CapturedStream::kStderr);
    std::cerr << "some thing";
  }
  {
    ScopedStdStreamCapture capture(CapturedStream::kStderr);
    std::cerr << "another thing";
    EXPECT_EQ(std::move(capture).StopCaptureAndReturnContents(),
              "another thing");
  }
}

TEST(ScopedStdStreamCaptureTest, CaptureStdoutAndStderr) {
  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  ScopedStdStreamCapture stderr_capture(CapturedStream::kStderr);

  std::cout << "stdout";
  std::cerr << "stderr";

  EXPECT_EQ(std::move(stdout_capture).StopCaptureAndReturnContents(), "stdout");
  EXPECT_EQ(std::move(stderr_capture).StopCaptureAndReturnContents(), "stderr");
}

TEST(ScopedStdStreamCaptureDeathTest, TwoCalls) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        ScopedStdStreamCapture capture(CapturedStream::kStdout);
        std::cout << "something";
        std::move(capture).StopCaptureAndReturnContents();
        // ClangTidy reports calling StopCaptureAndReturnContents() twice as an
        // error, which is what we want. But here we want to test the CHECK()
        // failure anyway since the code compiles anyway and some users may not
        // notice the ClangTidy error when they write new code.
        //
        // NOLINTNEXTLINE(bugprone-use-after-move)
        std::move(capture).StopCaptureAndReturnContents();
      },
      "twice");
}

#else  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED

TEST(ScopedStdStreamCaptureTest, NotSupported) {
  // This unit test is intentionally empty since iOS test fails when zero tests
  // are executed.
}

#endif  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED

}  // namespace
}  // namespace operations_research
