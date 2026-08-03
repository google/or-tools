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

#include "ortools/math_opt/cpp/executor/solve_executor_tests.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/executor/solve_executor.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/port/scoped_std_stream_capture.h"

namespace operations_research::math_opt {
namespace {

using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

// CanonicalStatusIs is only available internally
template <typename... Args>
inline auto CanonicalStatusIs(Args&&... args) {
  return StatusIs(std::forward<Args>(args)...);
}
}  // namespace

std::ostream& operator<<(std::ostream& ostr,
                         const SolveExecutorTestParameters& params) {
  ostr << params.name;
  return ostr;
}

namespace {

TEST_P(SolveExecutorTest, SolveSuccess) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  EXPECT_THAT(executor->Solve(model, SolverType::kGlop),
              IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 1.0}})));
}

TEST_P(SolveExecutorTest, CancelBeforeStart) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  SolveInterrupter canceller;
  canceller.Interrupt();
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  EXPECT_THAT(
      executor->Solve(model, SolverType::kGlop, {}, {.canceller = &canceller}),
      AnyOf(CanonicalStatusIs(absl::StatusCode::kCancelled),
            IsOkAndHolds(TerminatesWithLimit(Limit::kInterrupted))));
}

TEST_P(SolveExecutorTest, InterruptBeforeStart) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  if (GetParam().name == "subprocess_solve") {
    GTEST_SKIP() << "Subprocess solve does not guarantee interruptions before "
                    "the start of a solve are respected, see b/346799566";
  }
  SolveInterrupter interrupter;
  interrupter.Interrupt();
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  const absl::StatusOr<SolveResult> result =
      executor->Solve(model, SolverType::kGlop, {.interrupter = &interrupter});
  if (features().interruption) {
    EXPECT_THAT(result, IsOkAndHolds(TerminatesWithLimit(Limit::kInterrupted)));
  } else {
    EXPECT_THAT(result, IsOkAndHolds(IsOptimalWithSolution(1.0, {{x, 1.0}})));
  }
}

TEST_P(SolveExecutorTest, SolveCallback) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  model.Maximize(2 * x + y);
  auto cb = [x, y](const CallbackData& data) {
    CHECK_EQ(data.event, CallbackEvent::kMipSolution);
    CallbackResult result;
    if (data.solution->at(x) + data.solution->at(y) >= 1.0 + 1e-5) {
      result.AddLazyConstraint(x + y <= 1.0);
    }
    return result;
  };
  const absl::StatusOr<SolveResult> result = executor->Solve(
      model, SolverType::kGscip,
      {.callback_registration = {.events = {CallbackEvent::kMipSolution},
                                 .add_lazy_constraints = true},
       .callback = std::move(cb)});
  if (features().solve_callback) {
    EXPECT_THAT(result,
                IsOkAndHolds(IsOptimalWithSolution(2.0, {{x, 1.0}, {y, 0.0}})));
  } else {
    EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                 HasSubstr("solve callback")));
  }
}

TEST_P(SolveExecutorTest, LogCallback) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  std::vector<std::string> logs;
  EXPECT_THAT(
      executor->Solve(model, SolverType::kGlop,
                      {.message_callback = VectorMessageCallback(&logs)}),
      IsOkAndHolds(IsOptimal(1.0)));
  EXPECT_THAT(absl::StrJoin(logs, "\n"), HasSubstr("status: OPTIMAL"));
}

TEST_P(SolveExecutorTest, EnableOutput) {
  if (!ScopedStdStreamCapture::kIsSupported) {
    GTEST_SKIP() << "Stdout can't be captured.";
  }

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  absl::StatusOr<SolveResult> result = executor->Solve(
      model, SolverType::kGlop, {.parameters = {.enable_output = true}});
  EXPECT_THAT(std::move(stdout_capture).StopCaptureAndReturnContents(),
              HasSubstr("status: OPTIMAL"));
  EXPECT_THAT(result, IsOkAndHolds(IsOptimal(1.0)));
}

TEST_P(SolveExecutorTest, EnableOutputFalseIsSilent) {
  if (!ScopedStdStreamCapture::kIsSupported) {
    GTEST_SKIP() << "Stdout can't be captured.";
  }

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  absl::StatusOr<SolveResult> result =
      executor->Solve(model, SolverType::kGlop, {});
  EXPECT_THAT(std::move(stdout_capture).StopCaptureAndReturnContents(),
              IsEmpty());
  EXPECT_THAT(result, IsOkAndHolds(IsOptimal(1.0)));
}

