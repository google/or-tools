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

#include <iterator>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
#include "ortools/math_opt/solvers/gscip/gscip.pb.h"
#include "scip/def.h"
#include "scip/type_event.h"
#include "scip/type_retcode.h"
#include "scip/type_var.h"

namespace operations_research {
namespace {

using ::testing::AnyOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::status::StatusIs;

enum class CalledMethod {
  kInit,
  kExecute,
  kExit,
};

std::ostream& operator<<(std::ostream& out, const CalledMethod& method) {
  switch (method) {
    case CalledMethod::kInit:
      out << "Init()";
      break;
    case CalledMethod::kExecute:
      out << "Execute()";
      break;
    case CalledMethod::kExit:
      out << "Exit()";
      break;
    default:
      out << "[" << static_cast<int>(method) << "]()";
      break;
  }
  return out;
}

struct TestEventHandler : public GScipEventHandler {
  TestEventHandler()
      : GScipEventHandler({.name = "TestEventHandler",
                           .description = "Test event handler."}) {}

  SCIP_RETCODE Init(GScip*) override {
    calls.emplace_back(CalledMethod::kInit, SCIP_EVENTTYPE_DISABLED);

    for (const SCIP_EVENTTYPE event_type : events_to_catch_in_init) {
      SCIP_CALL(CatchEvent(event_type));
    }

    return SCIP_OKAY;
  }

  SCIP_RETCODE Execute(GScipEventHandlerContext context) override {
    calls.emplace_back(CalledMethod::kExecute, context.event_type());
    return SCIP_OKAY;
  }

  SCIP_RETCODE Exit(GScip*) override {
    calls.emplace_back(CalledMethod::kExit, SCIP_EVENTTYPE_DISABLED);
    return SCIP_OKAY;
  }

  std::vector<SCIP_EVENTTYPE> events_to_catch_in_init;

  // For each call to a virtual method of the handler, store the method and the
  // event. Use SCIP_EVENTTYPE_DISABLED when there is no associated event.
  std::vector<std::pair<CalledMethod, SCIP_EVENTTYPE>> calls;
};

TEST(GScipEventHandlerTest, WithSomeEvents) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("event_handler_test"));
  SCIP_VAR* const x =
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger).value();
  SCIP_VAR* const y =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger).value();
  GScipLinearRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 3.0;
  range.variables = {x, y};
  range.coefficients = {1.0, 1.0};
  ASSERT_OK(gscip->AddLinearConstraint(range).status());
  ASSERT_OK(gscip->SetMaximize(false));

  TestEventHandler handler;
  ASSERT_OK(handler.Register(gscip.get()));
  handler.events_to_catch_in_init = {SCIP_EVENTTYPE_SOLFOUND};
  {
    const GScipResult result = gscip->Solve().value();
    EXPECT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  }

  ASSERT_THAT(handler.calls, Not(IsEmpty()));

  EXPECT_THAT(*handler.calls.begin(),
              Pair(CalledMethod::kInit, SCIP_EVENTTYPE_DISABLED));

  // SCIP_EVENTTYPE_SOLFOUND is a mask of two "atomic" events. Only the
  // atomic events are returned from event_type(), never the mask.
  EXPECT_THAT(std::vector(handler.calls.begin() + 1, handler.calls.end() - 1),
              Each(Pair(CalledMethod::kExecute,
                        AnyOf(Eq(SCIP_EVENTTYPE_POORSOLFOUND),
                              Eq(SCIP_EVENTTYPE_BESTSOLFOUND)))));

  EXPECT_THAT(*handler.calls.rbegin(),
              Pair(CalledMethod::kExit, SCIP_EVENTTYPE_DISABLED));

  // Test that events have been correctly dropped by making sure Init() does not
  // call CatchEvent() this time.
  handler.events_to_catch_in_init.clear();
  handler.calls.clear();

  const GScipResult result = gscip->Solve().value();
  EXPECT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);

  EXPECT_THAT(handler.calls,
              ElementsAre(Pair(CalledMethod::kInit, SCIP_EVENTTYPE_DISABLED),
                          Pair(CalledMethod::kExit, SCIP_EVENTTYPE_DISABLED)));
}

