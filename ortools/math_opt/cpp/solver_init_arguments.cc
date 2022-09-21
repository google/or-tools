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

#include "ortools/math_opt/cpp/solver_init_arguments.h"

#include <memory>
#include <utility>

namespace operations_research::math_opt {

SolverInitArguments::SolverInitArguments(
    StreamableSolverInitArguments streamable)
    : streamable(std::move(streamable)) {}

SolverInitArguments::SolverInitArguments(
    const NonStreamableSolverInitArguments& non_streamable)
    : non_streamable(non_streamable.Clone()) {}

SolverInitArguments::SolverInitArguments(
    StreamableSolverInitArguments streamable,
    const NonStreamableSolverInitArguments& non_streamable)
    : streamable(std::move(streamable)),
      non_streamable(non_streamable.Clone()) {}

SolverInitArguments::SolverInitArguments(const SolverInitArguments& other)
    : streamable(other.streamable),
      non_streamable(other.non_streamable != nullptr
                         ? other.non_streamable->Clone()
                         : nullptr) {}

SolverInitArguments& SolverInitArguments::operator=(
    const SolverInitArguments& other) {
  // Assignment to self is possible.
  if (&other == this) {
    return *this;
  }

  streamable = other.streamable;
  non_streamable =
      other.non_streamable != nullptr ? other.non_streamable->Clone() : nullptr;

  return *this;
}

}  // namespace operations_research::math_opt
