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

#include "ortools/math_opt/cpp/message_callback.h"

#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/solver_interface_mock.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "testing/base/public/mock-log.h"

ABSL_DECLARE_FLAG(std::string, vmodule);

namespace operations_research::math_opt {
namespace {

using ::base_logging::INFO;
using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::EquivToProto;
using ::testing::InSequence;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::Ne;
using ::testing::Return;
using ::testing::ScopedMockLog;

constexpr double kInf = std::numeric_limits<double>::infinity();

// Basic LP model:
//
//   a and b are continuous variable
//
//   minimize a - b
//       s.t. 0 <= a
//            0 <= b <= 3
struct BasicLp {
  BasicLp();

  // Returns the expected optimal result for this model. Only put the given set
  // of variables in the result (to test filters).
  SolveResultProto OptimalResult(
      const absl::flat_hash_set<Variable>& vars) const;

  Model model;
  const Variable a;
  const Variable b;
};

BasicLp::BasicLp()
    : a(model.AddVariable(0.0, kInf, false, "a")),
      b(model.AddVariable(0.0, 3.0, false, "b")) {}

SolveResultProto BasicLp::OptimalResult(
    const absl::flat_hash_set<Variable>& vars) const {
  SolveResultProto result;
  result.mutable_termination()->set_reason(TERMINATION_REASON_OPTIMAL);
  result.mutable_solve_stats()->mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_FEASIBLE);
  result.mutable_solve_stats()->mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_FEASIBLE);
  PrimalSolutionProto* const solution =
      result.add_solutions()->mutable_primal_solution();
  solution->set_objective_value(0.0);
  solution->set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  if (vars.contains(a)) {
    solution->mutable_variable_values()->add_ids(a.id());
    solution->mutable_variable_values()->add_values(0.0);
  }
  if (vars.contains(b)) {
    solution->mutable_variable_values()->add_ids(b.id());
    solution->mutable_variable_values()->add_values(3.0);
  }
  return result;
}

TEST(PrinterMessageCallbackTest, StringStream) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  std::ostringstream oss;
  const SolveArguments args = {.message_callback =
                                   PrinterMessageCallback(oss, "logs| ")};

  const auto fake_solve =
      [&](const SolveParametersProto&, const ModelSolveParametersProto&,
          const MessageCallback message_cb, const CallbackRegistrationProto&,
          SolverInterface::Callback,
          const SolveInterrupter*) -> absl::StatusOr<SolveResultProto> {
    message_cb({"line 1", "line 2"});
    message_cb({"line 3"});
    return basic_lp.OptimalResult({basic_lp.a, basic_lp.b});
  };

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(EquivToProto(args.parameters.Proto()),
                              EquivToProto(model_parameters), Ne(nullptr),
                              EquivToProto(args.callback_registration.Proto()),
                              Eq(nullptr), Eq(nullptr)))
        .WillOnce(fake_solve);
  }

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(basic_lp.model, EnumFromProto(registration.solver_type()).value(),
            args));

  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
  EXPECT_EQ(oss.str(), "logs| line 1\nlogs| line 2\nlogs| line 3\n");
}

TEST(InfoLoggerMessageCallbackTest, Logging) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  std::ostringstream oss;
  const SolveArguments args = {.message_callback =
                                   InfoLoggerMessageCallback("logs| ")};

  const auto fake_solve =
      [&](const SolveParametersProto&, const ModelSolveParametersProto&,
          const MessageCallback message_cb, const CallbackRegistrationProto&,
          SolverInterface::Callback,
          const SolveInterrupter*) -> absl::StatusOr<SolveResultProto> {
    message_cb({"line 1", "line 2"});
    message_cb({"line 3"});
    return basic_lp.OptimalResult({basic_lp.a, basic_lp.b});
  };

  ScopedMockLog log(kDoNotCaptureLogsYet);

  // Expected file path for testing logs.
  const std::string test_path = "ortools/math_opt/cpp/message_callback_test.cc";

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(EquivToProto(args.parameters.Proto()),
                              EquivToProto(model_parameters), Ne(nullptr),
                              EquivToProto(args.callback_registration.Proto()),
                              Eq(nullptr), Eq(nullptr)))
        .WillOnce(fake_solve);
    EXPECT_CALL(log, Log(INFO, test_path, "logs| line 1"));
    EXPECT_CALL(log, Log(INFO, test_path, "logs| line 2"));
    EXPECT_CALL(log, Log(INFO, test_path, "logs| line 3"));
  }

  log.StartCapturingLogs();
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(basic_lp.model, EnumFromProto(registration.solver_type()).value(),
            args));

  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
}

// In this test we set the `-v` flag to enable verbose logging.
TEST(VLoggerMessageCallbackTest, VisibleLog) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  std::ostringstream oss;
  const SolveArguments args = {.message_callback =
                                   VLoggerMessageCallback(1, "logs| ")};

  // Flags local to each test, see go/gunitprimer#invoking-the-tests.
  absl::SetFlag(&FLAGS_vmodule, "message_callback=1");

  const auto fake_solve =
      [&](const SolveParametersProto&, const ModelSolveParametersProto&,
          const MessageCallback message_cb, const CallbackRegistrationProto&,
          SolverInterface::Callback,
          const SolveInterrupter*) -> absl::StatusOr<SolveResultProto> {
    message_cb({"line 1", "line 2"});
    message_cb({"line 3"});
    return basic_lp.OptimalResult({basic_lp.a, basic_lp.b});
  };

  ScopedMockLog log(kDoNotCaptureLogsYet);

  // Expected file path for testing logs.
  const std::string test_path = "ortools/math_opt/cpp/message_callback_test.cc";

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(EquivToProto(args.parameters.Proto()),
                              EquivToProto(model_parameters), Ne(nullptr),
                              EquivToProto(args.callback_registration.Proto()),
                              Eq(nullptr), Eq(nullptr)))
        .WillOnce(fake_solve);
    EXPECT_CALL(log, Log(INFO, test_path, "logs| line 1"));
    EXPECT_CALL(log, Log(INFO, test_path, "logs| line 2"));
    EXPECT_CALL(log, Log(INFO, test_path, "logs| line 3"));
  }

  log.StartCapturingLogs();
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(basic_lp.model, EnumFromProto(registration.solver_type()).value(),
            args));

  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
}

