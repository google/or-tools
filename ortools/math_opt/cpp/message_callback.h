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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

#ifndef OR_TOOLS_MATH_OPT_CPP_MESSAGE_CALLBACK_H_
#define OR_TOOLS_MATH_OPT_CPP_MESSAGE_CALLBACK_H_

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
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

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_MESSAGE_CALLBACK_H_
