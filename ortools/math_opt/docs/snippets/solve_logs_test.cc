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

#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/port/scoped_std_stream_capture.h"
#include "testing/base/public/mock-log.h"

namespace {

using ::testing::_;
using ::testing::Contains;
using ::testing::DoubleNear;
using ::testing::HasSubstr;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::ScopedMockLog;
using ::testing::status::IsOkAndHolds;

namespace math_opt = ::operations_research::math_opt;

// [START_print_logs_with_solve_parameters]

// Solves
//   max x
//   s.t. x in {0, 1}
// to optimality, returns the objective value, and prints the logs to standard
// output.
absl::StatusOr<double> PrintLogsWithSolveParameters() {
  math_opt::Model model;
  const math_opt::Variable x = model.AddBinaryVariable("x");
  model.Maximize(x);

  const math_opt::SolveParameters params = {.enable_output = true};
  ABSL_ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                        math_opt::Solve(model, math_opt::SolverType::kGscip,
                                        {.parameters = params}));
  ABSL_RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
  return result.objective_value();
}
// [END_print_logs_with_solve_parameters]

TEST(SolveLogsTest, PrintLogsWithSolveParameters) {
  if (!operations_research::ScopedStdStreamCapture::kIsSupported) {
    GTEST_SKIP() << "Stdout can't be captured.";
  }

  absl::StatusOr<double> objective_value = 0.0;
  {
    operations_research::ScopedStdStreamCapture stdout_capture(
        operations_research::CapturedStream::kStdout);
    objective_value = PrintLogsWithSolveParameters();
    if (objective_value.ok()) {
      EXPECT_THAT(
          std::move(stdout_capture).StopCaptureAndReturnContents(),
          testing::ContainsRegex("problem is solved.*optimal solution found"));
    }
  }
  ASSERT_THAT(objective_value, IsOkAndHolds(DoubleNear(1.0, 1.0e-5)));
}

// [START_canned_message_callback]

// Solves
//   max x
//   s.t. x in {0, 1}
// to optimality and prints the logs to LOG(INFO) with a canned message
// callback, then returns the optimal objective value.
absl::StatusOr<double> CannedMessageCallback() {
  math_opt::Model model;
  const math_opt::Variable x = model.AddBinaryVariable("x");
  model.Maximize(x);

  // See message_callback.h for other canned callbacks.
  ABSL_ASSIGN_OR_RETURN(
      const math_opt::SolveResult result,
      math_opt::Solve(model, math_opt::SolverType::kGscip,
                      {.message_callback = math_opt::InfoLoggerMessageCallback(
                           "my_application_prefix: ")}));
  ABSL_RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
  return result.objective_value();
}
// [END_canned_message_callback]

TEST(SolveLogsTest, CannedMessageCallbackLogs) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log).Times(testing::AnyNumber());
  EXPECT_CALL(log,
              Log(base_logging::INFO, _,
                  testing::MatchesRegex("my_application_prefix.*problem is "
                                        "solved.*optimal solution found.*")));
  log.StartCapturingLogs();
  absl::StatusOr<double> objective_value = CannedMessageCallback();
  log.StopCapturingLogs();
  testing::Mock::VerifyAndClearExpectations(&log);
  ASSERT_THAT(objective_value, IsOkAndHolds(DoubleNear(1.0, 1.0e-5)));
}

// [START_solve_and_return_logs]

// Solves
//   max x
//   s.t. x in {0, 1}
// to optimality and returns the solver logs on success.
absl::StatusOr<std::vector<std::string>> SolveAndReturnLogs() {
  math_opt::Model model;
  const math_opt::Variable x = model.AddBinaryVariable("x");
  model.Maximize(x);

  // The logging callback is not threadsafe for every solver.
  absl::Mutex mutex;
  std::vector<std::string> logs;
  // Note: `VectorMessageCallback()` in message_callback.h does exactly this, we
  // write the callback by hand only as a demonstration.
  auto message_cb = [&mutex, &logs](const std::vector<std::string>& msgs) {
    absl::MutexLock lock(mutex);
    for (const std::string& msg : msgs) {
      logs.push_back(msg);
    }
  };
  ABSL_ASSIGN_OR_RETURN(
      const math_opt::SolveResult result,
      math_opt::Solve(model, math_opt::SolverType::kGscip,
                      {.message_callback = std::move(message_cb)}));
  ABSL_RETURN_IF_ERROR(result.termination.EnsureIsOptimal());
  return logs;
}
// [END_solve_and_return_logs]

TEST(SolveLogsTest, SolveAndReturnLogsHasLogs) {
  ASSERT_OK_AND_ASSIGN(const std::vector<std::string> logs,
                       SolveAndReturnLogs());
  ASSERT_THAT(
      logs, Contains(HasSubstr("problem is solved [optimal solution found]")));
}

}  // namespace
