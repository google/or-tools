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
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ortools/math_opt/core/solver_interface.h"

namespace operations_research::math_opt {

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
  // lines. Returns an empty vector if the input message did not contain any
  // '\n'.
  //
  // It updates this object with the last unfinished line to use it to complete
  // the next message.
  std::vector<std::string> Parse(absl::string_view message);

  // Returns a vector with the last unfinished line if it exists, else an empty
  // vector.
  std::vector<std::string> Flush();

 private:
  // The last message line not ending with '\n'.
  std::string unfinished_line_;
};

// Buffers callback data into lines using MessageCallbackData and invokes the
// callback as new lines are ready.
//
// In MathOpt, typically used for solvers that provide a callback with the
// solver logs where the logs contain `\n` characters and messages may be both
// less than a complete line or multiple lines. Recommended use:
//   * Register a callback with the underlying solver to get its logs. In the
//     callback, when given a log string, call OnMessage() on it.
//   * Run the solver's Solve() function.
//   * Unregister the callback with the underlying solver.
//   * Call Flush() to ensure any final incomplete lines are sent.
//
// If initialized with a nullptr for the user callback, all functions on this
// class have no effect.
//
// This class is threadsafe if the input callback is also threadsafe.
class BufferedMessageCallback {
 public:
  explicit BufferedMessageCallback(
      SolverInterface::MessageCallback user_message_callback);

  // If false, incoming messages are ignored and OnMessage() and Flush() have no
  // effect.
  bool has_user_message_callback() const {
    return user_message_callback_ != nullptr;
  }

  // Appends `message` to the buffer, then invokes the callback once on all
  // newly complete lines and removes those lines from the buffer. In
  // particular, the callback is not invoked if message does not contain any
  // `\n`.
  void OnMessage(absl::string_view message);

  // If the buffer has any pending message, sends it to the callback. This
  // function has no effect if called when the buffer is empty. Calling this
  // function when the buffer is non-empty before the stream of logs is complete
  // will result in the user getting extra line breaks.
  void Flush();

 private:
  SolverInterface::MessageCallback user_message_callback_;
  absl::Mutex mutex_;
  MessageCallbackData message_callback_data_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_MESSAGE_CALLBACK_DATA_H_
