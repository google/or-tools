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

#ifndef ORTOOLS_MATH_OPT_CPP_EXECUTOR_EXECUTOR_INIT_ARGS_H_
#define ORTOOLS_MATH_OPT_CPP_EXECUTOR_EXECUTOR_INIT_ARGS_H_

#include <optional>

#include "absl/base/nullability.h"
#include "absl/time/time.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research::math_opt {

// Similar to SolverInitArguments, but for the executor APIs.
struct ExecutorSolverInitArguments {
  // An explicit deadline for the RPC call.
  //
  // For stubby users, see remote_solve.h for a complete explanation on how this
  // interacts with any propagated deadline from base::Context.
  std::optional<absl::Duration> explicit_deadline;

  // Arguments controlling the initialization of the solver.
  math_opt::StreamableSolverInitArguments streamable;

  // End the solve as soon as possible. In contrast to interrupter,
  // absl::Cancelled error might be returned. For incremental solver, may be in
  // an unusable state after.
  const SolveInterrupter* absl_nullable canceller = nullptr;

  // If true, the names of variables and constraints are discarded before
  // sending them to the solver. This is particularly useful for models near the
  // two gigabyte limit in proto form (all models solved by remote solve must
  // be serialized).
  bool remove_names = false;

  // Hints on resources requested for the solve.
  math_opt::SolverResources resources;
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CPP_EXECUTOR_EXECUTOR_INIT_ARGS_H_
