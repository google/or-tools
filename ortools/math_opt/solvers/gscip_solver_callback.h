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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_SOLVER_CALLBACK_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_SOLVER_CALLBACK_H_

#include <memory>
#include <optional>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "scip/type_scip.h"

namespace operations_research {
namespace math_opt {

// Handler for user callbacks for GScipSolver.
//
// It deals with solve interruption when the user request it or when an error
// occurs during the call of the user callback. Any such error is returned by
// Flush().
//
// TODO(b/193537362): see if we need to share code with the handling of
// SolveInterrupter. It is likely that it could the case to make sure the
// `userinterrupt` flag is not lost. It may require sharing the same SCIP event
// handler to make sure the user callback is called first; but maybe that is not
// necessary.
class GScipSolverCallbackHandler {
 public:
  // Returns a non null handler if needed (there are supported events that we
  // register to).
  //
  // The caller will also have to use MessageHandler() when calling
  // GScip::Solve() when the result is not nullptr.
  //
  // At the end of the solve, Flush() must be called (when everything else
  // succeeded) to make the final user callback calls and return the first error
  // that occurred when calling the user callback.
  static std::unique_ptr<GScipSolverCallbackHandler> RegisterIfNeeded(
      const CallbackRegistrationProto& callback_registration,
      SolverInterface::Callback callback, absl::Time solve_start, SCIP* scip);

  GScipSolverCallbackHandler(const GScipSolverCallbackHandler&) = delete;
  GScipSolverCallbackHandler& operator=(const GScipSolverCallbackHandler&) =
      delete;

  // Makes any last pending calls and returns the first error that occurred
  // while calling the user callback. Returns OkStatus if no error has occurred.
  absl::Status Flush();

 private:
  GScipSolverCallbackHandler(SolverInterface::Callback callback,
                             absl::Time solve_start, SCIP* scip);

  // Makes a call to the user callback, updating the status_ and interrupting
  // the solve if needed (in case of error or if requested by the user).
  //
  // This function will ignores calls when status_ is not ok. It returns the
  // result of the call of the callback when the call has successfully been made
  // and the user has not requested the termination of the solve.
  //
  // This function will hold the callback_mutex_ while making the call to the
  // user callback to serialize calls.
  std::optional<CallbackResultProto> CallUserCallback(
      const CallbackDataProto& callback_data)
      ABSL_LOCKS_EXCLUDED(callback_mutex_);

  // The user callback. Should be called via CallUserCallback().
  const SolverInterface::Callback callback_;

  // Start time of the solve.
  const absl::Time solve_start_;

  // The SCIP solver, used for interruptions.
  SCIP* const scip_;

  // Mutex serializing calls to the user callback and the access to status_.
  absl::Mutex callback_mutex_;

  // The first error status returned by the user callback.
  absl::Status status_ ABSL_GUARDED_BY(callback_mutex_);
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_SOLVER_CALLBACK_H_
