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

#include <string>
#include <vector>

#include "absl/status/status.h"
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

  SCIP_EVENTHDLRDATA* const event_handler_data =
      SCIPeventhdlrGetData(eventhdlr);
  CHECK_NE(event_handler_data, nullptr);

  return event_handler_data->handler->Execute(
      {event_handler_data->gscip, SCIPeventGetType(event)});
}

static SCIP_DECL_EVENTINIT(EventInit) {
  VLOG(3) << "EventInit";
  CHECK_NE(scip, nullptr);
  CHECK_NE(eventhdlr, nullptr);

  SCIP_EVENTHDLRDATA* const event_handler_data =
      SCIPeventhdlrGetData(eventhdlr);
  CHECK_NE(event_handler_data, nullptr);

  return event_handler_data->handler->Init(event_handler_data->gscip);
}

static SCIP_DECL_EVENTEXIT(EventExit) {
  VLOG(3) << "EventExit";
  CHECK_NE(scip, nullptr);
  CHECK_NE(eventhdlr, nullptr);

  SCIP_EVENTHDLRDATA* const event_handler_data =
      SCIPeventhdlrGetData(eventhdlr);
  CHECK_NE(event_handler_data, nullptr);

  SCIP_CALL(DropAllEvents(*event_handler_data->handler));

  return event_handler_data->handler->Exit(event_handler_data->gscip);
}

static SCIP_DECL_EVENTFREE(EventFree) {
  VLOG(3) << "EventFree";
  CHECK_NE(scip, nullptr);
  CHECK_NE(eventhdlr, nullptr);

  SCIP_EVENTHDLRDATA* const event_handler_data =
      SCIPeventhdlrGetData(eventhdlr);
  CHECK_NE(event_handler_data, nullptr);

  delete event_handler_data;
  SCIPeventhdlrSetData(eventhdlr, nullptr);

  return SCIP_OKAY;
}

namespace operations_research {

void GScipEventHandler::Register(GScip* const gscip) {
  CHECK_EQ(gscip_, nullptr) << "Already registered.";
  CHECK_EQ(event_handler_, nullptr);

  gscip_ = gscip;

  // event_handler_data is freed in EventFree.
  SCIP_EVENTHDLRDATA* const event_handler_data = new SCIP_EVENTHDLRDATA;
  event_handler_data->gscip = gscip;
  event_handler_data->handler = this;

  CHECK_OK(SCIP_TO_STATUS(SCIPincludeEventhdlrBasic(
      gscip->scip(), &event_handler_, description_.name.c_str(),
      description_.description.c_str(), EventExec, event_handler_data)));
  CHECK_NE(event_handler_, nullptr);

  CHECK_OK(SCIP_TO_STATUS(
      SCIPsetEventhdlrInit(gscip->scip(), event_handler_, EventInit)));
  CHECK_OK(SCIP_TO_STATUS(
      SCIPsetEventhdlrExit(gscip->scip(), event_handler_, EventExit)));
  CHECK_OK(SCIP_TO_STATUS(
      SCIPsetEventhdlrFree(gscip->scip(), event_handler_, EventFree)));
}

SCIP_RETCODE GScipEventHandler::CatchEvent(const SCIP_EVENTTYPE event_type) {
  int filter_pos = -1;

  SCIP_CALL(SCIPcatchEvent(gscip_->scip(), event_type, event_handler_,
                           /*eventdata=*/nullptr, &filter_pos));
  CHECK_GE(filter_pos, 0);

  caught_events_.emplace_back(event_type, filter_pos);

  return SCIP_OKAY;
}

SCIP_RETCODE DropAllEvents(GScipEventHandler& handler) {
  for (const GScipEventHandler::CaughtEvent& caught_event :
       handler.caught_events_) {
    SCIP_CALL(SCIPdropEvent(handler.gscip_->scip(), caught_event.event_type,
                            handler.event_handler_,
                            /*eventdata=*/nullptr, caught_event.filter_pos));
  }

  handler.caught_events_.clear();

  return SCIP_OKAY;
}

}  // namespace operations_research
