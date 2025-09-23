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

#include "ortools/base/filesystem.h"

#include <algorithm>
#include <exception>   // IWYU pragma: keep
#include <filesystem>  // NOLINT(build/c++17)
#include <regex>       // NOLINT
#include <string>
#include <string_view>
#include <system_error>  // NOLINT
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_replace.h"
#include "ortools/base/file.h"

namespace fs = std::filesystem;

// Converts a absl::string_view into an object compatible with std::filesystem.
#ifdef ABSL_USES_STD_STRING_VIEW
#define SV_ABSL_TO_STD(X) X
#else
#define SV_ABSL_TO_STD(X) std::string(X)
#endif

namespace file {

absl::Status Match(std::string_view pattern, std::vector<std::string>* result,
                   const file::Options& options) {
  try {
    const auto search_dir = fs::path(SV_ABSL_TO_STD(pattern)).parent_path();
    const auto filename = fs::path(SV_ABSL_TO_STD(pattern)).filename().string();
    std::string regexp_filename =
        absl::StrReplaceAll(filename, {{".", "\\."}, {"*", ".*"}, {"?", "."}});
    std::regex regexp_pattern(regexp_filename);
    std::error_code error;

    const fs::directory_iterator path_end;
    for (auto path = fs::directory_iterator(search_dir, error);
         !error && path != path_end; path.increment(error)) {
      if (!fs::is_regular_file(path->path())) {
        continue;
      }
      if (std::regex_match(path->path().filename().string(), regexp_pattern)) {
        result->push_back(path->path().string());
      }
    }
    if (error) {
      return absl::InvalidArgumentError(error.message());
    }

    std::sort(result->begin(), result->end());
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(e.what());
  }
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

absl::Status RecursivelyCreateDir(std::string_view path,
                                  const file::Options& options) {
  (void)options;
  try {
    std::filesystem::create_directories(std::filesystem::path(path));
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(e.what());
  }
}

}  // namespace file
