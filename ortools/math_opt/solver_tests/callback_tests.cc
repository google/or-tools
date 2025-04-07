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

#include "ortools/math_opt/solver_tests/callback_tests.h"

#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/status_macros.h"
#include "ortools/gurobi/gurobi_stdout_matchers.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/io/mps_converter.h"
#include "ortools/math_opt/solver_tests/test_models.h"
#include "ortools/port/proto_utils.h"
#include "ortools/port/scoped_std_stream_capture.h"

namespace operations_research {
namespace math_opt {

MessageCallbackTestParams::MessageCallbackTestParams(
    const SolverType solver_type, const bool support_message_callback,
    const bool support_interrupter, const bool integer_variables,
    std::string ending_substring, SolveParameters solve_parameters)
    : solver_type(solver_type),
      support_message_callback(support_message_callback),
      support_interrupter(support_interrupter),
      integer_variables(integer_variables),
      ending_substring(std::move(ending_substring)),
      solve_parameters(std::move(solve_parameters)) {}

std::ostream& operator<<(std::ostream& out,
                         const MessageCallbackTestParams& params) {
  out << "{ solver_type: " << params.solver_type
      << ", support_message_callback: " << params.support_message_callback
      << ", support_interrupter: " << params.support_interrupter
      << ", integer_variables: " << params.integer_variables
      << ", ending_substring: \"" << absl::CEscape(params.ending_substring)
      << "\", solve_parameters: "
      << ProtobufShortDebugString(params.solve_parameters.Proto()) << " }";
  return out;
}

CallbackTestParams::CallbackTestParams(
    const SolverType solver_type, const bool integer_variables,
    const bool add_lazy_constraints, const bool add_cuts,
    absl::flat_hash_set<CallbackEvent> supported_events,
    std::optional<SolveParameters> all_solutions,
    std::optional<SolveParameters> reaches_cut_callback)
    : solver_type(solver_type),
      integer_variables(integer_variables),
      add_lazy_constraints(add_lazy_constraints),
      add_cuts(add_cuts),
      supported_events(std::move(supported_events)),
      all_solutions(std::move(all_solutions)),
      reaches_cut_callback(std::move(reaches_cut_callback)) {}

namespace {

template <typename T>
struct EnumFormatter {
  void operator()(std::string* const out, const T value) {
    out->append(std::string(EnumToString(value)));
  }
};

absl::StatusOr<std::unique_ptr<Model>> LoadMiplibInstance(
    const absl::string_view name) {
  ASSIGN_OR_RETURN(
      const ModelProto model_proto,
      ReadMpsFile(absl::StrCat("ortools/math_opt/solver_tests/testdata/", name,
                               ".mps")));
  return Model::FromModelProto(model_proto);
}

};  // namespace

std::ostream& operator<<(std::ostream& out, const CallbackTestParams& params) {
  out << "{ solver_type: " << params.solver_type
      << ", integer_variables: " << params.integer_variables
      << ", add_lazy_constraints: " << params.add_lazy_constraints
      << ", add_cuts: " << params.add_cuts << ", supported_events: "
      << absl::StrJoin(params.supported_events, ",",
                       EnumFormatter<CallbackEvent>())
      << ", all_solutions: "
      << (params.all_solutions.has_value()
              ? ProtobufShortDebugString(params.all_solutions->Proto())
              : "nullopt")
      << ", reaches_cut_callback: "
      << (params.reaches_cut_callback.has_value()
              ? ProtobufShortDebugString(params.reaches_cut_callback->Proto())
              : "nullopt")
      << " }";
  return out;
}

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Each;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kTolerance = 1e-6;

TEST_P(MessageCallbackTest, EmptyIfNotSupported) {
  Model model("model");

  absl::Mutex mutex;
  std::vector<std::string> callback_messages;
  const auto callback = [&](absl::Span<const std::string> messages) {
    const absl::MutexLock lock(&mutex);
    for (const auto& message : messages) {
      callback_messages.push_back(message);
    }
  };

  EXPECT_THAT(
      Solve(model, GetParam().solver_type, {.message_callback = callback}),
      IsOkAndHolds(IsOptimal(0.0)));
  if (!GetParam().support_message_callback) {
    EXPECT_THAT(callback_messages, IsEmpty());
  }
}

TEST_P(MessageCallbackTest, ObjectiveValueAndEndingSubstring) {
  Model model("model");
  const Variable x =
      model.AddVariable(0, 21.0, GetParam().integer_variables, "x");
  model.Maximize(2.0 * x);

  absl::Mutex mutex;
  std::vector<std::string> callback_messages;

  SolveArguments args = {
      .parameters = GetParam().solve_parameters,
      .message_callback =
          [&](absl::Span<const std::string> messages) {
            const absl::MutexLock lock(&mutex);
            for (const auto& message : messages) {
              callback_messages.push_back(message);
            }
          },
  };

  // First test with enable_output being false.
  args.parameters.enable_output = false;
  {
#ifdef OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
    ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
#endif  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         Solve(model, GetParam().solver_type, args));
#ifdef OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
    EXPECT_THAT(
        std::move(stdout_capture).StopCaptureAndReturnContents(),
        EmptyOrGurobiLicenseWarningIfGurobi(
            /*is_gurobi=*/GetParam().solver_type == SolverType::kGurobi));
#endif  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
    ASSERT_THAT(result, IsOptimal(42.0));
    EXPECT_THAT(callback_messages, Each(Not(HasSubstr("\n"))));
    if (GetParam().support_message_callback) {
      EXPECT_THAT(absl::StrJoin(callback_messages, "\n"),
                  AllOf(AnyOf(HasSubstr("42"), HasSubstr("4.2")),
                        HasSubstr(GetParam().ending_substring)));
    } else {
      EXPECT_THAT(callback_messages, IsEmpty());
    }
  }

  // Then test with enable_output being true.
  callback_messages.clear();
  args.parameters.enable_output = true;
  {
#ifdef OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
    ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
#endif  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         Solve(model, GetParam().solver_type, args));
#ifdef OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
    EXPECT_THAT(
        std::move(stdout_capture).StopCaptureAndReturnContents(),
        EmptyOrGurobiLicenseWarningIfGurobi(
            /*is_gurobi=*/GetParam().solver_type == SolverType::kGurobi));
#endif  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
    ASSERT_THAT(result, IsOptimal(42.0));
    EXPECT_THAT(callback_messages, Each(Not(HasSubstr("\n"))));
    if (GetParam().support_message_callback) {
      EXPECT_THAT(absl::StrJoin(callback_messages, "\n"),
                  AllOf(AnyOf(HasSubstr("42"), HasSubstr("4.2")),
                        HasSubstr(GetParam().ending_substring)));
    } else {
      EXPECT_THAT(callback_messages, IsEmpty());
    }
  }

  // Then test without callback and with enable_output being true.
  args.parameters.enable_output = true;
  args.message_callback = nullptr;
  callback_messages.clear();

#ifdef OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
#endif  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type, args));
#ifdef OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
  EXPECT_THAT(std::move(stdout_capture).StopCaptureAndReturnContents(),
              AllOf(AnyOf(HasSubstr("42"), HasSubstr("4.2")),
                    HasSubstr(GetParam().ending_substring)));
#endif  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
  ASSERT_THAT(result, IsOptimal(42.0));
  EXPECT_THAT(callback_messages, IsEmpty());
}

TEST_P(MessageCallbackTest, InterruptAtFirstMessage) {
  if (!GetParam().support_message_callback) {
    GTEST_SKIP() << "Message callback not supported. Ignoring this test.";
  }
  if (!GetParam().support_interrupter) {
    GTEST_SKIP() << "Solve interrupter not supported. Ignoring this test.";
  }
  const std::unique_ptr<const Model> model =
      SmallModel(GetParam().integer_variables);

  absl::Mutex mutex;
  std::vector<std::string> callback_messages;  // Guarded by mutex.
  bool first = true;                           // Guarded by mutex.

  SolveArguments args;
  SolveInterrupter interrupter;
  args.interrupter = &interrupter;
  args.message_callback = [&](absl::Span<const std::string> messages) {
    const absl::MutexLock lock(&mutex);
    for (const auto& message : messages) {
      callback_messages.push_back(message);
    }
    if (first) {
      first = false;
      interrupter.Interrupt();
    }
  };
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(*model, GetParam().solver_type, args));
  ASSERT_THAT(result, TerminatesWithLimit(Limit::kInterrupted));
  // We should have stopped before reaching the end.
  EXPECT_THAT(absl::StrJoin(callback_messages, "\n"),
              Not(HasSubstr(GetParam().ending_substring)));
}

// Builds a trivial model that can be solved in presolve, checks that the
// presolve stats show all variables and constraints are deleted.
TEST_P(CallbackTest, EventPresolve) {
  if (!GetParam().supported_events.contains(CallbackEvent::kPresolve)) {
    GTEST_SKIP()
        << "Test skipped because this solver does not support this event.";
  }

  Model model("model");
  Variable x = model.AddVariable(0, 2.0, GetParam().integer_variables, "x");
  Variable y = model.AddVariable(0, 3.0, GetParam().integer_variables, "y");
  model.AddLinearConstraint(y <= 1.0);
  model.Maximize(2.0 * x + y);
  SolveArguments args = {
      .callback_registration = {.events = {CallbackEvent::kPresolve}}};
  absl::Mutex mutex;
  std::optional<CallbackData> last_presolve_data;  // Guarded by mutex.
  args.callback = [&](const CallbackData& callback_data) {
    const absl::MutexLock lock(&mutex);
    last_presolve_data = callback_data;
    return CallbackResult();
  };

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type, args));
  ASSERT_THAT(result, IsOptimal(5.0));
  ASSERT_TRUE(last_presolve_data.has_value());
  EXPECT_EQ(last_presolve_data->presolve_stats.removed_variables(), 2);
  EXPECT_EQ(last_presolve_data->presolve_stats.removed_constraints(), 1);
}

