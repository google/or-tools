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

// Provides a safe C++ interface for SCIP event handlers, which are described at
// https://www.scipopt.org/doc/html/EVENT.php.
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
  // https://www.scipopt.org/doc/html/EVENT.php#EVENTHDLR_PROPERTIES for
  // details.

  // See CONSHDLR_NAME in SCIP documentation above.
  std::string name;

  // See CONSHDLR_DESC in SCIP documentation above.
  std::string description;
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

  // TODO(user): Support additional properties that might need to be
  // queried within an event handler.

 private:
  GScip* const gscip_;
  const SCIP_EVENTTYPE event_type_;
};

// Inherit from this to implement the callback and override Init() to call
// CatchEvent() for the events you want to listen to.
//
// Usage:
//
//   class MyHandler : public GScipEventHandler {
//    public:
//     MyHandler()
//       : GScipEventHandler({
//           .name =  "my handler", .description = "something"}) {}
//
//     SCIP_RETCODE Init(GScip* const gscip) override {
//       SCIP_CALL(CatchEvent(SCIP_EVENTTYPE_SOLFOUND));
//       return SCIP_OKAY;
//     }
//
//     SCIP_RETCODE Execute(const GScipEventHandlerContext context) override {
//       ...
//       return SCIP_OKAY;
//     }
//   };
//
//   std::unique_ptr<GScip> gscip = ...;
//
//   MyHandler handler;
//   handler.Register(gscip.get());
//
class GScipEventHandler {
 public:
  explicit GScipEventHandler(const GScipEventHandlerDescription& description)
      : description_(description) {}

  virtual ~GScipEventHandler() = default;

  // Registers this event handler to the given GScip.
  //
  // This function CHECKs that this handler has not been previously registered.
  //
  // The given GScip won't own this handler but will keep a pointer to it that
  // will be used during the solve.
  void Register(GScip* gscip);

  // Initialization of the event handler. Called after the problem was
  // transformed.
  //
  // The implementation should use the CatchEvent() method to register to global
  // events.
  //
  // Return SCIP_OKAY, or use SCIP_CALL macro to wraps SCIP calls to properly
  // propagate errors.
  virtual SCIP_RETCODE Init(GScip* gscip) { return SCIP_OKAY; }

  // Called when a caught event is emitted.
  //
  // Return SCIP_OKAY, or use SCIP_CALL macro to wraps SCIP calls to properly
  // propagate errors.
  virtual SCIP_RETCODE Execute(GScipEventHandlerContext context) {
    return SCIP_OKAY;
  }

  // Deinitialization of the event handler.
  //
  // Called before the transformed problem is freed and after all the events
  // specified in CatchEvent() have been dropped (thus there is no need to
  // implement this function to drop these events since this would have already
  // been done).
  //
  // Return SCIP_OKAY, or use SCIP_CALL macro to wraps SCIP calls to properly
  // propagate errors.
  virtual SCIP_RETCODE Exit(GScip* gscip) { return SCIP_OKAY; }

 protected:
  // Catches a global event (i.e. not a variable or row depedent one) based on
  // the input event_type mask.
  //
  // This method must only be called after the problem is transformed; typically
  // it is called in the Init() virtual method.
  //
  // Caught events will be automatically dropped when the handler will be called
  // on EXIT (before calling the corresponding Exit() member function).
  //
  // See scip/type_event.h for the list of possible events. This function
  // corresponds to SCIPcatchEvent().
  //
  // TODO(user): Support Var and Row events.
  //
  // TODO(user): Support registering events in the EVENTINITSOL
  // callback, which would cause them to be trapped only after presolve.
  SCIP_RETCODE CatchEvent(SCIP_EVENTTYPE event_type);

 private:
  struct CaughtEvent {
    CaughtEvent(const SCIP_EVENTTYPE event_type, const int filter_pos)
        : event_type(event_type), filter_pos(filter_pos) {}

    // The event_type mask for this catch.
    SCIP_EVENTTYPE event_type;

    // The key used by SCIP to identify this catch with SCIPdropEvent(). Using
    // this key prevents SCIP from having to do a look up to find the catch and
    // helps when there are duplicates.
    //
    // It is the index of the data associated to the catch in the array SCIP
    // uses as storage (this index is stable, even after other catch added
    // previously are removed, since SCIP maintains a free-list of removed items
    // instead of renumbering all elements).
    int filter_pos;
  };

  // Calls SCIPdropEvent() for all events in caught_events_ and clear this
  // collection.
  //
  // This is not a member function since it needs to be visible to the SCIP Exit
  // callback function.
  friend SCIP_RETCODE DropAllEvents(GScipEventHandler& handler);

  const GScipEventHandlerDescription description_;

  // Pointer to GScip set by Register();
  GScip* gscip_ = nullptr;

  // Pointer to the event handler registered on SCIP.
  SCIP_EVENTHDLR* event_handler_ = nullptr;

  // Caught events via CatchEvent().
  std::vector<CaughtEvent> caught_events_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_GSCIP_GSCIP_EVENT_HANDLER_H_
