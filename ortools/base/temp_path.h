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

// emulates g3/file/util/temp_path.h
#ifndef ORTOOLS_BASE_TEMP_PATH_H_
#define ORTOOLS_BASE_TEMP_PATH_H_

#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ortools/base/file.h"

class TempPath {
 public:
  // default mode to create directories (a+rwx):
  static constexpr int kDefaultMode = 0777;

  explicit TempPath(absl::string_view prefix);
  TempPath(absl::string_view prefix, absl::Status* status);

  // TempPath is moveable, but not copyable.
  TempPath(TempPath&& rhs);
  TempPath(const TempPath& rhs) = delete;
  TempPath& operator=(TempPath&& rhs);
  TempPath& operator=(const TempPath& rhs) = delete;

  ~TempPath();

  // Returns the path which was created by this object.
  std::string path() const { return path_; }

  enum Location {
    Local,
  };

  static TempPath* Create(Location location);

 private:
  // Internal constructor for Create* methods.
  TempPath(const std::string& dirname, file::Options options,
           absl::Status* status);

  // Shared initialization among constructors.
  // Makes directory given by path() and `options`.
  absl::Status Init(file::Options options);

  std::string path_;
};

namespace file {}  // namespace file

#endif  // ORTOOLS_BASE_TEMP_PATH_H_
