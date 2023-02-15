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

#ifndef OR_TOOLS_MATH_OPT_CORE_SOLVER_DEBUG_H_
#define OR_TOOLS_MATH_OPT_CORE_SOLVER_DEBUG_H_

#include <atomic>
#include <cstdint>

namespace operations_research {
namespace math_opt {
namespace internal {

// The number of Solver instances that currently exist.
//
// This variable is intended to be used by MathOpt unit tests in other languages
// to test the proper garbage collection. It should never be used in any other
// context.
extern std::atomic<int64_t> debug_num_solver;

}  // namespace internal
}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_SOLVER_DEBUG_H_
