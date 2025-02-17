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

#include "ortools/base/temp_path.h"

#include <utility>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "ortools/base/filesystem.h"

namespace file {

std::string TempFile(absl::string_view prefix) {
  std::string path;
  if (prefix.empty()) {
    path = absl::StrCat(absl::ToUnixMicros(absl::Now()));
  } else {
    path = absl::StrCat(prefix, "_", absl::ToUnixMicros(absl::Now()));
  }
  return path;
}

}  // namespace file

TempPath::TempPath(absl::string_view prefix) : path_(file::TempFile(prefix)) {
  CHECK_OK(Init(kDefaultMode));
}

TempPath::TempPath(absl::string_view prefix, absl::Status* status)
    : path_(file::TempFile(prefix)) {
  *status = Init(kDefaultMode);
}

TempPath::TempPath(TempPath&& rhs) : path_(std::move(rhs.path_)) {}

TempPath& TempPath::operator=(TempPath&& rhs) {
  TempPath tmp(std::move(*this));
  path_ = std::move(rhs.path_);
  return *this;
}

TempPath::~TempPath() {}

TempPath* TempPath::Create(Location location) {
  std::string dirname;
  switch (location) {
    case Local:
      dirname = file::TempFile("");
  }
  if (dirname.empty()) {
    return nullptr;
  }
  absl::Status status;
  TempPath* temp_path = new TempPath(dirname, &status);
  if (!status.ok()) {
    delete temp_path;
    return nullptr;
  }
  return temp_path;
}

TempPath::TempPath(const std::string& dirname, file::Options options,
                   absl::Status* status)
    : path_(dirname) {
  *status = Init(options);
}

absl::Status TempPath::Init(file::Options options) {
  return file::RecursivelyCreateDir(path(), options);
}