TEST_P(CallbackTest, EventSimplex) {
  if (!GetParam().supported_events.contains(CallbackEvent::kSimplex)) {
    GTEST_SKIP()
        << "Test skipped because this solver does not support this event.";
  }

  Model model("model");
  Variable x1 = model.AddVariable(0, 2.0, false, "x1");
  Variable x2 = model.AddVariable(0, 3.0, false, "x2");
  Variable x3 = model.AddVariable(0, 4.0, false, "x3");
  model.Maximize(x1 - x2 + x3);

  SolveArguments args;
  args.parameters.presolve = Emphasis::kOff;
  args.parameters.lp_algorithm = LPAlgorithm::kPrimalSimplex;
  // Note: we solve and then change the objective to so that on our second
  // solve, we know the starting basis. It would be simpler to set the starting
  // basis, once this is supported.
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<IncrementalSolver> solver,
                       NewIncrementalSolver(&model, GetParam().solver_type));
  {
    ASSERT_OK_AND_ASSIGN(const SolveResult result, solver->Solve(args));
    ASSERT_THAT(result, IsOptimal(6.0));
  }

  // We know that from the previous optimal solution, we should take 3 pivots.
  model.Maximize(-x1 + x2 - x3);

  args.callback_registration.events.insert(CallbackEvent::kSimplex);
  absl::Mutex mutex;
  std::vector<CallbackDataProto::SimplexStats> stats;  // Guarded-by mutex.
  args.callback = [&](const CallbackData& callback_data) {
    const absl::MutexLock lock(&mutex);
    stats.push_back(callback_data.simplex_stats);
    return CallbackResult();
  };
  ASSERT_OK_AND_ASSIGN(const SolveResult result, solver->Solve(args));
  ASSERT_THAT(result, IsOptimal(3.0));
  // It should take at least 3 pivots to move from (2, 0, 4) to (0, 3, 0)
  ASSERT_GE(stats.size(), 3);
  for (const CallbackDataProto::SimplexStats& s : stats) {
    // Because we are using primal simplex and start with a feasible solution,
    // it should always be feasible
    EXPECT_NEAR(s.primal_infeasibility(), 0, kTolerance);
  }
  // We should begin dual infeasible.
  EXPECT_EQ(stats[0].iteration_count(), 0);
  EXPECT_GT(stats[0].dual_infeasibility(), 0.0);
  EXPECT_NEAR(stats[0].objective_value(), -6.0, kTolerance);

  EXPECT_GE(stats.back().iteration_count(), 3);
  // The objective value is not updating in the callback, investigate.
}

