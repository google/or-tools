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

#include "ortools/math_opt/core/non_streamable_solver_init_arguments.h"

#include <memory>

namespace operations_research::math_opt {
namespace {

// Returns non_streamable->Clone() if non_streamable is not nullptr, else
// returns nullptr.
std::unique_ptr<const NonStreamableSolverInitArguments> CloneIfNotNull(
    const NonStreamableSolverInitArguments* const non_streamable) {
  return non_streamable != nullptr ? non_streamable->Clone() : nullptr;
}

}  // namespace

NonStreamableSolverInitArgumentsValue::NonStreamableSolverInitArgumentsValue(
    const NonStreamableSolverInitArguments& non_streamable)
    : non_streamable_(non_streamable.Clone()) {}

NonStreamableSolverInitArgumentsValue::NonStreamableSolverInitArgumentsValue(
    const NonStreamableSolverInitArgumentsValue& other)
    : non_streamable_(CloneIfNotNull(other.non_streamable_.get())) {}

NonStreamableSolverInitArgumentsValue&
NonStreamableSolverInitArgumentsValue::operator=(
    const NonStreamableSolverInitArgumentsValue& other) {
  // Assignment to self is possible.
  if (&other == this) {
    return *this;
  }

  non_streamable_ = CloneIfNotNull(other.non_streamable_.get());

  return *this;
}

}  // namespace operations_research::math_opt
