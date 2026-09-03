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

#ifndef ORTOOLS_UTIL_DATA_PATH_RESOLVER_H_
#define ORTOOLS_UTIL_DATA_PATH_RESOLVER_H_

#include <string>

#include "absl/strings/string_view.h"

namespace ortools {

// Resolves a path relative to the root directory/runfiles.
std::string GetDataDependencyFilepath(absl::string_view relative_path);

}  // namespace ortools

#endif  // ORTOOLS_UTIL_DATA_PATH_RESOLVER_H_
