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

#include "ortools/gscip/gscip_event_handler.h"

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"
#include "ortools/gscip/gscip.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "scip/def.h"
#include "scip/scip.h"
#include "scip/scip_event.h"
#include "scip/type_event.h"

struct SCIP_EventhdlrData {
  operations_research::GScipEventHandler* handler = nullptr;
  operations_research::GScip* gscip = nullptr;
};

// SCIP callback implementation

static SCIP_DECL_EVENTEXEC(EventExec) {
  VLOG(3) << "EventExec";
  CHECK_NE(scip, nullptr);
  CHECK_NE(eventhdlr, nullptr);
  CHECK_NE(event, nullptr);

  SCIP_EVENTHDLRDATA* event_handler_data = SCIPeventhdlrGetData(eventhdlr);
  CHECK_NE(event_handler_data, nullptr);
  operations_research::GScipEventHandler* handler = event_handler_data->handler;
  handler->Execute(operations_research::GScipEventHandlerContext(
      event_handler_data->gscip, SCIPeventGetType(event)));

  return SCIP_OKAY;
}

static SCIP_DECL_EVENTINIT(EventInit) {
  VLOG(3) << "EventInit";
  CHECK_NE(scip, nullptr);
  CHECK_NE(eventhdlr, nullptr);

  SCIP_EVENTHDLRDATA* event_handler_data = SCIPeventhdlrGetData(eventhdlr);
  CHECK_NE(event_handler_data, nullptr);
  operations_research::GScipEventHandler* handler = event_handler_data->handler;
  for (const SCIP_EVENTTYPE event_type :
       handler->description().events_to_catch_from_start) {
    SCIP_CALL(SCIPcatchEvent(scip, event_type, eventhdlr, nullptr, nullptr));
  }

  return SCIP_OKAY;
}

static SCIP_DECL_EVENTFREE(EventFree) {
  VLOG(3) << "EventFree";
  CHECK_NE(scip, nullptr);
  CHECK_NE(eventhdlr, nullptr);

  SCIP_EVENTHDLRDATA* event_handler_data = SCIPeventhdlrGetData(eventhdlr);
  CHECK_NE(event_handler_data, nullptr);
  delete event_handler_data;
  SCIPeventhdlrSetData(eventhdlr, nullptr);
  return SCIP_OKAY;
}

namespace operations_research {

void RegisterGScipEventHandler(GScip* scip, GScipEventHandler* handler) {
  // event_handler_data is freed in EventFree.
  SCIP_EVENTHDLRDATA* event_handler_data = new SCIP_EVENTHDLRDATA;
  event_handler_data->gscip = scip;
  event_handler_data->handler = handler;
  SCIP_EVENTHDLR* event_handler = nullptr;
  CHECK_OK(SCIP_TO_STATUS(SCIPincludeEventhdlrBasic(
      scip->scip(), &event_handler, handler->description().name.c_str(),
      handler->description().description.c_str(), EventExec,
      event_handler_data)));
  CHECK_NE(event_handler, nullptr);
  CHECK_OK(SCIP_TO_STATUS(
      SCIPsetEventhdlrInit(scip->scip(), event_handler, EventInit)));
  CHECK_OK(SCIP_TO_STATUS(
      SCIPsetEventhdlrFree(scip->scip(), event_handler, EventFree)));
}

}  // namespace operations_research