// In this test we set the `-v` flag to false to disable verbose logging.
TEST(VLoggerMessageCallbackTest, InvisibleLog) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  std::ostringstream oss;
  const SolveArguments args = {.message_callback =
                                   VLoggerMessageCallback(1, "logs| ")};

  // Flags local to each test, see go/gunitprimer#invoking-the-tests.
  absl::SetFlag(&FLAGS_v, false);

  const auto fake_solve =
      [&](const SolveParametersProto&, const ModelSolveParametersProto&,
          const MessageCallback message_cb, const CallbackRegistrationProto&,
          SolverInterface::Callback,
          const SolveInterrupter*) -> absl::StatusOr<SolveResultProto> {
    message_cb({"line 1", "line 2"});
    message_cb({"line 3"});
    return basic_lp.OptimalResult({basic_lp.a, basic_lp.b});
  };

  ScopedMockLog log(kDoNotCaptureLogsYet);

  // Expected file path for testing logs.
  const std::string test_path = "ortools/math_opt/cpp/message_callback_test.cc";

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(EquivToProto(args.parameters.Proto()),
                              EquivToProto(model_parameters), Ne(nullptr),
                              EquivToProto(args.callback_registration.Proto()),
                              Eq(nullptr), Eq(nullptr)))
        .WillOnce(fake_solve);
  }

  log.StartCapturingLogs();
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(basic_lp.model, EnumFromProto(registration.solver_type()).value(),
            args));

  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
}

TEST(VectorMessageCallbackTest, Basic) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  std::vector<std::string> messages = {"initial content 1",
                                       "initial content 2"};
  const SolveArguments args = {.message_callback =
                                   VectorMessageCallback(&messages)};

  const auto fake_solve =
      [&](const SolveParametersProto&, const ModelSolveParametersProto&,
          const MessageCallback message_cb, const CallbackRegistrationProto&,
          SolverInterface::Callback,
          const SolveInterrupter*) -> absl::StatusOr<SolveResultProto> {
    message_cb({"line 1", "line 2"});
    message_cb({"line 3"});
    return basic_lp.OptimalResult({basic_lp.a, basic_lp.b});
  };

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(EquivToProto(args.parameters.Proto()),
                              EquivToProto(model_parameters), Ne(nullptr),
                              EquivToProto(args.callback_registration.Proto()),
                              Eq(nullptr), Eq(nullptr)))
        .WillOnce(fake_solve);
  }

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(basic_lp.model, EnumFromProto(registration.solver_type()).value(),
            args));

  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(messages, ElementsAre("initial content 1", "initial content 2",
                                    "line 1", "line 2", "line 3"));
}

TEST(RepeatedPtrFieldMessageCallbackTest, Basic) {
  SolverInterfaceFactoryMock factory_mock;
  SolverFactoryRegistration registration(factory_mock.AsStdFunction());

  BasicLp basic_lp;

  google::protobuf::RepeatedPtrField<std::string> messages;
  messages.Add("initial content 1");
  messages.Add("initial content 2");
  const SolveArguments args = {.message_callback =
                                   RepeatedPtrFieldMessageCallback(&messages)};

  const auto fake_solve =
      [&](const SolveParametersProto&, const ModelSolveParametersProto&,
          const MessageCallback message_cb, const CallbackRegistrationProto&,
          SolverInterface::Callback,
          const SolveInterrupter*) -> absl::StatusOr<SolveResultProto> {
    message_cb({"line 1", "line 2"});
    message_cb({"line 3"});
    return basic_lp.OptimalResult({basic_lp.a, basic_lp.b});
  };

  SolverInterfaceMock solver;
  {
    InSequence s;

    EXPECT_CALL(factory_mock,
                Call(EquivToProto(basic_lp.model.ExportModel()), _))
        .WillOnce(Return(ByMove(std::make_unique<DelegatingSolver>(&solver))));

    ASSERT_OK_AND_ASSIGN(const ModelSolveParametersProto model_parameters,
                         args.model_parameters.Proto());
    EXPECT_CALL(solver, Solve(EquivToProto(args.parameters.Proto()),
                              EquivToProto(model_parameters), Ne(nullptr),
                              EquivToProto(args.callback_registration.Proto()),
                              Eq(nullptr), Eq(nullptr)))
        .WillOnce(fake_solve);
  }

  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      Solve(basic_lp.model, EnumFromProto(registration.solver_type()).value(),
            args));

  EXPECT_EQ(result.termination.reason, TerminationReason::kOptimal);
  EXPECT_THAT(messages, ElementsAre("initial content 1", "initial content 2",
                                    "line 1", "line 2", "line 3"));
}

}  // namespace
}  // namespace operations_research::math_opt
