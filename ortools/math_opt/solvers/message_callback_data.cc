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

#include "ortools/math_opt/solvers/message_callback_data.h"

#include <cstddef>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ortools/math_opt/callback.pb.h"

namespace operations_research {
namespace math_opt {

absl::optional<CallbackDataProto> MessageCallbackData::Parse(
    const absl::string_view message) {
  CallbackDataProto data;

  // Iterate on all complete lines (lines ending with a '\n').
  absl::string_view remainder = message;
  for (std::size_t end = 0; end = remainder.find('\n'), end != remainder.npos;
       remainder = remainder.substr(end + 1)) {
    const auto line = remainder.substr(0, end);
    if (!unfinished_line_.empty()) {
      std::string& new_message = *data.add_messages();
      new_message = std::move(unfinished_line_);
      unfinished_line_.clear();
      new_message += line;
    } else {
      data.add_messages(std::string(line));
    }
  }

  // At the end of the loop, the remainder may contains the last unfinished
  // line. This could be the first line too if the entire message does not
  // contain '\n'.
  unfinished_line_ += remainder;

  // It is an error to call the user callback without any message.
  if (data.messages().empty()) {
    return absl::nullopt;
  }

  // We only need to set that if we have messages.
  data.set_event(CALLBACK_EVENT_MESSAGE);

  return data;
}

absl::optional<CallbackDataProto> MessageCallbackData::Flush() {
  if (unfinished_line_.empty()) {
    return absl::nullopt;
  }

  CallbackDataProto data;
  data.set_event(CALLBACK_EVENT_MESSAGE);
  *data.add_messages() = std::move(unfinished_line_);
  unfinished_line_.clear();
  return data;
}

}  // namespace math_opt
}  // namespace operations_research
