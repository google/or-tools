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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_SOLVER_MESSAGE_CALLBACK_HANDLER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_SOLVER_MESSAGE_CALLBACK_HANDLER_H_

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ortools/gscip/gscip.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/solvers/message_callback_data.h"

namespace operations_research {
namespace math_opt {

// Handler for message callbacks.
//
// The message callback is called on calls to MessageHandler() and when this
// object is destroyed (i.e. when we flush the message callback data). Doing so
// in the destructor ensures that even in case of solver failure we do call the
// message callback with the last pending messages before returning the error.
//
// Usage:
//
//   std:unique_ptr<GScipSolverMessageCallbackHandler> message_callback_handler;
//   if (message_callback != nullptr) {
//     message_callback_handler =
//       std::make_unique<GScipSolverMessageCallbackHandler>(message_callback);
//   }
//
//   GScip* gscip = ...;
//   RETURN_IF_ERROR(
//     gscip->Solve(...,
//                  message_callback_handler != nullptr
//                  ? message_callback_handler.MessageHandler()
//                  : nullptr);
//
//   // Flush the last unset message as soon as the solve is done. GScip won't
//   // call the MessageHandler() after the end of the solve so there is no need
//   // to wait here.
//   message_callback_handler.reset();
//
//   ...
class GScipSolverMessageCallbackHandler {
 public:
  // The input callback must not be null.
  explicit GScipSolverMessageCallbackHandler(
      SolverInterface::MessageCallback message_callback);

  // Calls the message callback with the last unfinished line if it exists.
  ~GScipSolverMessageCallbackHandler();

  GScipSolverMessageCallbackHandler(const GScipSolverMessageCallbackHandler&) =
      delete;
  GScipSolverMessageCallbackHandler& operator=(
      const GScipSolverMessageCallbackHandler&) = delete;

  // Returns the handler to pass to GScip::Solve().
  GScipMessageHandler MessageHandler();

 private:
  // Updates message_callback_data_ and makes the call to the message callback
  // if necessary. This method has the expected signature for a
  // GScipMessageHandler.
  void MessageCallback(GScipMessageType, absl::string_view message);

  // Mutex serializing access to message_callback_data_ and the serialization of
  // calls to the message callback.
  absl::Mutex message_mutex_;

  // The message callback; never nullptr. The message_mutex_ should be held
  // while calling it to ensure proper ordering of the messages.
  const SolverInterface::MessageCallback message_callback_
      ABSL_GUARDED_BY(message_mutex_);

  // The buffer used to generate the message events.
  MessageCallbackData message_callback_data_ ABSL_GUARDED_BY(message_mutex_);
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GSCIP_SOLVER_MESSAGE_CALLBACK_HANDLER_H_
