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

#include "ortools/base/temp_file.h"

#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "ortools/base/logging.h"

#if !defined(__PORTABLE_PLATFORM__)
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#endif  // !defined(__PORTABLE_PLATFORM__)

namespace file {

::absl::StatusOr<std::string> MakeTempFilename(absl::string_view directory,
                                               absl::string_view file_prefix) {
  std::string filename;
#if defined(__PORTABLE_PLATFORM__)
  filename = "Temporary files are not implemented for this platform.";
  LOG(ERROR) << filename;
  return absl::UnavailableError(filename);
#endif  // !defined(__PORTABLE_PLATFORM__)

#if defined(__linux__)
  int32_t tid = static_cast<int32_t>(pthread_self());
#else   // defined(__linux__)
  int32_t tid = 123;
#endif  // defined(__linux__)
#if !defined(_MSC_VER)
  int32_t pid = static_cast<int32_t>(getpid());
#else   // _MSC_VER
  int32_t pid = 456;
#endif  // _MSC_VER
  int64_t now = absl::GetCurrentTimeNanos();

  if (directory.empty()) {
    directory = "/tmp";
  }

  if (file_prefix.empty()) {
    file_prefix = "tempfile";
  }

  filename = absl::StrFormat("%s/%s-%x-%d-%llx", directory, file_prefix, tid,
                             pid, now);
  return filename;
}

}  // namespace file
