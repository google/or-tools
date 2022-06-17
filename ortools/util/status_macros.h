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

#ifndef OR_TOOLS_UTIL_STATUS_MACROS_H_
#define OR_TOOLS_UTIL_STATUS_MACROS_H_

#include "ortools/base/status_macros.h"

// Executes an expression that returns an absl::StatusOr, extracting its value
// into the variable defined by lhs or returning on error and enabling
// decorating it with the third argument.
//
// Example: Assigning to an existing value
//   ValueType value;
//   OR_ASSIGN_OR_RETURN3(value, MaybeGetValue(arg), _ << "context");
//   OR_ASSIGN_OR_RETURN3((auto [key, val]), MaybeGetValue(arg),
//                        _ << "with key " << key);
//
// WARNING: OR_ASSIGN_OR_RETURN3 expands into multiple statements; it cannot be
//  used in a single statement (e.g. as the body of an if statement without {})!
//
#define OR_ASSIGN_OR_RETURN3(lhs, rexpr, error_expression)                   \
  OR_ASSIGN_OR_RETURN3_IMPL_(                                                \
      STATUS_MACROS_IMPL_CONCAT_(_status_or_value, __COUNTER__), lhs, rexpr, \
      error_expression)

#define OR_ASSIGN_OR_RETURN3_IMPL_(statusor, lhs, rexpr, error_expression) \
  auto statusor = (rexpr);                                                 \
  if (!statusor.ok()) {                                                    \
    ::util::StatusBuilder _(std::move(statusor).status());                 \
    return (error_expression);                                             \
  }                                                                        \
  STATUS_MACROS_IMPL_UNPARENTHESIS(lhs) = std::move(statusor).value()

#endif  // OR_TOOLS_UTIL_STATUS_MACROS_H_
