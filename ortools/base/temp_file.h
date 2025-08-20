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

// emulates g3/file/util/temp_file.h
#ifndef ORTOOLS_BASE_TEMP_FILE_H_
#define ORTOOLS_BASE_TEMP_FILE_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace file {

// The following functions return a temporary-looking filename.
// Note that the pattern of the generated filenames is subject to change.

// Returns a unique filename in 'directory' (which can be followed or not by a
// "/"). Unique filenames begin with the given file_prefix followed by a unique
// string.
// If 'directory' is empty a local scratch directory is selected.
// If 'file_prefix' is empty a default prefix is used.
::absl::StatusOr<std::string> MakeTempFilename(absl::string_view directory,
                                               absl::string_view file_prefix);

}  // namespace file

#endif  // ORTOOLS_BASE_TEMP_FILE_H_