TEST(GScipEventHandlerTest, NoEvents) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("event_handler_test"));
  SCIP_VAR* const x =
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger).value();
  SCIP_VAR* const y =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger).value();
  GScipLinearRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 3.0;
  range.variables = {x, y};
  range.coefficients = {1.0, 1.0};
  ASSERT_OK(gscip->AddLinearConstraint(range).status());
  ASSERT_OK(gscip->SetMaximize(false));

  TestEventHandler handler;
  ASSERT_OK(handler.Register(gscip.get()));

  const GScipResult result = gscip->Solve().value();
  EXPECT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);

  EXPECT_THAT(handler.calls,
              ElementsAre(Pair(CalledMethod::kInit, SCIP_EVENTTYPE_DISABLED),
                          Pair(CalledMethod::kExit, SCIP_EVENTTYPE_DISABLED)));
}

TEST(GScipEventHandlerTest, RegisterTwice) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("event_handler_test"));
  TestEventHandler handler;
  ASSERT_OK(handler.Register(gscip.get()));
  EXPECT_THAT(
      handler.Register(gscip.get()),
      StatusIs(absl::StatusCode::kInternal, HasSubstr("Already registered")));
}

TEST(GScipEventHandlerDeathTest, ErrorReturnedByInit) {
  struct FailingHandler : public GScipEventHandler {
    FailingHandler() : GScipEventHandler({.name = "failing handler"}) {}

    SCIP_RETCODE Init(GScip* const /*gscip*/) override { return SCIP_ERROR; }
  };

  // Returning an error in Init() will not only make the Solve() fail, but will
  // also generate an error in SCIPfree(). This function is called by the
  // destructor of Gscip. Hence if we were to only test the absl::Status
  // returned by Solve(), the test would crash when exiting at the destruction
  // of the Gscip pointer.
  //
  // As a consequence, we must include the destruction of the Gscip in an
  // EXPECT_DEATH. But we don't want to test that death, instead we want to make
  // sure Solve() returns a Status with the expected error.
  //
  // To do so, we use a marker string that is written in the output if the solve
  // fails with the expected error.
  const std::string kMarker = "FailingHandler failed as expected";

  EXPECT_DEATH(
      {
        const std::unique_ptr<GScip> gscip =
            GScip::Create("event_handler_test").value();

        FailingHandler handler;
        ASSERT_OK(handler.Register(gscip.get()));

        const absl::Status status = gscip->Solve().status();
        if (!status.ok() &&
            absl::StrContains(status.message(), "SCIP error code -8")) {
          // Write the expected marker only if we see the expected error.
          LOG(FATAL) << kMarker;
        }
      },
      kMarker);
}

TEST(GScipEventHandlerDeathTest, ErrorReturnedByExit) {
  struct FailingHandler : public GScipEventHandler {
    FailingHandler() : GScipEventHandler({.name = "failing handler"}) {}

    SCIP_RETCODE Exit(GScip* const /*gscip*/) override { return SCIP_ERROR; }
  };

  // See the comment in ErrorReturnedByInit test.
  const std::string kMarker = "FailingHandler failed as expected";

  EXPECT_DEATH(
      {
        const std::unique_ptr<GScip> gscip =
            GScip::Create("event_handler_test").value();

        FailingHandler handler;
        ASSERT_OK(handler.Register(gscip.get()));

        const absl::Status status = gscip->Solve().status();
        if (!status.ok() &&
            absl::StrContains(status.message(), "SCIP error code 0")) {
          LOG(FATAL) << kMarker;
        }
      },
      kMarker);
}

TEST(GScipEventHandlerDeathTest, ErrorReturnedByExecute) {
  struct FailingHandler : public GScipEventHandler {
    FailingHandler() : GScipEventHandler({.name = "failing handler"}) {}

    SCIP_RETCODE Init(GScip*) override {
      SCIP_CALL(CatchEvent(SCIP_EVENTTYPE_SOLFOUND));
      return SCIP_OKAY;
    }

    SCIP_RETCODE Execute(GScipEventHandlerContext) override {
      return SCIP_ERROR;
    }
  };

  // See the comment in ErrorReturnedByInit test.
  const std::string kMarker = "FailingHandler failed as expected";

  EXPECT_DEATH(
      {
        const std::unique_ptr<GScip> gscip =
            GScip::Create("event_handler_test").value();

        FailingHandler handler;
        ASSERT_OK(handler.Register(gscip.get()));

        const absl::Status status = gscip->Solve().status();
        if (!status.ok() &&
            absl::StrContains(status.message(), "SCIP error code 0")) {
          LOG(FATAL) << kMarker;
        }
      },
      kMarker);
}

}  // namespace
}  // namespace operations_research
