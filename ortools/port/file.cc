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

#include "ortools/port/file.h"

#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ortools/base/filesystem.h"
#include "ortools/base/macros/os_support.h"

#if defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
static_assert(operations_research::kTargetOsSupportsFile);
#if !defined(_MSC_VER)
#include <unistd.h>
#endif !defined(_MSC_VER)

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#else   // defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
static_assert(!operations_research::kTargetOsSupportsFile);
#endif  // defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)

namespace operations_research {

::absl::Status PortableFileSetContents(absl::string_view file_name,
                                       absl::string_view content) {
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
  return file::SetContents(file_name, content, file::Defaults());
#else   // defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
  static_assert(!kTargetOsSupportsFile);
  return absl::Status(absl::StatusCode::kUnimplemented,
                      "File io is not implemented for this platform.");
#endif  // !defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
}

::absl::Status PortableFileGetContents(absl::string_view file_name,
                                       std::string* output) {
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
  return file::GetContents(file_name, output, file::Defaults());
#else   // defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
  static_assert(!kTargetOsSupportsFile);
  return absl::Status(absl::StatusCode::kUnimplemented,
                      "File io is not implemented for this platform.");
#endif  // !defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
}

::absl::Status PortableDeleteFile(absl::string_view file_name) {
#if defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
  return file::Delete(file_name, file::Defaults());
#else   // defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
  static_assert(!kTargetOsSupportsFile);
  return absl::Status(absl::StatusCode::kUnimplemented,
                      "File io is not implemented for this platform.");
#endif  // !defined(ORTOOLS_TARGET_OS_SUPPORTS_FILE)
}
}  // namespace operations_research
