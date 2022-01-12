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

#include "ortools/math_opt/solvers/gscip_solver_message_callback_handler.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ortools/gscip/gscip_message_handler.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/solvers/message_callback_data.h"

namespace operations_research {
namespace math_opt {

GScipSolverMessageCallbackHandler::GScipSolverMessageCallbackHandler(
    SolverInterface::MessageCallback message_callback)
    : message_callback_(std::move(message_callback)) {}

GScipSolverMessageCallbackHandler::~GScipSolverMessageCallbackHandler() {
  const absl::MutexLock lock(&message_mutex_);
  const std::vector<std::string> lines = message_callback_data_.Flush();
  if (!lines.empty()) {
    message_callback_(lines);
  }
}

GScipMessageHandler GScipSolverMessageCallbackHandler::MessageHandler() {
  return std::bind(&GScipSolverMessageCallbackHandler::MessageCallback, this,
                   std::placeholders::_1, std::placeholders::_2);
}

void GScipSolverMessageCallbackHandler::MessageCallback(
    GScipMessageType, const absl::string_view message) {
  const absl::MutexLock lock(&message_mutex_);
  const std::vector<std::string> lines = message_callback_data_.Parse(message);
  if (!lines.empty()) {
    message_callback_(lines);
  }
}

}  // namespace math_opt
}  // namespace operations_research
