// Copyright 2010-2022 Google LLC
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
#include <string_view>
#include <utility>
#include <vector>

namespace operations_research {
namespace math_opt {

std::vector<std::string> MessageCallbackData::Parse(
    const std::string_view message) {
  std::vector<std::string> strings;

  // Iterate on all complete lines (lines ending with a '\n').
  std::string_view remainder = message;
  for (std::size_t end = 0; end = remainder.find('\n'), end != remainder.npos;
       remainder = remainder.substr(end + 1)) {
    const auto line = remainder.substr(0, end);
    if (!unfinished_line_.empty()) {
      std::string new_message = std::move(unfinished_line_);
      unfinished_line_.clear();
      new_message += line;
      strings.push_back(std::move(new_message));
    } else {
      strings.emplace_back(line);
    }
  }

  // At the end of the loop, the remainder may contains the last unfinished
  // line. This could be the first line too if the entire message does not
  // contain '\n'.
  unfinished_line_ += remainder;

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

}  // namespace math_opt
}  // namespace operations_research
