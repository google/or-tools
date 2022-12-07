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

#ifndef OR_TOOLS_MATH_OPT_CPP_SOLVE_ARGUMENTS_H_
#define OR_TOOLS_MATH_OPT_CPP_SOLVE_ARGUMENTS_H_

#include "absl/status/status.h"
#include "ortools/math_opt/core/solve_interrupter.h"      // IWYU pragma: export
#include "ortools/math_opt/cpp/callback.h"                // IWYU pragma: export
#include "ortools/math_opt/cpp/message_callback.h"        // IWYU pragma: export
#include "ortools/math_opt/cpp/model_solve_parameters.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/parameters.h"              // IWYU pragma: export
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {

// Arguments passed to Solve() and IncrementalSolver::Solve() to control the
// solve.
struct SolveArguments {
  // Model independent parameters, e.g. time limit.
  SolveParameters parameters;

  // Model dependent parameters, e.g. solution hint.
  ModelSolveParameters model_parameters;

  // An optional callback for messages emitted by the solver.
  //
  // When set it enables the solver messages and ignores the `enable_output` in
  // solve parameters; messages are redirected to the callback and not printed
  // on stdout/stderr/logs anymore.
  //
  // See PrinterMessageCallback() for logging to stdout/stderr.
  //
  // Usage:
  //
  //   // To print messages to stdout with a prefix.
  //   ASSIGN_OR_RETURN(
  //     const SolveResult result,
  //     Solve(model, SOLVER_TYPE_GLOP,
  //           { .message_callback = PrinterMessageCallback(std::cout,
  //                                                        "logs| "); });
  MessageCallback message_callback = nullptr;

  // Callback registration parameters. Usually `callback` should also be set
  // when these parameters are modified.
  CallbackRegistration callback_registration;

  // The optional callback for LP/MIP events.
  //
  // The `callback_registration` parameters have to be set, in particular
  // `callback_registration.events`.
  //
  // See callback.h file comment for documentation on callbacks.
  Callback callback = nullptr;

  // An optional interrupter that the solver can use to interrupt the solve
  // early.
  //
  // Usage:
  //   auto interrupter = std::make_shared<SolveInterrupter>();
  //
  //   // Use another thread to trigger the interrupter.
  //   RunInOtherThread([interrupter](){
  //     ... wait for something that should interrupt the solve ...
  //     interrupter->Interrupt();
  //   });
  //
  //   ASSIGN_OR_RETURN(const SolveResult result,
  //                    Solve(model, SOLVER_TYPE_GLOP,
  //                          { .interrupter = interrupter.get() });
  //
  SolveInterrupter* interrupter = nullptr;

  // Returns a failure if the referenced variables and constraints don't belong
  // to the input expected_storage (which must not be nullptr). Also returns a
  // failure if callback events are registered but no callback is provided.
  absl::Status CheckModelStorageAndCallback(
      const ModelStorage* expected_storage) const;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_SOLVE_ARGUMENTS_H_