TEST_P(CallbackTest, EventBarrier) {
  if (!GetParam().supported_events.contains(CallbackEvent::kBarrier)) {
    GTEST_SKIP()
        << "Test skipped because this solver does not support this event.";
  }

  // Make a model that requires multiple barrier steps to solve.
  const std::unique_ptr<const Model> model =
      SmallModel(GetParam().integer_variables);

  SolveArguments args;
  args.parameters.presolve = Emphasis::kOff;
  args.parameters.lp_algorithm = LPAlgorithm::kBarrier;
  args.callback_registration.events.insert(CallbackEvent::kBarrier);
  absl::Mutex mutex;
  std::vector<CallbackDataProto::BarrierStats> stats;  // Guarded-by mutex.
  args.callback = [&](const CallbackData& callback_data) {
    const absl::MutexLock lock(&mutex);
    stats.push_back(callback_data.barrier_stats);
    return CallbackResult();
  };
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(*model, GetParam().solver_type, args));
  ASSERT_THAT(result, IsOptimal(12.0));

  // TODO(b/196035470): test more data from the stats.
  ASSERT_GE(stats.size(), 1);
  EXPECT_GE(stats.back().iteration_count(), 3);
}

TEST_P(CallbackTest, EventSolutionAlwaysCalled) {
  if (!GetParam().supported_events.contains(CallbackEvent::kMipSolution)) {
    GTEST_SKIP() << "Test skipped because this solver does not support "
                    "CallbackEvent::kMipSolution.";
  }

  // This test must use integer variables.
  ASSERT_TRUE(GetParam().integer_variables);

  Model model("model");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  model.AddLinearConstraint(x + y <= 1);
  model.Maximize(x + 2 * y);

  SolveArguments args = {
      .callback_registration = {.events = {CallbackEvent::kMipSolution}}};
  absl::Mutex mutex;
  bool cb_called = false;
  bool cb_called_on_optimal = false;
  args.callback = [&](const CallbackData& callback_data) {
    const absl::MutexLock lock(&mutex);
    cb_called = true;
    EXPECT_EQ(callback_data.event, CallbackEvent::kMipSolution);
    if (!callback_data.solution.has_value()) {
      ADD_FAILURE() << "callback_data.solution should always be set at event "
                       "MIP_SOLUTION but was empty";
      return CallbackResult();
    }
    const VariableMap<double>& sol = *callback_data.solution;
    EXPECT_THAT(
        sol, AnyOf(IsNear({{x, 0.0}, {y, 0.0}}), IsNear({{x, 1.0}, {y, 0.0}}),
                   IsNear({{x, 0.0}, {y, 1.0}})));
    if (gtl::FindWithDefault(sol, y) > 0.5) {
      cb_called_on_optimal = true;
    }
    return CallbackResult();
  };
  EXPECT_THAT(Solve(model, GetParam().solver_type, args),
              IsOkAndHolds(IsOptimal(2.0)));
  EXPECT_TRUE(cb_called);
  EXPECT_TRUE(cb_called_on_optimal);
}

