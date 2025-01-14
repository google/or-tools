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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef OR_TOOLS_MATH_OPT_CPP_MESSAGE_CALLBACK_H_
#define OR_TOOLS_MATH_OPT_CPP_MESSAGE_CALLBACK_H_

#include <functional>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/base/source_location.h"

namespace operations_research::math_opt {

// Callback function for messages callback sent by the solver.
//
// Each message represents a single output line from the solver, and each
// message does not contain any '\n' character in it.
//
// Thread-safety: a callback may be called concurrently from multiple
// threads. The users is expected to use proper synchronization primitives to
// deal with that.
using MessageCallback = std::function<void(const std::vector<std::string>&)>;

// Returns a message callback function that prints its output to the given
// output stream, prefixing each line with the given prefix.
//
// For each call to the returned message callback, the output_stream is flushed.
//
// Usage:
//
//   SolveArguments args;
//   args.message_callback = PrinterMessageCallback(std::cerr, "solver logs> ");
MessageCallback PrinterMessageCallback(std::ostream& output_stream = std::cout,
                                       absl::string_view prefix = "");

// Returns a message callback function that prints each line to LOG(INFO),
// prefixing each line with the given prefix.
//
// Usage:
//
//   SolveArguments args;
//   args.message_callback = InfoLoggerMessageCallback("[solver] ");
MessageCallback InfoLoggerMessageCallback(
    absl::string_view prefix = "",
    absl::SourceLocation loc = absl::SourceLocation::current());

// Returns a message callback function that prints each line to VLOG(level),
// prefixing each line with the given prefix.
//
// Usage:
//
//   SolveArguments args;
//   args.message_callback = VLoggerMessageCallback(1, "[solver] ");
MessageCallback VLoggerMessageCallback(
    int level, absl::string_view prefix = "",
    absl::SourceLocation loc = absl::SourceLocation::current());

// Returns a message callback function that aggregates all messages in the
// provided vector.
//
// Usage:
//
//   std::vector<std::string> msgs;
//   SolveArguments args;
//   args.message_callback = VectorMessageCallback(&msgs);
MessageCallback VectorMessageCallback(std::vector<std::string>* sink);

// Returns a message callback function that aggregates all messages in the
// provided RepeatedPtrField.
//
// Usage:
//
//   google::protobuf::RepeatedPtrField<std::string> msgs;
//   SolveArguments args;
//   args.message_callback = RepeatedPtrFieldMessageCallback(&msgs);
MessageCallback RepeatedPtrFieldMessageCallback(
    google::protobuf::RepeatedPtrField<std::string>* sink);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_MESSAGE_CALLBACK_H_
