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

#include "ortools/gscip/gscip_message_handler.h"

#include <stdio.h>

#include <atomic>
#include <memory>

#include "absl/status/statusor.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "scip/pub_message.h"
#include "scip/type_message.h"

// This is an incomplete SCIP struct meant to be fully defined by SCIP users.
struct SCIP_MessagehdlrData {
  explicit SCIP_MessagehdlrData(
      const operations_research::GScipMessageHandler gscip_message_handler)
      : gscip_message_handler(ABSL_DIE_IF_NULL(gscip_message_handler)) {}

  // This will be set to true by ScopedSCIPMessageHandlerDisabler when
  // GScip::Solve() returns. We use an atomic here since SCIP can be
  // multi-threaded.
  std::atomic<bool> disabled = false;

  const operations_research::GScipMessageHandler gscip_message_handler;
};

namespace operations_research {
namespace internal {
namespace {

// Equivalent to SCIP_DECL_MESSAGEHDLRFREE(SCIPMessageHandlerFree).
SCIP_RETCODE SCIPMessageHandlerFree(SCIP_MESSAGEHDLR* const handler) {
  SCIP_MessagehdlrData* const data = SCIPmessagehdlrGetData(handler);
  delete data;
  CHECK_EQ(SCIPmessagehdlrSetData(handler, nullptr), SCIP_OKAY);
  return SCIP_OKAY;
}

// Shared function used by all three implementations below.
void SCIPMessageHandlerPrinter(const GScipMessageType message_type,
                               SCIP_MESSAGEHDLR* const handler,
                               const char* const message) {
  // Contrary to SCIP's documentation, the code of handleMessage() in
  // src/scip/message.c never calls the handler functions when its input `msg`
  // is NULL.
  CHECK(message != nullptr);

  SCIP_MessagehdlrData* const data = SCIPmessagehdlrGetData(handler);
  if (data->disabled) {
    LOG(ERROR) << "Unexpected SCIP message: " << message;
    return;
  }

  // We ignore empty messages. The implementation of handleMessage() in
  // src/scip/message.c calls this function with an empty message when the
  // handler's buffer is flushed but was empty.
  //
  // This typically happenbs when the handler is freed since messagehdlrFree()
  // calls messagePrintWarning(), messagePrintDialog() and messagePrintInfo()
  // will NULL message just before calling the handler free function (which is
  // SCIPMessageHandlerFree() above). So this function is usually called three
  // times when the custom handler is freed. There is no need to redirect these
  // useless calls to the gscip_message_handler user function.
  //
  // Note that we do this test only in the `!disabled` branch since we want to
  // detect cases of unexpected calls even with empty messages in the other
  // branch.
  if (message[0] == '\0') {
    return;
  }
  data->gscip_message_handler(GScipMessageType::kInfoMessage, message);
}

// Equivalent to SCIP_DECL_MESSAGEINFO(SCIPMessageHandlerInfo).
void SCIPMessageHandlerInfo(SCIP_MESSAGEHDLR* const handler, FILE*,
                            const char* const message) {
  SCIPMessageHandlerPrinter(GScipMessageType::kInfoMessage, handler, message);
}

// Equivalent to SCIP_DECL_MESSAGEDIALOG(SCIPMessageHandlerDialog).
void SCIPMessageHandlerDialog(SCIP_MESSAGEHDLR* const handler, FILE*,
                              const char* const message) {
  SCIPMessageHandlerPrinter(GScipMessageType::kDialogMessage, handler, message);
}

// Equivalent to SCIP_DECL_MESSAGEWARNING(SCIPMessageHandlerWarning).
void SCIPMessageHandlerWarning(SCIP_MESSAGEHDLR* const handler, FILE*,
                               const char* const message) {
  SCIPMessageHandlerPrinter(GScipMessageType::kWarningMessage, handler,
                            message);
}

}  // namespace

void ReleaseSCIPMessageHandler::operator()(SCIP_MESSAGEHDLR* handler) const {
  if (handler != nullptr) {
    CHECK_EQ(SCIPmessagehdlrRelease(&handler), SCIP_OKAY);
  }
}

MessageHandlerPtr CaptureMessageHandlerPtr(SCIP_MESSAGEHDLR* const handler) {
  if (handler != nullptr) {
    SCIPmessagehdlrCapture(handler);
  }
  return MessageHandlerPtr(handler);
}

absl::StatusOr<MessageHandlerPtr> MakeSCIPMessageHandler(
    const GScipMessageHandler gscip_message_handler) {
  // We use a unique_ptr here to make sure we delete the data if
  // SCIPmessagehdlrCreate() fails.
  auto data = std::make_unique<SCIP_MessagehdlrData>(gscip_message_handler);
  SCIP_MESSAGEHDLR* message_handler = nullptr;
  RETURN_IF_SCIP_ERROR(
      SCIPmessagehdlrCreate(&message_handler,
                            /*bufferedoutput=*/true,
                            /*filename=*/nullptr,
                            /*quiet=*/false,
                            /*messagewarning=*/SCIPMessageHandlerWarning,
                            /*messagedialog=*/SCIPMessageHandlerDialog,
                            /*messageinfo=*/SCIPMessageHandlerInfo,
                            /*messagehdlrfree=*/SCIPMessageHandlerFree,
                            /*messagehdlrdata=*/data.get()));

  // SCIPmessagehdlrCreate() succeeded, we disable the deletion of the data by
  // the unique_ptr since the ownership has been transferred to SCIP.
  data.release();

  return MessageHandlerPtr(message_handler);
}

ScopedSCIPMessageHandlerDisabler::ScopedSCIPMessageHandlerDisabler(
    const MessageHandlerPtr& handler)
    : handler_(handler) {}

ScopedSCIPMessageHandlerDisabler::~ScopedSCIPMessageHandlerDisabler() {
  if (handler_ != nullptr) {
    // Note that SCIPmessagehdlrGetData is a macro in optimized builds and a
    // function in debug ones. Hence here we assign the result to a variable
    // instead of chaining the calls.
    SCIP_MessagehdlrData* const data = SCIPmessagehdlrGetData(handler_.get());
    data->disabled = true;
  }
}

}  // namespace internal
}  // namespace operations_research
