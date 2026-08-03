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

#include "ortools/math_opt/core/java/solver.h"

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_join.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/util/java/java_swig_solve_interrupter.h"

namespace operations_research::math_opt {

absl::StatusOr<SolveResultProto> cppSolve(
    const ModelProto& model, const SolverTypeProto solverType,
    const SolveParametersProto& solveParameters,
    const ModelSolveParametersProto& modelSolveParameters,
    CppMessageCallback* const messageCallback,
    const CallbackRegistrationProto& cbRegistration,
    CppSolveCallback* const callback,
    JavaSwigSolveInterrupter* const interrupter) {
  Solver::InitArgs init;
  Solver::SolveArgs solve_args;
  solve_args.parameters = solveParameters;
  solve_args.model_parameters = modelSolveParameters;
  solve_args.callback_registration = cbRegistration;
  if (interrupter != nullptr) {
    solve_args.interrupter = &interrupter->interrupter();
  }
  if (messageCallback != nullptr) {
    solve_args.message_callback =
        [messageCallback](const std::vector<std::string>& messages) {
          messageCallback->onMessage(absl::StrJoin(messages, "\n"));
        };
  }
  if (callback != nullptr) {
    solve_args.user_cb = [callback](const CallbackDataProto& cb_data) {
      return callback->onEvent(cb_data);
    };
  }
  return Solver::NonIncrementalSolve(model, solverType, init, solve_args);
}

}  // namespace operations_research::math_opt
