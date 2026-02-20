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

#ifndef ORTOOLS_MATH_OPT_CORE_BASE_SOLVER_H_
#define ORTOOLS_MATH_OPT_CORE_BASE_SOLVER_H_

#include <functional>
#include <ostream>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research::math_opt {

// The API of solvers (in-process, sub-process and streaming RPC ones).
//
// Thread-safety: methods Solve(), ComputeInfeasibleSubsystem() and Update()
// must not be called concurrently; they should immediately return with an error
// status if this happens.
//
// TODO: b/350984134 - Rename `Solver` into `InProcessSolver` and then rename
// `BaseSolver` into `Solver`.
class BaseSolver {
 public:
  // Callback function for messages callback sent by the solver.
  //
  // Each message represents a single output line from the solver, and each
  // message does not contain any '\n' character in it.
  //
  // Thread-safety: a callback may be called concurrently from multiple
  // threads. The users is expected to use proper synchronization primitives to
  // deal with that.
  using MessageCallback = std::function<void(const std::vector<std::string>&)>;

  // Callback function type for MIP/LP callbacks.
  using Callback = std::function<CallbackResultProto(const CallbackDataProto&)>;

  // Arguments used when calling Solve() to solve the problem.
  struct SolveArgs {
    SolveParametersProto parameters;
    ModelSolveParametersProto model_parameters;

    // An optional callback for messages emitted by the solver.
    //
    // When set it enables the solver messages and ignores the `enable_output`
    // in solve parameters; messages are redirected to the callback and not
    // printed on stdout/stderr/logs anymore.
    MessageCallback message_callback = nullptr;

    // Registration parameter controlling calls to user_cb.
    CallbackRegistrationProto callback_registration;

    // An optional MIP/LP callback. Only called for events registered in
    // callback_registration.
    //
    // Solve() returns an error if called without a user_cb but with some
    // non-empty callback_registration.request_registration.
    Callback user_cb = nullptr;

    // An optional interrupter that the solver can use to interrupt the solve
    // early.
    const SolveInterrupter* absl_nullable interrupter = nullptr;

    friend std::ostream& operator<<(std::ostream& out, const SolveArgs& args);
  };

  // Arguments used when calling ComputeInfeasibleSubsystem().
  struct ComputeInfeasibleSubsystemArgs {
    SolveParametersProto parameters;

    // An optional callback for messages emitted by the solver.
    //
    // When set it enables the solver messages and ignores the `enable_output`
    // in solve parameters; messages are redirected to the callback and not
    // printed on stdout/stderr/logs anymore.
    MessageCallback message_callback = nullptr;

    // An optional interrupter that the solver can use to interrupt the solve
    // early.
    const SolveInterrupter* absl_nullable interrupter = nullptr;

    friend std::ostream& operator<<(std::ostream& out,
                                    const ComputeInfeasibleSubsystemArgs& args);
  };

  BaseSolver() = default;
  BaseSolver(const BaseSolver&) = delete;
  BaseSolver& operator=(const BaseSolver&) = delete;
  virtual ~BaseSolver() = default;

  // Solves the current model (including all updates).
  virtual absl::StatusOr<SolveResultProto> Solve(
      const SolveArgs& arguments) = 0;

  // Computes an infeasible subsystem of `model` (including all updates).
  virtual absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
  ComputeInfeasibleSubsystem(
      const ComputeInfeasibleSubsystemArgs& arguments) = 0;

  // Updates the model to solve and returns true, or returns false if this
  // update is not supported by the underlying solver.
  //
  // The model_update is passed by value. Non in-process implementations will
  // move it in-place in the messages used to communicate with the other
  // process. Thus if possible, the caller should std::move() this proto to this
  // function.
  //
  // A status error will be returned if the model_update is invalid or the
  // underlying solver has an internal error.
  //
  // When this function returns false, the BaseSolver object is in a failed
  // state.
  virtual absl::StatusOr<bool> Update(ModelUpdateProto model_update) = 0;
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CORE_BASE_SOLVER_H_
