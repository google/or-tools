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

#include "ortools/base/version.h"

#include "absl/strings/str_cat.h"

namespace operations_research {

int OrToolsMajorVersion() { return OR_TOOLS_MAJOR; }

int OrToolsMinorVersion() { return OR_TOOLS_MINOR; }

int OrToolsPatchVersion() { return OR_TOOLS_PATCH; }

std::string OrToolsVersionString() {
  return absl::StrCat(OR_TOOLS_MAJOR, ".", OR_TOOLS_MINOR, ".", OR_TOOLS_PATCH);
}

}  // namespace operations_research
