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

#ifndef OR_TOOLS_MATH_OPT_CPP_COMPUTE_INFEASIBLE_SUBSYSTEM_ARGUMENTS_H_
#define OR_TOOLS_MATH_OPT_CPP_COMPUTE_INFEASIBLE_SUBSYSTEM_ARGUMENTS_H_

#include "ortools/math_opt/core/solve_interrupter.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/message_callback.h"    // IWYU pragma: export
#include "ortools/math_opt/cpp/parameters.h"          // IWYU pragma: export

namespace operations_research::math_opt {

// Arguments passed to ComputeInfeasibleSubsystem() to control the solver.
struct ComputeInfeasibleSubsystemArguments {
  // Model independent parameters, e.g. time limit.
  SolveParameters parameters;

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
  //     const ComputeInfeasibleSubsystemResult result,
  //     ComputeInfeasibleSubsystem(model, SolverType::kGurobi,
  //           { .message_callback = PrinterMessageCallback(std::cout,
  //                                                        "logs| "); });
  MessageCallback message_callback = nullptr;

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
  //   ASSIGN_OR_RETURN(const ComputeInfeasibleSubsystemResult result,
  //                    ComputeInfeasibleSubsystem(model, SolverType::kGurobi,
  //                          { .interrupter = interrupter.get() });
  //
  SolveInterrupter* interrupter = nullptr;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_COMPUTE_INFEASIBLE_SUBSYSTEM_ARGUMENTS_H_
