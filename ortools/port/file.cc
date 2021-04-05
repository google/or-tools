// Copyright 2010-2021 Google LLC
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

#include "ortools/port/file.h"

#include "absl/status/status.h"
#include "ortools/base/logging.h"

#if !defined(__PORTABLE_PLATFORM__)
#if !defined(_MSC_VER)
#include <unistd.h>
#endif

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "ortools/base/file.h"
#endif  // !defined(__PORTABLE_PLATFORM__)

namespace operations_research {

::absl::Status PortableFileSetContents(absl::string_view file_name,
                                       absl::string_view content) {
#if defined(__PORTABLE_PLATFORM__)
  return absl::Status(absl::StatusCode::kUnimplemented,
                      "File io is not implemented for this platform.");
#else   // defined(__PORTABLE_PLATFORM__)
  return file::SetContents(file_name, content, file::Defaults());
#endif  // !defined(__PORTABLE_PLATFORM__)
}

::absl::Status PortableFileGetContents(absl::string_view file_name,
                                       std::string* output) {
#if defined(__PORTABLE_PLATFORM__)
  return absl::Status(absl::StatusCode::kUnimplemented,
                      "File io is not implemented for this platform.");
#else   // defined(__PORTABLE_PLATFORM__)
  return file::GetContents(file_name, output, file::Defaults());
#endif  // !defined(__PORTABLE_PLATFORM__)
}

bool PortableTemporaryFile(const char* directory_prefix,
                           std::string* filename_out) {
#if defined(__PORTABLE_PLATFORM__)
  LOG(ERROR) << "Temporary files are not implemented for this platform.";
  return false;
#else  // defined(__PORTABLE_PLATFORM__)
#if defined(__linux)
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
  std::string filename =
      absl::StrFormat("/tmp/parameters-tempfile-%x-%d-%llx", tid, pid, now);
  return true;
#endif  // !defined(__PORTABLE_PLATFORM__)
}

::absl::Status PortableDeleteFile(absl::string_view file_name) {
#if defined(__PORTABLE_PLATFORM__)
  return absl::Status(absl::StatusCode::kUnimplemented,
                      "File io is not implemented for this platform.");
#else   // defined(__PORTABLE_PLATFORM__)
  return file::Delete(file_name, file::Defaults());
#endif  // !defined(__PORTABLE_PLATFORM__)
}
}  // namespace operations_research