TEST_P(CallbackTest, EventSolutionInterrupt) {
  if (!GetParam().supported_events.contains(CallbackEvent::kMipSolution)) {
    GTEST_SKIP() << "Test skipped because this solver does not support "
                    "CallbackEvent::kMipSolution.";
  }

  // This test must use integer variables.
  ASSERT_TRUE(GetParam().integer_variables);

  // A model where we will not prove optimality immediately.
  const std::unique_ptr<const Model> model =
      DenseIndependentSet(/*integer=*/true);
  const SolveArguments args = {
      // Don't prove optimality in presolve.
      .parameters = {.presolve = Emphasis::kOff},
      .callback_registration = {.events = {CallbackEvent::kMipSolution}},
      .callback = [&](const CallbackData& /*callback_data*/) {
        return CallbackResult{.terminate = true};
      }};
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(*model, GetParam().solver_type, args));
  EXPECT_THAT(result, TerminatesWithReasonFeasible(Limit::kInterrupted));
  EXPECT_TRUE(result.has_primal_feasible_solution());
}

TEST_P(CallbackTest, EventSolutionCalledMoreThanOnce) {
  if (!GetParam().supported_events.contains(CallbackEvent::kMipSolution)) {
    GTEST_SKIP() << "Test skipped because this solver does not support "
                    "CallbackEvent::kMipSolution.";
  }
  if (!GetParam().all_solutions.has_value()) {
    GTEST_SKIP() << "Test skipped because this solver does not support "
                    "getting all solutions.";
  }

  // This test must use integer variables.
  ASSERT_TRUE(GetParam().integer_variables);

  Model model("model");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const Variable z = model.AddBinaryVariable("z");
  model.AddLinearConstraint(x + y + z <= 1);

  SolveArguments args = {
      .parameters = *GetParam().all_solutions,
      .callback_registration = {.events = {CallbackEvent::kMipSolution}},
  };
  absl::Mutex mutex;
  bool cb_called_on_zero = false;
  bool cb_called_on_x = false;
  bool cb_called_on_y = false;
  bool cb_called_on_z = false;
  args.callback = [&](const CallbackData& callback_data) {
    const absl::MutexLock lock(&mutex);
    EXPECT_EQ(callback_data.event, CallbackEvent::kMipSolution);
    if (!callback_data.solution.has_value()) {
      ADD_FAILURE() << "callback_data.solution should always be set at event "
                       "MIP_SOLUTION but was empty";
      return CallbackResult();
    }
    const VariableMap<double>& sol = *callback_data.solution;
    EXPECT_THAT(sol, AnyOf(IsNear({{x, 0.0}, {y, 0.0}, {z, 0.0}}),
                           IsNear({{x, 1.0}, {y, 0.0}, {z, 0.0}}),
                           IsNear({{x, 0.0}, {y, 1.0}, {z, 0.0}}),
                           IsNear({{x, 0.0}, {y, 0.0}, {z, 1.0}})));
    if (gtl::FindWithDefault(sol, x) > 0.5) {
      cb_called_on_x = true;
    } else if (gtl::FindWithDefault(sol, y) > 0.5) {
      cb_called_on_y = true;
    } else if (gtl::FindWithDefault(sol, z) > 0.5) {
      cb_called_on_z = true;
    } else {
      cb_called_on_zero = true;
    }
    return CallbackResult();
  };
  EXPECT_THAT(Solve(model, GetParam().solver_type, args),
              IsOkAndHolds(IsOptimal()));
  EXPECT_TRUE(cb_called_on_zero);
  EXPECT_TRUE(cb_called_on_x);
  EXPECT_TRUE(cb_called_on_y);
  EXPECT_TRUE(cb_called_on_z);
}

