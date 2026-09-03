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

#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>  // NOLINT

#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "ortools/base/macros/os_support.h"

#if defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
static_assert(operations_research::kTargetOsSupportsFile);
#ifdef _WIN32
#include <windows.h>
#define GetPID() GetCurrentProcessId()
#else
#include <unistd.h>
#define GetPID() getpid()
#endif
#else
static_assert(!operations_research::kTargetOsSupportsFile);
#endif  // defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)

namespace file {

::absl::StatusOr<std::string> MakeTempFilename(absl::string_view directory,
                                               absl::string_view file_prefix) {
  std::string filename;
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
  static_assert(operations_research::kTargetOsSupportsFile);
  const size_t tid = absl::HashOf(std::this_thread::get_id());
  const size_t pid = absl::HashOf(GetPID());
  const int64_t now = absl::GetCurrentTimeNanos();

  if (directory.empty()) {
    directory = "/tmp";
  }

  if (file_prefix.empty()) {
    file_prefix = "tempfile";
  }

  filename = absl::StrFormat("%s/%s-%x-%d-%llx", directory, file_prefix, tid,
                             pid, now);
  return filename;
#else   // defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
  static_assert(!operations_research::kTargetOsSupportsFile);
  return absl::UnavailableError(
      "Temporary files are not implemented for this platform.");
#endif  // defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
}

}  // namespace file
