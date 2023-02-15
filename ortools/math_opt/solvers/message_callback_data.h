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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_MESSAGE_CALLBACK_DATA_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_MESSAGE_CALLBACK_DATA_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace operations_research {
namespace math_opt {

// Buffer for solvers messages that enforces the contract of MessageCallback.
//
// This contract mandates that each message is a full finished line. As a
// consequence, if the solver calls the callback with a partial last line, this
// one must not be passed immediately to MessageCallback but kept until the end
// of the line is reached (or the solve is done).
//
// To implement that this class has two methods:
//
// - Parse() that is to be called for each received message from the solver.
//
// - Flush() that must be called at the end of the solve to generate the data
//   corresponding the last message sent by the solver if it was an unfinished -
//   line.
class MessageCallbackData {
 public:
  MessageCallbackData() = default;

  MessageCallbackData(const MessageCallbackData&) = delete;
  MessageCallbackData& operator=(const MessageCallbackData&) = delete;

  // Parses the input message, returning a vector with all finished
  // lines. Returns an empty vector if the input message did not contained any
  // '\n'.
  //
  // It updates this object with the last unfinished line to use it to complete
  // the next message.
  std::vector<std::string> Parse(std::string_view message);

  // Returns a vector with the last unfinished line if it exists, else an empty
  // vector.
  std::vector<std::string> Flush();

 private:
  // The last message line not ending with '\n'.
  std::string unfinished_line_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_MESSAGE_CALLBACK_DATA_H_
