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

#ifdef OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED

#include <string>

#include "absl/log/check.h"
#include "testing/base/public/googletest.h"

namespace operations_research {

ScopedStdStreamCapture::ScopedStdStreamCapture(const CapturedStream stream)
    : stream_(stream) {
  switch (stream) {
    case CapturedStream::kStdout:
      CaptureTestStdout();
      break;
    case CapturedStream::kStderr:
      CaptureTestStderr();
      break;
  }
}

ScopedStdStreamCapture::~ScopedStdStreamCapture() {
  if (!capture_released_) {
    switch (stream_) {
      case CapturedStream::kStdout:
        GetCapturedTestStdout();
        break;
      case CapturedStream::kStderr:
        GetCapturedTestStderr();
        break;
    }
  }
}

std::string ScopedStdStreamCapture::StopCaptureAndReturnContents() && {
  CHECK(!capture_released_) << "Can't call this function twice.";
  capture_released_ = true;
  switch (stream_) {
    case CapturedStream::kStdout:
      return GetCapturedTestStdout();
    case CapturedStream::kStderr:
      return GetCapturedTestStderr();
  }
}

}  // namespace operations_research

#endif  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
