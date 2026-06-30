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

#include "ortools/util/data_path_resolver.h"

#include <string>

#include "absl/log/log.h"  // IWYU pragma: keep
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#ifdef ORTOOLS_USE_RUNFILES
#include "rules_cc/cc/runfiles/runfiles.h"
#endif  // ORTOOLS_USE_RUNFILES

namespace ortools {

std::string GetDataDependencyFilepath(absl::string_view relative_path) {
#ifdef ORTOOLS_USE_RUNFILES
  using ::rules_cc::cc::runfiles::Runfiles;
  std::string error;
  std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(&error));
  if (runfiles == nullptr) {
    LOG(FATAL) << "Failed to create Runfiles: " << error;
  }
  return runfiles->Rlocation(absl::StrCat("_main/", relative_path));
#else
  return std::string(relative_path);
#endif  // ORTOOLS_USE_RUNFILES
}

}  // namespace ortools
