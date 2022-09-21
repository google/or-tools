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

#ifndef OR_TOOLS_GSCIP_GSCIP_MESSAGE_HANDLER_H_
#define OR_TOOLS_GSCIP_GSCIP_MESSAGE_HANDLER_H_

#include <functional>
#include <memory>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "scip/type_message.h"

namespace operations_research {

// Scip message handlers have three methods to log messages. This enum enables
// using the same function for all three types of messages.
enum class GScipMessageType { kInfoMessage, kDialogMessage, kWarningMessage };

// An optional callback function to redirect the SCIP logging messages.
//
// The input `message` usually ends with a newline character. This may not be
// the case though when the internal buffer of SCIP is full, in the case this
// function is called with a partial message. This will also happen when the
// last message the solve ends with an unfinished line.
using GScipMessageHandler =
    std::function<void(GScipMessageType type, absl::string_view message)>;

namespace internal {

// Functor that releases the input message handler if not nullptr. It is used as
// the deleter for the MessageHandlerPtr unique_ptr.
//
// It is a wrapper around SCIPmessagehdlrRelease.
struct ReleaseSCIPMessageHandler {
  void operator()(SCIP_MESSAGEHDLR* handler) const;
};

// A unique_ptr that releases a SCIP message handler when destroyed.
//
// Use CaptureMessageHandlerPtr() to create to capture an existing message
// handler and creates this automatic pointer that will released it on
// destruction.
using MessageHandlerPtr =
    std::unique_ptr<SCIP_MESSAGEHDLR, ReleaseSCIPMessageHandler>;

// Captures the input handler and returns a unique pointer that will release it
// when destroyed.
MessageHandlerPtr CaptureMessageHandlerPtr(SCIP_MESSAGEHDLR* const handler);

// Make a message handler for SCIP that calls the input function.
absl::StatusOr<MessageHandlerPtr> MakeSCIPMessageHandler(
    const GScipMessageHandler gscip_message_handler);

// Object to be instantiated on stack that, when destroyed, will disable the
// custom handler so that it does not call the GScipMessageHandler.
//
// It is used so that the GScipMessageHandler is not called after GScip::Solve()
// have returned, even if the handler has not been uninstalled and freed
// properly (when an error occurs).
class ScopedSCIPMessageHandlerDisabler {
 public:
  // The input handler must be the result of MakeSCIPMessageHandler(). If
  // nullptr (initially or after being reset to null), nothing will happen.
  //
  // A reference is kept to the input so the caller must make sure this input
  // MessageHandlerPtr will outlive this object.
  explicit ScopedSCIPMessageHandlerDisabler(const MessageHandlerPtr& handler);
  ~ScopedSCIPMessageHandlerDisabler();

 private:
  const MessageHandlerPtr& handler_;
};

}  // namespace internal
}  // namespace operations_research

#endif  // OR_TOOLS_GSCIP_GSCIP_MESSAGE_HANDLER_H_