TEST_P(CallbackTest, EventSolutionLazyConstraint) {
  if (!GetParam().supported_events.contains(CallbackEvent::kMipSolution)) {
    GTEST_SKIP() << "Test skipped because this solver does not support "
                    "CallbackEvent::kMipSolution.";
  }
  if (!GetParam().add_lazy_constraints) {
    GTEST_SKIP() << "Test skipped because this solver does not support adding "
                    "lazy constraints.";
  }

  // This test must use integer variables.
  ASSERT_TRUE(GetParam().integer_variables);

  Model model("model");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  model.Maximize(x + 2 * y);

  SolveArguments args = {
      .callback_registration = {.events = {CallbackEvent::kMipSolution},
                                .add_lazy_constraints = true}};
  // Add the constraint x+y <= 1 if it is violated by the current solution.
  args.callback = [&](const CallbackData& callback_data) -> CallbackResult {
    if (!callback_data.solution.has_value()) {
      ADD_FAILURE() << "callback_data.solution should always be set at event "
                       "MIP_SOLUTION but was empty";
      return {};
    }
    const VariableMap<double>& sol = *callback_data.solution;
    if (sol.size() != 2) {
      ADD_FAILURE()
          << "callback_data.solution should have two elements but found: "
          << sol.size();
      return {};
    }
    const double x_val = sol.at(x);
    const double y_val = sol.at(y);
    CallbackResult result;
    if (x_val + y_val >= 1.0 + 1e-5) {
      result.AddLazyConstraint(x + y <= 1.0);
    }
    return result;
  };
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type, args));
  ASSERT_THAT(result, IsOptimal(2.0));
}

