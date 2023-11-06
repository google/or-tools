// Copyright 2010-2022 Google LLC
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

#include "ortools/base/filesystem.h"

#include <filesystem>  // NOLINT(build/c++17)

#include "absl/status/status.h"

namespace file {

absl::Status Match(std::string_view pattern, std::vector<std::string>* result,
                   const file::Options& options) {
  return absl::Status();
}

absl::Status IsDirectory(std::string_view path, const file::Options& options) {
  (void)options;
  if (std::filesystem::is_directory(std::filesystem::path(path))) {
    return absl::OkStatus();
  } else {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        absl::StrCat(path, " exists, but is not a directory"));
  }
}

}  // namespace file
