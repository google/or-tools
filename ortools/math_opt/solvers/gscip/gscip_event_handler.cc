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

#include "ortools/math_opt/solvers/gscip/gscip_event_handler.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
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

absl::Status GScipEventHandler::Register(GScip* const gscip) {
  if (gscip_ != nullptr || event_handler_ != nullptr) {
    return absl::InternalError("Already registered");
  }

  gscip_ = gscip;

  // event_handler_data is freed in EventFree.
  SCIP_EVENTHDLRDATA* const event_handler_data = new SCIP_EVENTHDLRDATA;
  event_handler_data->gscip = gscip;
  event_handler_data->handler = this;

  RETURN_IF_SCIP_ERROR(SCIPincludeEventhdlrBasic(
      gscip->scip(), &event_handler_, description_.name.c_str(),
      description_.description.c_str(), EventExec, event_handler_data));
  if (event_handler_ == nullptr) {
    // This is only defensive: SCIP should return a SCIP error above instead.
    return absl::InternalError("SCIP failed to create event handler");
  }

  RETURN_IF_SCIP_ERROR(
      SCIPsetEventhdlrInit(gscip->scip(), event_handler_, EventInit));
  RETURN_IF_SCIP_ERROR(
      SCIPsetEventhdlrExit(gscip->scip(), event_handler_, EventExit));
  RETURN_IF_SCIP_ERROR(
      SCIPsetEventhdlrFree(gscip->scip(), event_handler_, EventFree));
  return absl::OkStatus();
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