TEST_P(CallbackTest, EventSolutionLazyConstraintWithLinearConstraints) {
  if (!GetParam().supported_events.contains(CallbackEvent::kMipSolution)) {
    GTEST_SKIP() << "Test skipped because this solver does not support "
                    "CallbackEvent::kMipSolution.";
  }
  if (!GetParam().add_lazy_constraints) {
    GTEST_SKIP() << "Test skipped because this solver does not support adding "
                    "lazy constraints.";
  }

  // This test must use integer variables.
  ASSERT_TRUE(GetParam().integer_variables);

  Model model("model");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const Variable z = model.AddBinaryVariable("z");
  model.Maximize(x + 2 * y - z);
  model.AddLinearConstraint(x + y + z >= 1.0);

  SolveArguments args = {
      .callback_registration = {.events = {CallbackEvent::kMipSolution},
                                .add_lazy_constraints = true}};
  // Add the constraint x+y <= 1 if it is violated by the current solution.
  args.callback = [&](const CallbackData& callback_data) -> CallbackResult {
    if (!callback_data.solution.has_value()) {
      ADD_FAILURE() << "callback_data.solution should always be set at event "
                       "MIP_SOLUTION but was empty";
      return {};
    }
    const VariableMap<double>& sol = *callback_data.solution;
    if (sol.size() != 3) {
      ADD_FAILURE()
          << "callback_data.solution should have two elements but found: "
          << sol.size();
      return {};
    }
    const double x_val = sol.at(x);
    const double y_val = sol.at(y);
    CallbackResult result;
    if (x_val + y_val >= 1.0 + 1e-5) {
      result.AddLazyConstraint(x + y <= 1.0);
    }
    return result;
  };
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type, args));
  ASSERT_THAT(result, IsOptimal(2.0));
}

