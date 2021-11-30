// Copyright 2010-2021 Google LLC
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

#include <functional>
#include <memory>
#include <utility>

#include "ortools/base/logging.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "scip/scip.h"
#include "scip/type_scip.h"
#include "ortools/gscip/gscip_message_handler.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/protoutil.h"

namespace operations_research {
namespace math_opt {

std::unique_ptr<GScipSolverCallbackHandler>
GScipSolverCallbackHandler::RegisterIfNeeded(
    const CallbackRegistrationProto& callback_registration,
    const SolverInterface::Callback callback, const absl::Time solve_start,
    SCIP* const scip) {
  // TODO(b/180617976): Don't ignore unknown callbacks.
  const absl::flat_hash_set<CallbackEventProto> events =
      EventSet(callback_registration);
  if (!events.contains(CALLBACK_EVENT_MESSAGE)) {
    return nullptr;
  }

  return absl::WrapUnique(
      new GScipSolverCallbackHandler(callback, solve_start, scip));
}

GScipSolverCallbackHandler::GScipSolverCallbackHandler(
    SolverInterface::Callback callback, absl::Time solve_start,
    SCIP* const scip)
    : callback_(std::move(ABSL_DIE_IF_NULL(callback))),
      solve_start_(std::move(solve_start)),
      scip_(ABSL_DIE_IF_NULL(scip)) {}

GScipMessageHandler GScipSolverCallbackHandler::MessageHandler() {
  return std::bind(&GScipSolverCallbackHandler::MessageCallback, this,
                   std::placeholders::_1, std::placeholders::_2);
}

absl::Status GScipSolverCallbackHandler::Flush() {
  {
    // Here we don't expect to be called in MessageCallback() at the same time
    // since GScip is not supposed to call the callback given to GScip::Solve()
    // after the end of this Solve(). But we lock anyway in case GScip does not
    // honor its contract or if the caller calls Flush() before the end of the
    // GScip::Solve().
    //
    // See MessageCallback() for the rationale of keeping the lock while calling
    // CallUserCallback().
    const absl::MutexLock lock(&message_mutex_);

    absl::optional<CallbackDataProto> data = message_callback_data_.Flush();
    if (data) {
      RETURN_IF_ERROR(util_time::EncodeGoogleApiProto(
          absl::Now() - solve_start_, data->mutable_runtime()))
          << "Failed to encode the time.";
      CallUserCallback(*data);
    }
  }

  const absl::MutexLock lock(&callback_mutex_);
  return status_;
}

void GScipSolverCallbackHandler::MessageCallback(
    const GScipMessageType type, const absl::string_view message) {
  // We hold the mutex until the end of the call of the user callback to ensure
  // proper ordering of messages. We don't expect any user action in the user
  // callback to trigger another call to MessageCallback(). If it happens to be
  // the case then we will need to make the code of CallUserCallback()
  // asynchronous to ensure that we don't end up making recursive calls to the
  // user callback.
  const absl::MutexLock lock(&message_mutex_);

  absl::optional<CallbackDataProto> data =
      message_callback_data_.Parse(message);
  if (!data) {
    return;
  }

  const absl::Status runtime_status = util_time::EncodeGoogleApiProto(
      absl::Now() - solve_start_, data->mutable_runtime());
  if (!runtime_status.ok()) {
    absl::MutexLock lock(&callback_mutex_);
    // Here we must not modify the status if it is already not OK.
    if (!status_.ok()) {
      return;
    }
    status_ = util::StatusBuilder(runtime_status)
              << "Failed to encode the time.";
    // TODO(b/182919884): Make sure it is correct to use SCIPinterruptSolve()
    // here and maybe migrate to the same architecture as the one used to
    // interrupt the solve from foreign threads..
    const auto interrupt_status = SCIP_TO_STATUS(SCIPinterruptSolve(scip_));
    LOG_IF(ERROR, !interrupt_status.ok())
        << "Failed to interrupt the solve on error: " << interrupt_status;
    return;
  }

  // Events of type CALLBACK_EVENT_MESSAGE are not expected to return anything
  // but `terminate`. Since CallUserCallback() already handles the termination,
  // we can simply ignore the returned value here.
  CallUserCallback(*data);
}

absl::optional<CallbackResultProto>
GScipSolverCallbackHandler::CallUserCallback(
    const CallbackDataProto& callback_data) {
  // We hold the lock during the call of the user callback to ensure only one
  // call execute at a time. Having multiple calls at once may be an issue when
  // the user asks for termination since it may ask for it in one call while
  // another thread is about to make its call for another callback.
  //
  // We don't expect any valid actions taken by the user is a callback to lead
  // to another callback. That said, a potential corner case could be that
  // adding a constraint lead to a message callback. If this happens, then this
  // code will need to be made more complex to deal with that. And there won't
  // be any easy solution.
  //
  // The simplest would be to have the message callbacks being delivered by a
  // background thread so that a call to MessageCallback() is never blocking on
  // the user answering to it. The issue here would be to deal with `terminate`
  // since we can't call SCIPinterruptSolve() from another thread than a SCIP
  // thread (it is not thread safe). Maybe this means that `terminate` should
  // not be callable for message callbacks, or that the termination should be
  // delayed to the next time it can be made.
  absl::MutexLock lock(&callback_mutex_);
  if (!status_.ok()) {
    return absl::nullopt;
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
    return absl::nullopt;
  }

  return *std::move(result_or);
}

}  // namespace math_opt
}  // namespace operations_research
