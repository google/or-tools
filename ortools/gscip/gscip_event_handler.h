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

// Provides a safe C++ interface for SCIP event handlers, which are described at
// https://www.scipopt.org/doc-7.0.1/html/EVENT.php.
#ifndef OR_TOOLS_GSCIP_GSCIP_EVENT_HANDLER_H_
#define OR_TOOLS_GSCIP_GSCIP_EVENT_HANDLER_H_

#include <string>
#include <vector>

#include "ortools/gscip/gscip.h"
#include "scip/type_event.h"

namespace operations_research {

struct GScipEventHandlerDescription {
  // For the first two members below, the SCIP constraint handler
  // property name is provided. See
  // https://www.scipopt.org/doc-7.0.1/html/EVENT.php#EVENTHDLR_PROPERTIES for
  // details.

  // See CONSHDLR_NAME in SCIP documentation above.
  std::string name;

  // See CONSHDLR_DESC in SCIP documentation above.
  std::string description;

  // These are "general" events to be added via SCIPcatchEvent in the EVENTINIT
  // callback, not "Var" or "Row" events. See scip/type_event.h for the list of
  // possible events.
  std::vector<SCIP_EVENTTYPE> events_to_catch_from_start;

  // TODO(user,user): Support Var and Row events.

  // TODO(user,user): Support registering events in the EVENTINITSOL
  // callback, which would cause them to be trapped only after presolve.
};

// Passed by value. This is a lightweight interface to the callback context and
// the underlying problem. It's preferred for callbacks to use the context
// object to query information rather than using the raw SCIP pointer, because
// the context object can be set up to do this in a safe way.
class GScipEventHandlerContext {
 public:
  GScipEventHandlerContext(GScip* gscip, SCIP_EVENTTYPE event_type)
      : gscip_(gscip), event_type_(event_type) {}

  GScip* gscip() const { return gscip_; }

  // This is always an atomic event type, not a mask (i.e., one of the events
  // defined as a bitwise OR).
  SCIP_EVENTTYPE event_type() const { return event_type_; }

  // TODO(user,user): Support additional properties that might need to be
  // queried within an event handler.

 private:
  GScip* gscip_;
  SCIP_EVENTTYPE event_type_;
};

// Inherit from this to implement the callback.
class GScipEventHandler {
 public:
  explicit GScipEventHandler(const GScipEventHandlerDescription& description)
      : description_(description) {}
  virtual ~GScipEventHandler() {}
  const GScipEventHandlerDescription& description() const {
    return description_;
  }

  virtual void Execute(GScipEventHandlerContext context) = 0;

 private:
  GScipEventHandlerDescription description_;
};

// Handler is not owned but held.
void RegisterGScipEventHandler(GScip* scip, GScipEventHandler* handler);

}  // namespace operations_research

#endif  // OR_TOOLS_GSCIP_GSCIP_EVENT_HANDLER_H_