TEST_P(CallbackTest, EventSolutionFilter) {
  if (!GetParam().supported_events.contains(CallbackEvent::kMipSolution)) {
    GTEST_SKIP() << "Test skipped because this solver does not support "
                    "CallbackEvent::kMipSolution.";
  }

  // This test must use integer variables.
  ASSERT_TRUE(GetParam().integer_variables);

  Model model("model");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  model.AddLinearConstraint(x + y <= 1);
  model.Maximize(x + 2 * y);

  SolveArguments args = {.callback_registration = {
                             .events = {CallbackEvent::kMipSolution},
                             .mip_solution_filter = MakeKeepKeysFilter({y})}};
  absl::Mutex mutex;
  bool cb_called = false;
  bool cb_called_on_optimal = false;
  args.callback = [&](const CallbackData& callback_data) {
    const absl::MutexLock lock(&mutex);
    cb_called = true;
    EXPECT_EQ(callback_data.event, CallbackEvent::kMipSolution);
    if (!callback_data.solution.has_value()) {
      ADD_FAILURE() << "callback_data.solution should always be set at event "
                       "MIP_SOLUTION but was empty";
      return CallbackResult();
    }
    const VariableMap<double>& sol = *callback_data.solution;
    EXPECT_THAT(sol, AnyOf(IsNear({{y, 0.0}}), IsNear({{y, 1.0}})));
    if (gtl::FindWithDefault(sol, y) > 0.5) {
      cb_called_on_optimal = true;
    }
    return CallbackResult();
  };
  EXPECT_THAT(Solve(model, GetParam().solver_type, args),
              IsOkAndHolds(IsOptimal(2.0)));
  EXPECT_TRUE(cb_called);
  EXPECT_TRUE(cb_called_on_optimal);
}

TEST_P(CallbackTest, EventNodeCut) {
  if (GetParam().solver_type == SolverType::kGscip) {
    GTEST_SKIP() << "This test does not work with SCIP v900";
  }
  if (!GetParam().supported_events.contains(CallbackEvent::kMipNode)) {
    GTEST_SKIP() << "Test skipped because this solver does not support "
                    "CallbackEvent::kMipNode.";
  }
  if (!GetParam().add_cuts) {
    GTEST_SKIP() << "Test skipped because this solver does not support adding "
                    "cuts.";
  }
  if (!GetParam().reaches_cut_callback.has_value()) {
    GTEST_SKIP() << "Test skipped, needs reaches_cut_callback to be set.";
  }

  // This test must use integer variables.
  ASSERT_TRUE(GetParam().integer_variables);

  // Max sum_i x_i
  // s.t. x_i + x_j + x_k <= 2 for all i < j < k
  // x_i binary for all i
  //
  // Optimal objective is 2, where any two x_i = 1 and the rest are zero.
  //
  // Strengthened by the cut:
  //   sum_i x_i <= 2
  //
  // This is basically a clique cut. Note that if we try to use a simpler form
  // of the problem, where x_i + x_j <= 1 for all i, j, with an optimal
  // objective of one, then the branching rule in SCIP can do domain reductions
  // and solve the problem at the root node.
  Model model("model");
  constexpr int n = 10;
  std::vector<Variable> x;
  for (int i = 0; i < n; ++i) {
    x.push_back(model.AddBinaryVariable(absl::StrCat("x", i)));
  }
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      for (int k = j + 1; k < n; ++k) {
        model.AddLinearConstraint(x[i] + x[j] + x[k] <= 2.0);
      }
    }
  }
  model.Maximize(Sum(x));

  for (const bool use_cut : {false, true}) {
    SCOPED_TRACE(absl::StrCat("use_cut:", use_cut));
    SolveArguments args = {.parameters =
                               GetParam().reaches_cut_callback.value()};
    args.parameters.node_limit = 1;
    if (use_cut) {
      args.callback_registration = {.events = {CallbackEvent::kMipNode},
                                    .add_cuts = true};

      args.callback = [&](const CallbackData& callback_data) -> CallbackResult {
        if (!callback_data.solution.has_value()) {
          return {};
        }
        const VariableMap<double>& sol = *callback_data.solution;
        CallbackResult result;
        if (Sum(x).Evaluate(sol) > 2.0 + 1.0e-5) {
          result.AddUserCut(Sum(x) <= 2.0);
        }
        return result;
      };
    }
    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         Solve(model, GetParam().solver_type, args));
    // Even with use_cut: False, SCIP v900 return OPTIMAL
    if (GetParam().solver_type == SolverType::kGscip) {
      EXPECT_THAT(result, IsOptimal(2.0));
    } else {
      if (use_cut) {
        EXPECT_THAT(result, IsOptimal(2.0));
      } else {
        EXPECT_THAT(result.termination, LimitIs(Limit::kNode));
      }
    }
  }
}

