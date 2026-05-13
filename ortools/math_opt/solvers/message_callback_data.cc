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

#include "ortools/math_opt/solvers/message_callback_data.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace operations_research {
namespace math_opt {

std::vector<std::string> MessageCallbackData::Parse(
    const absl::string_view message) {
  std::vector<std::string> strings;

  // Iterate on all complete lines (lines ending with a '\n').
  absl::string_view remainder = message;
  for (std::size_t end = 0; end = remainder.find('\n'), end != remainder.npos;
       remainder = remainder.substr(end + 1)) {
    const auto line = remainder.substr(0, end);
    if (!unfinished_line_.empty()) {
      std::string new_message = std::move(unfinished_line_);
      unfinished_line_.clear();
      absl::StrAppend(&new_message, line);
      strings.push_back(std::move(new_message));
    } else {
      strings.emplace_back(line);
    }
  }

  // At the end of the loop, the remainder may contain the last unfinished line.
  // This could be the first line too if the entire message does not contain
  // '\n'.
  absl::StrAppend(&unfinished_line_, remainder);

  return strings;
}

std::vector<std::string> MessageCallbackData::Flush() {
  if (unfinished_line_.empty()) {
    return {};
  }

  std::vector<std::string> strings = {std::move(unfinished_line_)};
  unfinished_line_.clear();
  return strings;
}

BufferedMessageCallback::BufferedMessageCallback(
    SolverInterface::MessageCallback user_message_callback)
    : user_message_callback_(std::move(user_message_callback)) {}

void BufferedMessageCallback::OnMessage(absl::string_view message) {
  if (!has_user_message_callback()) {
    return;
  }
  std::vector<std::string> messages;
  {
    absl::MutexLock lock(&mutex_);
    messages = message_callback_data_.Parse(message);
  }
  // Do not hold lock during callback to user code.
  if (!messages.empty()) {
    user_message_callback_(messages);
  }
}

void BufferedMessageCallback::Flush() {
  if (!has_user_message_callback()) {
    return;
  }
  std::vector<std::string> messages;
  {
    absl::MutexLock lock(&mutex_);
    messages = message_callback_data_.Flush();
  }
  // Do not hold lock during callback to user code.
  if (!messages.empty()) {
    user_message_callback_(messages);
  }
}

}  // namespace math_opt
}  // namespace operations_research
