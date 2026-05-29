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

#ifndef ORTOOLS_BASE_STATUS_MACROS_H_
#define ORTOOLS_BASE_STATUS_MACROS_H_

#include "absl/status/status.h"           // IWYU pragma: keep
#include "ortools/base/status_builder.h"  // IWYU pragma: keep

// Run a command that returns a absl::Status.  If the called code returns an
// error status, return that status up out of this method too.
//
// Example:
//   OR_RETURN_IF_ERROR(DoThings(4));
//   OR_RETURN_IF_ERROR(DoThings(5)) << "Additional error context";
#define OR_RETURN_IF_ERROR(expr)                                     \
  switch (0)                                                         \
  case 0:                                                            \
  default:                                                           \
    if (const ::absl::Status status_macro_internal_adaptor = (expr); \
        status_macro_internal_adaptor.ok()) {                        \
    } else /* NOLINT */                                              \
      return ::ortools::StatusBuilder(status_macro_internal_adaptor)

// Executes an expression that returns an absl::StatusOr, extracting its value
// into the variable defined by lhs (or returning on error).
//
// Example: Assigning to an existing value
//   ValueType value;
//   OR_ASSIGN_OR_RETURN(value, MaybeGetValue(arg));
//   OR_ASSIGN_OR_RETURN((auto [key, val]), MaybeGetValue(arg));
//
// WARNING: OR_ASSIGN_OR_RETURN expands into multiple statements; it cannot be
// used
//  in a single statement (e.g. as the body of an if statement without {})!
#define OR_ASSIGN_OR_RETURN(lhs, rexpr)                                  \
  OR_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_(                               \
      OR_STATUS_MACROS_IMPL_CONCAT_(_status_or_value, __COUNTER__), lhs, \
      rexpr);

#define OR_STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                                            \
  OR_RETURN_IF_ERROR(statusor.status());                              \
  OR_STATUS_MACROS_IMPL_UNPARENTHESIS(lhs) = std::move(statusor).value()

// Internal helpers for macro expansion.
#define OR_STATUS_MACROS_IMPL_UNPARENTHESIS_INNER(...) \
  OR_STATUS_MACROS_IMPL_UNPARENTHESIS_INNER_(__VA_ARGS__)
#define OR_STATUS_MACROS_IMPL_UNPARENTHESIS_INNER_(...) \
  OR_STATUS_MACROS_IMPL_VAN##__VA_ARGS__
#define ISH(...) ISH __VA_ARGS__
#define OR_STATUS_MACROS_IMPL_VANISH

// If the input is parenthesized, removes the parentheses. Otherwise expands to
// the input unchanged.
#define OR_STATUS_MACROS_IMPL_UNPARENTHESIS(...) \
  OR_STATUS_MACROS_IMPL_UNPARENTHESIS_INNER(ISH __VA_ARGS__)

// Internal helper for concatenating macro values.
#define OR_STATUS_MACROS_IMPL_CONCAT_INNER_(x, y) x##y
#define OR_STATUS_MACROS_IMPL_CONCAT_(x, y) \
  OR_STATUS_MACROS_IMPL_CONCAT_INNER_(x, y)

#endif  // ORTOOLS_BASE_STATUS_MACROS_H_
