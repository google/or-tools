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

// Testing tools for SolveSessionProto.
#ifndef ORTOOLS_MATH_OPT_CORE_SESSION_TESTING_H_
#define ORTOOLS_MATH_OPT_CORE_SESSION_TESTING_H_

#include "ortools/math_opt/session.pb.h"

namespace operations_research::math_opt {

// Returns a sessions where timestamps have been removed.
//
// Timestamps are:
// * SolveSessionProto.initialization_(start|end)_time,
// * SolveSessionProto.Action.(start|end)_time.
[[nodiscard]] SolveSessionProto RemoveSolveSessionTimestamps(
    SolveSessionProto session);

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CORE_SESSION_TESTING_H_
