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

#include "ortools/util/logging.h"

#include <iostream>

#include "absl/strings/str_cat.h"
#include "ortools/base/logging.h"

namespace operations_research {

void SolverLogger::AddInfoLoggingCallback(
    std::function<void(const std::string& message)> callback) {
  info_callbacks_.push_back(callback);
}

void SolverLogger::ClearInfoLoggingCallbacks() { info_callbacks_.clear(); }

void SolverLogger::LogInfo(const char* source_filename, int source_line,
                           const std::string& message) {
  if (log_to_stdout_) {
    std::cout << message << std::endl;
  }

  for (const auto& callback : info_callbacks_) {
    callback(message);
  }
}

}  // namespace operations_research
