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

#ifndef ORTOOLS_MATH_OPT_CORE_MESSAGE_CALLBACK_TESTING_H_
#define ORTOOLS_MATH_OPT_CORE_MESSAGE_CALLBACK_TESTING_H_

#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "ortools/math_opt/core/base_solver.h"

namespace operations_research::math_opt {

// Returns a MessageCallback that keeps track of the messages received on each
// call.
//
// Contrary to the message callback returned by VectorMessageCallback(), which
// accumulate messages for each call in a single std::vector<std::string>, this
// testing callback stores messages for each call in a sub-vector. This enables
// testing how messages were grouped.
//
// The current content of `calls` is not removed.
BaseSolver::MessageCallback TestingMessageCallback(
    std::vector<std::vector<std::string>>* absl_nonnull const calls);

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CORE_MESSAGE_CALLBACK_TESTING_H_