TEST_P(CallbackTest, EventNodeFilter) {
  if (!GetParam().supported_events.contains(CallbackEvent::kMipNode)) {
    GTEST_SKIP() << "Test skipped because this solver does not support "
                    "CallbackEvent::kMipNode.";
  }

  // This test must use integer variables.
  ASSERT_TRUE(GetParam().integer_variables);
  // Use the MIPLIB instance 23588, which has optimal solution 8090 and LP
  // relaxation of 7649.87. This instance was selected because every
  // supported solver can solve it quickly (a few seconds), but no solver can
  // solve it in one node (so the node callback will be invoked).
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<Model> model,
                       LoadMiplibInstance("23588"));
  const std::vector<Variable> variables = model->SortedVariables();
  CHECK_GE(variables.size(), 3);
  const Variable x0 = variables[0];
  const Variable x2 = variables[2];

  SolveArguments args = {.callback_registration = {
                             .events = {CallbackEvent::kMipNode},
                             .mip_node_filter = MakeKeepKeysFilter({x0, x2})}};
  absl::Mutex mutex;
  std::vector<VariableMap<double>> solutions;
  int empty_solution_count = 0;
  args.callback = [&](const CallbackData& callback_data) {
    EXPECT_EQ(callback_data.event, CallbackEvent::kMipNode);
    const absl::MutexLock lock(&mutex);
    if (!callback_data.solution.has_value()) {
      empty_solution_count++;
    } else {
      solutions.push_back(callback_data.solution.value());
    }
    return CallbackResult();
  };
  EXPECT_THAT(Solve(*model, GetParam().solver_type, args),
              IsOkAndHolds(IsOptimal(8090)));
  LOG(INFO) << "callback_data.solution was not set " << empty_solution_count
            << " times";
  EXPECT_THAT(solutions, Each(UnorderedElementsAre(Pair(x0, _), Pair(x2, _))));
}

TEST_P(CallbackTest, StatusPropagation) {
  if (!GetParam().supported_events.contains(CallbackEvent::kMipSolution)) {
    GTEST_SKIP() << "Test skipped because this solver does not support "
                    "CallbackEvent::kMipSolution.";
  }
  if (!GetParam().add_lazy_constraints) {
    GTEST_SKIP() << "Test skipped because this solver does not support adding "
                    "lazy constraints.";
  }

  // This test must use integer variables.
  ASSERT_TRUE(GetParam().integer_variables);

  // Check status propagation by adding an invalid cut.
  Model model("model");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  model.Maximize(x + 2 * y);

  SolveArguments args = {
      .callback_registration = {.events = {CallbackEvent::kMipSolution},
                                .add_lazy_constraints = true}};
  absl::Mutex mutex;
  bool added_cut = false;  // Guarded by mutex.
  args.callback = [&](const CallbackData& /*callback_data*/) {
    const absl::MutexLock lock(&mutex);
    CallbackResult result;
    if (!added_cut) {
      result.AddLazyConstraint(x + y <= -kInf);
      added_cut = true;
    }
    return result;
  };
  EXPECT_THAT(Solve(model, GetParam().solver_type, args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid negative infinite value; for "
                                 "GeneratedLinearConstraint.upper_bound")));
}

TEST_P(CallbackTest, UnsupportedEvents) {
  Model model("model");
  model.AddVariable(0, 1.0, GetParam().integer_variables, "x");

  for (const CallbackEvent event : Enum<CallbackEvent>::AllValues()) {
    if (GetParam().supported_events.contains(event)) {
      continue;
    }
    SCOPED_TRACE(absl::StrCat("event: ", EnumToString(event)));

    const SolveArguments args = {
        .callback_registration = {.events = {event}},
        .callback = [](const CallbackData&) { return CallbackResult{}; }};

    EXPECT_THAT(Solve(model, GetParam().solver_type, args),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr(ProtoEnumToString(EnumToProto(event)))));
  }
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
