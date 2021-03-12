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

#include "ortools/flatzinc/logging.h"

#include "absl/strings/str_split.h"

ABSL_FLAG(bool, fz_logging, false,
          "Print logging information from the flatzinc interpreter.");
ABSL_FLAG(bool, fz_verbose, false,
          "Print verbose logging information from the flatzinc interpreter.");
ABSL_FLAG(bool, fz_debug, false,
          "Print debug logging information from the flatzinc interpreter.");

namespace operations_research {
namespace fz {

void LogInFlatzincFormat(const std::string& multi_line_input) {
  if (multi_line_input.empty()) {
    std::cout << std::endl;
    return;
  }
  std::vector<std::string> lines =
      absl::StrSplit(multi_line_input, '\n', absl::SkipEmpty());
  for (const std::string& line : lines) {
    std::cout << "%% " << line << std::endl;
  }
}

}  // namespace fz
}  // namespace operations_research
