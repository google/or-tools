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

#include "ortools/math_opt/core/base_solver.h"

#include <ostream>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace operations_research::math_opt {
namespace {

// Returns a string describing if the input pointer-like type is == nullptr.
template <typename Ptr>
absl::string_view IsNullString(const Ptr& ptr) {
  return ptr == nullptr ? "<null>" : "<not null>";
}

}  // namespace

std::ostream& operator<<(std::ostream& out, const BaseSolver::SolveArgs& args) {
  out << "{ parameters: <" << absl::StrCat(args.parameters)
      << ">, model_parameters: <" << absl::StrCat(args.model_parameters)
      << ">, message_callback: " << IsNullString(args.message_callback)
      << ", callback_registration: <"
      << absl::StrCat(args.callback_registration)
      << ">, user_cb: " << IsNullString(args.user_cb)
      << ", interrupter: " << args.interrupter << " }";
  return out;
}

std::ostream& operator<<(
    std::ostream& out, const BaseSolver::ComputeInfeasibleSubsystemArgs& args) {
  out << "{ parameters: <" << absl::StrCat(args.parameters)
      << ">, message_callback: " << IsNullString(args.message_callback)
      << ", interrupter: " << args.interrupter << " }";
  return out;
}

}  // namespace operations_research::math_opt