TEST_P(SolveExecutorTest, ExplicitDeadlineAlreadyPassed) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  EXPECT_THAT(
      executor->Solve(model, SolverType::kGlop, {},
                      {.explicit_deadline = absl::ZeroDuration()}),
      AnyOf(CanonicalStatusIs(absl::StatusCode::kDeadlineExceeded),
            IsOkAndHolds(TerminatesWith(TerminationReason::kNoSolutionFound))));
}

TEST_P(SolveExecutorTest, SolverType) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExecutorIncrementalSolver> solver,
                       executor->New(&model, SolverType::kGlop));
  EXPECT_EQ(solver->solver_type(), SolverType::kGlop);
}

TEST_P(SolveExecutorTest, IncrementalSolverIsActuallyIncremental) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  SolveParameters params = {.presolve = Emphasis::kOff};
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExecutorIncrementalSolver> solver,
                       executor->New(&model, SolverType::kGlop));
  EXPECT_THAT(solver->Solve({.parameters = params}),
              IsOkAndHolds(IsOptimal(1.0)));

  std::vector<std::string> logs;
  EXPECT_THAT(solver->Solve({.parameters = params,
                             .message_callback = VectorMessageCallback(&logs)}),
              IsOkAndHolds(IsOptimal(1.0)));
  const std::string was_incremental = "Starting basis: incremental solve";
  if (features().actual_incrementalism) {
    EXPECT_THAT(absl::StrJoin(logs, "\n"), HasSubstr(was_incremental));
  } else {
    EXPECT_THAT(absl::StrJoin(logs, "\n"), Not(HasSubstr(was_incremental)));
  }
}

TEST_P(SolveExecutorTest,
       IncrementalSolveRespectsAlreadyPassedExplicitDeadline) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ExecutorIncrementalSolver> solver,
      executor->New(&model, SolverType::kGlop,
                    {.explicit_deadline = absl::ZeroDuration()}));
  EXPECT_THAT(
      solver->Solve({}),
      AnyOf(CanonicalStatusIs(absl::StatusCode::kDeadlineExceeded),
            IsOkAndHolds(TerminatesWith(TerminationReason::kNoSolutionFound))));
}

TEST_P(SolveExecutorTest, IncrementalSolveRespectsAlreadyCancelled) {
  if (GetParam().name == "subprocess_solve") {
    GTEST_SKIP() << "Subprocess solve does not guarantee interruptions before "
                    "the start of a solve are respected, see b/346799566";
  }
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  SolveInterrupter canceller;
  canceller.Interrupt();
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<ExecutorIncrementalSolver> solver,
      executor->New(&model, SolverType::kGlop, {.canceller = &canceller}));
  EXPECT_THAT(solver->Solve({}),
              AnyOf(CanonicalStatusIs(absl::StatusCode::kCancelled),
                    IsOkAndHolds(TerminatesWithLimit(Limit::kInterrupted))));
}

TEST_P(SolveExecutorTest, IncrementalSolverLogCallback) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  std::vector<std::string> logs;
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExecutorIncrementalSolver> solver,
                       executor->New(&model, SolverType::kGlop));
  EXPECT_THAT(solver->Solve({.message_callback = VectorMessageCallback(&logs)}),
              IsOkAndHolds(IsOptimal(1.0)));
  EXPECT_THAT(absl::StrJoin(logs, "\n"), HasSubstr("status: OPTIMAL"));
}

TEST_P(SolveExecutorTest, IncrementalSolverEnableOutput) {
  if (!ScopedStdStreamCapture::kIsSupported) {
    GTEST_SKIP() << "Stdout can't be captured.";
  }

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExecutorIncrementalSolver> solver,
                       executor->New(&model, SolverType::kGlop));
  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  absl::StatusOr<SolveResult> result =
      solver->Solve({.parameters = {.enable_output = true}});
  EXPECT_THAT(std::move(stdout_capture).StopCaptureAndReturnContents(),
              HasSubstr("status: OPTIMAL"));
  EXPECT_THAT(result, IsOkAndHolds(IsOptimal(1.0)));
}

TEST_P(SolveExecutorTest, IncrementalSolverEnableOutputFalseIsSilent) {
  if (!ScopedStdStreamCapture::kIsSupported) {
    GTEST_SKIP() << "Stdout can't be captured.";
  }

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<SolveExecutor> executor, MakeExecutor());
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  model.Maximize(x);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ExecutorIncrementalSolver> solver,
                       executor->New(&model, SolverType::kGlop));
  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  absl::StatusOr<SolveResult> result = solver->Solve();
  EXPECT_THAT(std::move(stdout_capture).StopCaptureAndReturnContents(),
              IsEmpty());
  EXPECT_THAT(result, IsOkAndHolds(IsOptimal(1.0)));
}

}  // namespace

}  // namespace operations_research::math_opt
