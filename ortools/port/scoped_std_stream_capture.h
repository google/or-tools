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

#ifndef ORTOOLS_PORT_SCOPED_STD_STREAM_CAPTURE_H_
#define ORTOOLS_PORT_SCOPED_STD_STREAM_CAPTURE_H_

#include <string>

namespace operations_research {

// The stream to capture for
enum class CapturedStream { kStdout, kStderr };

// RAII class that captures stdout & stderr in tests until either
// StopCaptureAndReturnContents() is called or it is destroyed.
//
// This capture is not available in all configurations, thus the `kIsSupported`
// constant should be used in tests to disable the checks on the captured
// result.
//
// Usage:
//
//   TEST(MyTest, Something) {
//     ...
//     ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
//     ... some code that may return ...
//     if (ScopedStdStreamCapture::kIsSupported) {
//       EXPECT_THAT(std::move(stdout_capture).StopCaptureAndReturnContents(),
//                   HasSubstr("..."));
//     }
//   }
class ScopedStdStreamCapture {
 public:
  // True if the capture is supported by the configuration.
  //
  // When false, no capture occurs and StopCaptureAndReturnContents() returns an
  // empty string.
  static const bool kIsSupported;

  // Starts the capture.
  explicit ScopedStdStreamCapture(CapturedStream stream);

  ScopedStdStreamCapture(const ScopedStdStreamCapture&) = delete;
  ScopedStdStreamCapture& operator=(const ScopedStdStreamCapture&) = delete;
  ScopedStdStreamCapture& operator=(ScopedStdStreamCapture&&) = delete;
  ScopedStdStreamCapture(ScopedStdStreamCapture&& other) = delete;

  // Releases the capture if needed.
  ~ScopedStdStreamCapture();

  // Stops the capture and returns the captured data.
  //
  // Returns an empty string when kIsSupported is false.
  //
  // This function can only be called once. This function requires moving *this
  // so that the user gets a ClangTidy error (bugprone-use-after-move) when they
  // try to use this function more than once.
  std::string StopCaptureAndReturnContents() &&;

 private:
  const CapturedStream stream_;
  bool capture_released_ = false;
};

}  // namespace operations_research

#endif  // ORTOOLS_PORT_SCOPED_STD_STREAM_CAPTURE_H_
