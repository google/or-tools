// Copyright 2010-2018 Google LLC
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

#include "absl/status/status.h"
#include "ortools/port/file.h"

#if !defined(_MSC_VER)
#include <unistd.h>
#endif

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "ortools/base/file.h"

namespace operations_research {

::absl::Status PortableFileSetContents(absl::string_view file_name,
                                       absl::string_view content) {
  return file::SetContents(file_name, content, file::Defaults());
}

::absl::Status PortableFileGetContents(absl::string_view file_name,
                                       std::string* output) {
  return file::GetContents(file_name, output, file::Defaults());
}

bool PortableTemporaryFile(const char* directory_prefix,
                           std::string* filename_out) {
#if defined(__linux)
  int32 tid = static_cast<int32>(pthread_self());
#else   // defined(__linux__)
  int32 tid = 123;
#endif  // defined(__linux__)
#if !defined(_MSC_VER)
  int32 pid = static_cast<int32>(getpid());
#else   // _MSC_VER
  int32 pid = 456;
#endif  // _MSC_VER
  int64 now = absl::GetCurrentTimeNanos();
  std::string filename =
      absl::StrFormat("/tmp/parameters-tempfile-%x-%d-%llx", tid, pid, now);
  return true;
}

::absl::Status PortableDeleteFile(absl::string_view file_name) {
  return file::Delete(file_name, file::Defaults());
}

}  // namespace operations_research
