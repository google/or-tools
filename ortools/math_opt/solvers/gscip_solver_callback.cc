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

#include "ortools/math_opt/solvers/gscip_solver_callback.h"

#include <memory>
#include <optional>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "scip/scip.h"
#include "scip/type_scip.h"

namespace operations_research {
namespace math_opt {

std::unique_ptr<GScipSolverCallbackHandler>
GScipSolverCallbackHandler::RegisterIfNeeded(
    const CallbackRegistrationProto& callback_registration,
    const SolverInterface::Callback callback, const absl::Time solve_start,
    SCIP* const scip) {
  // TODO(b/180617976): Don't ignore unknown callbacks.
  return nullptr;
}

GScipSolverCallbackHandler::GScipSolverCallbackHandler(
    SolverInterface::Callback callback, absl::Time solve_start,
    SCIP* const scip)
    : callback_(std::move(ABSL_DIE_IF_NULL(callback))),
      solve_start_(std::move(solve_start)),
      scip_(ABSL_DIE_IF_NULL(scip)) {}

absl::Status GScipSolverCallbackHandler::Flush() {
  const absl::MutexLock lock(&callback_mutex_);
  return status_;
}

std::optional<CallbackResultProto> GScipSolverCallbackHandler::CallUserCallback(
    const CallbackDataProto& callback_data) {
  // We hold the lock during the call of the user callback to ensure only one
  // call execute at a time. Having multiple calls at once may be an issue when
  // the user asks for termination since it may ask for it in one call while
  // another thread is about to make its call for another callback.
  //
  // We don't expect any valid actions taken by the user is a callback to lead
  // to another callback.
  absl::MutexLock lock(&callback_mutex_);
  if (!status_.ok()) {
    return std::nullopt;
  }

  absl::StatusOr<CallbackResultProto> result_or = callback_(callback_data);
  status_ = result_or.status();
  if (!result_or.ok() || result_or->terminate()) {
    // TODO(b/182919884): Make sure it is correct to use SCIPinterruptSolve()
    // here and maybe migrate to the same architecture as the one used to
    // interrupt the solve from foreign threads..
    const auto interrupt_status = SCIP_TO_STATUS(SCIPinterruptSolve(scip_));
    if (!interrupt_status.ok()) {
      if (status_.ok()) {
        status_ = interrupt_status;
      } else {
        LOG(ERROR) << "Failed to interrupt the solve on error: "
                   << interrupt_status;
      }
    }
    return std::nullopt;
  }

  return *std::move(result_or);
}

}  // namespace math_opt
}  // namespace operations_research
