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

#include "ortools/math_opt/solver_tests/generic_tests.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/gurobi/gurobi_stdout_matchers.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/test_models.h"
#include "ortools/port/proto_utils.h"
#include "ortools/port/scoped_std_stream_capture.h"

namespace operations_research::math_opt {

std::ostream& operator<<(std::ostream& out,
                         const TimeLimitTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << ", integer_variables: " << params.integer_variables
      << ", callback_event: " << params.event << " }";
  return out;
}

GenericTestParameters::GenericTestParameters(const SolverType solver_type,
                                             const bool support_interrupter,
                                             const bool integer_variables,
                                             std::string expected_log,
                                             SolveParameters solve_parameters)
    : solver_type(solver_type),
      support_interrupter(support_interrupter),
      integer_variables(integer_variables),
      expected_log(std::move(expected_log)),
      solve_parameters(std::move(solve_parameters)) {}

std::ostream& operator<<(std::ostream& out,
                         const GenericTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << ", support_interrupter: " << params.support_interrupter
      << ", integer_variables: " << params.integer_variables
      << ", expected_log: \"" << absl::CEscape(params.expected_log) << "\""
      << ", solve_parameters: "
      << ProtobufShortDebugString(params.solve_parameters.Proto()) << " }";
  return out;
}

namespace {

using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;

constexpr double kInf = std::numeric_limits<double>::infinity();

TEST_P(GenericTest, EmptyModel) {
  Model model;
  EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(0.0)));
}

TEST_P(GenericTest, OffsetOnlyMinimization) {
  Model model;
  model.Minimize(4.0);
  EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(4.0)));
}

TEST_P(GenericTest, OffsetOnlyMaximization) {
  Model model;
  model.Maximize(4.0);
  EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(4.0)));
}

TEST_P(GenericTest, OffsetMinimization) {
  Model model;
  const Variable x =
      model.AddVariable(-1.0, 2.0, GetParam().integer_variables, "x");
  model.Minimize(2 * x + 4);
  EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(2.0)));
}

TEST_P(GenericTest, OffsetMaximization) {
  Model model;
  const Variable x =
      model.AddVariable(-1.0, 2.0, GetParam().integer_variables, "x");
  model.Maximize(2 * x + 4);
  EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(8.0)));
}

TEST_P(GenericTest, SolveTime) {
  // We use a non-trivial problem since on WASM the time resolution is of 1ms
  // and thus a trivial model could be solved in absl::ZeroDuration().
  //
  // We also don't use a constant complexity. The reason is that the solve time
  // depends on the build flags and the solve algorithm used by the solver (and
  // the solver itself). And using a unique constant can lead to too short or
  // too long solve times. Here we just want to make sure that we have a long
  // enough solve time so that it is not too close to zero.
  constexpr int kMinN = 10;
  constexpr int kMaxN = 30;
  constexpr int kIncrementN = 5;
  constexpr absl::Duration kMinSolveTime = absl::Milliseconds(5);
  for (int n = kMinN; n <= kMaxN; n += kIncrementN) {
    SCOPED_TRACE(absl::StrCat("n = ", n));
    const std::unique_ptr<const Model> model =
        DenseIndependentSet(GetParam().integer_variables, /*n=*/n);

    const absl::Time start = absl::Now();
    ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(*model));
    const absl::Duration expected_max_solve_time = absl::Now() - start;

    if (expected_max_solve_time <= kMinSolveTime && n < kMaxN) {
      LOG(INFO) << "The solve ended too quickly (" << expected_max_solve_time
                << ") with n=" << n << "; retrying with a more complex model.";
      continue;
    }
    EXPECT_GT(result.solve_stats.solve_time, absl::ZeroDuration());
    EXPECT_LE(result.solve_stats.solve_time, expected_max_solve_time);

    break;
  }
}

TEST_P(GenericTest, InterruptBeforeSolve) {
  if (!GetParam().support_interrupter) {
    GTEST_SKIP() << "Solve interrupter not supported. Ignoring this test.";
  }

  const std::unique_ptr<Model> model = SmallModel(GetParam().integer_variables);

  SolveInterrupter interrupter;
  interrupter.Interrupt();

  SolveArguments args;
  args.parameters = GetParam().solve_parameters;
  args.interrupter = &interrupter;

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(*model, GetParam().solver_type, args));
  EXPECT_THAT(result, TerminatesWithLimit(Limit::kInterrupted));
}

TEST_P(GenericTest, InterruptAfterSolve) {
  if (!GetParam().support_interrupter) {
    GTEST_SKIP() << "Solve interrupter not supported. Ignoring this test.";
  }

  const std::unique_ptr<Model> model = SmallModel(GetParam().integer_variables);

  SolveInterrupter interrupter;

  SolveArguments args;
  args.parameters = GetParam().solve_parameters;
  args.interrupter = &interrupter;

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(*model, GetParam().solver_type, args));

  // Calling Interrupt after the end of the solve should not break anything.
  interrupter.Interrupt();
  EXPECT_THAT(result, IsOptimal());
}

TEST_P(GenericTest, InterrupterNeverTriggered) {
  // The rationale for this test is that for Gurobi we have a background thread
  // that is responsible from calling the Gurobi termination API. We want to
  // test that this background thread terminates properly even when the
  // interrupter is not triggered at all.

  if (!GetParam().support_interrupter) {
    GTEST_SKIP() << "Solve interrupter not supported. Ignoring this test.";
  }

  const std::unique_ptr<Model> model = SmallModel(GetParam().integer_variables);

  SolveInterrupter interrupter;

  SolveArguments args;
  args.parameters = GetParam().solve_parameters;
  args.interrupter = &interrupter;

  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(*model, GetParam().solver_type, args));
  EXPECT_THAT(result, IsOptimal());
}

#ifdef OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED
TEST_P(GenericTest, NoStdoutOutputByDefault) {
  Model model("model");
  const Variable x =
      model.AddVariable(0, 21.0, GetParam().integer_variables, "x");
  model.Maximize(2.0 * x);

  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  ASSERT_OK(SimpleSolve(model));
  EXPECT_THAT(std::move(stdout_capture).StopCaptureAndReturnContents(),
              EmptyOrGurobiLicenseWarningIfGurobi(
                  /*is_gurobi=*/GetParam().solver_type == SolverType::kGurobi));
}

TEST_P(GenericTest, EnableOutputPrintsToStdOut) {
  Model model("model");
  const Variable x =
      model.AddVariable(0, 21.0, GetParam().integer_variables, "x");
  model.Maximize(2.0 * x);

  SolveParameters params = GetParam().solve_parameters;
  params.enable_output = true;

  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);

  EXPECT_THAT(Solve(model, GetParam().solver_type, {.parameters = params}),
              IsOkAndHolds(IsOptimal(42.0)));

  EXPECT_THAT(std::move(stdout_capture).StopCaptureAndReturnContents(),
              HasSubstr(GetParam().expected_log));
}
#endif  // OPERATIONS_RESEARCH_OUTPUT_CAPTURE_SUPPORTED

// Returns a string containing all ASCII 7-bits characters (but 0); i.e. all
// characters in [1, 0x7f].
std::string AllAsciiCharacters() {
  std::ostringstream oss;
  for (char c = '\x1'; c < '\x80'; ++c) {
    oss << c;
  }
  return oss.str();
}

// Returns all non-ASCII 7-bits characters, i.e. characters in [0x80, 0xff].
std::string AllNonAsciiCharacters() {
  std::ostringstream oss;
  for (int c = 0x80; c <= 0xff; ++c) {
    oss << static_cast<char>(c);
  }
  return oss.str();
}

TEST_P(GenericTest, ModelNameTooLong) {
  // GLPK and Gurobi have a limit for problem name to 255 characters; here we
  // use long names to validate that it does not raise any assertion (along with
  // other solvers).
  EXPECT_THAT(SimpleSolve(Model(std::string(1024, 'x'))),
              IsOkAndHolds(IsOptimal(0.0)));

  // GLPK refuses control characters (iscntrl()) in the problem name and has a
  // limit for problem name to 255 characters. Here we validate that the
  // truncation of the string takes into account the quoting of the control
  // characters (we pass all 7-bits ASCII characters to make sure they are
  // accepted).
  EXPECT_THAT(SimpleSolve(Model(AllAsciiCharacters() + std::string(1024, 'x'))),
              IsOkAndHolds(IsOptimal(0.0)));

  // GLPK should accept non-ASCII characters (>= 0x80).
  EXPECT_THAT(
      SimpleSolve(Model(AllNonAsciiCharacters() + std::string(1024, 'x'))),
      IsOkAndHolds(IsOptimal(0.0)));
}

TEST_P(GenericTest, VariableNames) {
  // See rationales in ModelName for these tests.
  {
    Model model;
    model.AddVariable(-1.0, 2.0, GetParam().integer_variables,
                      std::string(1024, 'x'));
    EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(0.0)));
  }
  {
    Model model;
    model.AddVariable(-1.0, 2.0, GetParam().integer_variables,
                      AllAsciiCharacters() + std::string(1024, 'x'));
    EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(0.0)));
  }
  {
    Model model;
    model.AddVariable(-1.0, 2.0, GetParam().integer_variables,
                      AllNonAsciiCharacters() + std::string(1024, 'x'));
    EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(0.0)));
  }
  // Test two variables that thanks to the truncation will get the same name are
  // not an issue for the solver.
  {
    Model model;
    model.AddVariable(-1.0, 2.0, GetParam().integer_variables,
                      std::string(1024, '-') + 'x');
    model.AddVariable(-1.0, 2.0, GetParam().integer_variables,
                      std::string(1024, '-') + 'y');
    EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(0.0)));
  }
}

TEST_P(GenericTest, LinearConstraintNames) {
  // See rationales in ModelName for these tests.
  {
    Model model;
    model.AddLinearConstraint(-1.0, 2.0, std::string(1024, 'x'));
    EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(0.0)));
  }
  {
    Model model;
    model.AddLinearConstraint(-1.0, 2.0,
                              AllAsciiCharacters() + std::string(1024, 'x'));
    EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(0.0)));
  }
  {
    Model model;
    model.AddLinearConstraint(-1.0, 2.0,
                              AllNonAsciiCharacters() + std::string(1024, 'x'));
    EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(0.0)));
  }
  // Test two constraints that thanks to the truncation will get the same name
  // are not an issue for the solver.
  {
    Model model;
    model.AddLinearConstraint(-1.0, 2.0, std::string(1024, '-') + 'x');
    model.AddLinearConstraint(-1.0, 2.0, std::string(1024, '-') + 'y');
    EXPECT_THAT(SimpleSolve(model), IsOkAndHolds(IsOptimal(0.0)));
  }
  // Solvers should accept a ModelProto whose linear_constraints.names repeated
  // field is not set. As of 2023-08-21 this is done by remove_names.
  {
    Model model;
    const Variable x =
        model.AddVariable(0.0, 1.0, GetParam().integer_variables, "x");
    model.AddLinearConstraint(x == 1.0, "c");
    SolverInitArguments init_args;
    init_args.remove_names = true;
    ASSERT_OK_AND_ASSIGN(
        const SolveResult result,
        Solve(model, GetParam().solver_type,
              {.parameters = GetParam().solve_parameters}, init_args));
    EXPECT_THAT(result, IsOptimal(0.0));
  }
}

// TODO(b/227217735): Add a QuadraticConstraintNames test.

// Test that the solvers properly translates the MathOpt ids to their internal
// indices by using a model where indices don't start a zero.
TEST_P(GenericTest, NonZeroIndices) {
  // To test that solvers don't truncate by mistake numbers in the whole range
  // of valid id numbers, we force the use of the maximum value by using a input
  // model proto.
  ModelProto base_model_proto;
  constexpr int64_t kMaxValidId = std::numeric_limits<int64_t>::max() - 1;
  {
    VariablesProto& variables = *base_model_proto.mutable_variables();
    variables.add_ids(kMaxValidId - 1);
    variables.add_lower_bounds(-kInf);
    variables.add_upper_bounds(kInf);
    variables.add_integers(false);
  }
  {
    LinearConstraintsProto& linear_constraints =
        *base_model_proto.mutable_linear_constraints();
    linear_constraints.add_ids(kMaxValidId - 1);
    linear_constraints.add_lower_bounds(-kInf);
    linear_constraints.add_upper_bounds(kInf);
  }

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Model> model,
                       Model::FromModelProto(base_model_proto));

  // We remove the temporary variable and constraint we used to offset the id of
  // the new variables and constraints below.
  model->DeleteVariable(model->Variables().back());
  model->DeleteLinearConstraint(model->LinearConstraints().back());

  const Variable x =
      model->AddVariable(0.0, kInf, GetParam().integer_variables, "x");
  EXPECT_EQ(x.id(), kMaxValidId);

  model->Maximize(x);

  const LinearConstraint c = model->AddLinearConstraint(2 * x <= 8.0, "c");
  EXPECT_EQ(c.id(), kMaxValidId);

  EXPECT_THAT(SimpleSolve(*model), IsOkAndHolds(IsOptimal(4.0)));
}

// Returns a matcher that passes if the solver returns the
// InvertedBounds::ToStatus() status.
testing::Matcher<absl::StatusOr<SolveResult>> StatusIsInvertedBounds(
    const InvertedBounds& inverted_bounds) {
  return testing::Property("status", &absl::StatusOr<SolveResult>::status,
                           inverted_bounds.ToStatus());
}

TEST_P(GenericTest, InvertedVariableBounds) {
  const SolveArguments solve_args = {.parameters = GetParam().solve_parameters};

  // First test with bounds inverted at the construction of the solver.
  //
  // Here we test multiple values as some solvers like SCIP can show specific
  // bugs for variables with bounds in {0.0, 1.0}. Those are upgraded to binary
  // and changing bounds of these variables later raises assertions.
  const std::vector<std::pair<double, double>> new_variables_inverted_bounds = {
      {3.0, 1.0}, {0.0, -1.0}, {1.0, -1.0}, {1.0, 0.0}};
  for (const auto [lb, ub] : new_variables_inverted_bounds) {
    SCOPED_TRACE(absl::StrCat("lb: ", lb, " ub: ", ub));

    Model model;

    // Here we add some variables that we immediately remove so that the id of
    // `x` below won't be 0. This will help making sure bugs in conversion from
    // column number to MathOpt ids are caught by this test.
    constexpr int64_t kXId = 13;
    for (int64_t i = 0; i < kXId; ++i) {
      model.DeleteVariable(model.AddVariable());
    }

    const Variable x = model.AddVariable(/*lower_bound=*/lb, /*upper_bound=*/ub,
                                         GetParam().integer_variables, "x");
    ASSERT_EQ(x.id(), kXId);

    model.Maximize(3.0 * x);

    // The instantiation should not fail, even if the bounds are reversed.
    ASSERT_OK_AND_ASSIGN(const std::unique_ptr<IncrementalSolver> solver,
                         NewIncrementalSolver(&model, GetParam().solver_type));

    // Solving should fail because of the inverted bounds.
    EXPECT_THAT(solver->Solve(solve_args),
                StatusIsInvertedBounds({.variables = {x.id()}}));
  }

  // Then test with bounds inverted during an update.
  //
  // See above for why we use various bounds.
  for (const auto [initial_lb, initial_ub, new_lb, new_ub] :
       std::vector<std::tuple<double, double, double, double>>{
           {3.0, 4.0, 5.0, 4.0},
           {0.0, 1.0, 2.0, 1.0},
           {1.0, 1.0, 2.0, 1.0},
           {0.0, 1.0, 0.0, -1.0},
           {1.0, 1.0, 1.0, 0.0},
           {1.0, 1.0, 1.0, -1.0}}) {
    SCOPED_TRACE(absl::StrCat("initial_lb: ", initial_lb,
                              " initial_ub: ", initial_ub, " new_lb: ", new_lb,
                              " new_ub: ", new_ub));
    Model model;

    // Here we add some variables that we immediately remove so that the id of
    // `x` below won't be 0. This will help making sure bugs in conversion from
    // column number to MathOpt ids are caught by this test.
    constexpr int64_t kXId = 13;
    for (int64_t i = 0; i < kXId; ++i) {
      model.DeleteVariable(model.AddVariable());
    }

    const Variable x = model.AddVariable(/*lower_bound=*/initial_lb,
                                         /*upper_bound=*/initial_ub,
                                         GetParam().integer_variables, "x");
    ASSERT_EQ(x.id(), kXId);

    model.Maximize(3.0 * x);

    ASSERT_OK_AND_ASSIGN(const std::unique_ptr<IncrementalSolver> solver,
                         NewIncrementalSolver(&model, GetParam().solver_type));

    // As of 2022-11-17 the glp_interior() algorithm returns GLP_EFAIL when the
    // model is "empty" (no rows or columns). The issue is that the emptiness is
    // considered *after* the model has been somewhat pre-processed, in
    // particular after FIXED variables have been removed.
    //
    // TODO(b/259557110): remove this skip once glpk_solver.cc is fixed
    if (GetParam().solver_type == SolverType::kGlpk &&
        GetParam().solve_parameters.lp_algorithm == LPAlgorithm::kBarrier) {
      LOG(INFO) << "Skipping the initial solve as glp_interior() would fail.";
    } else {
      EXPECT_THAT(solver->SolveWithoutUpdate(solve_args),
                  IsOkAndHolds(IsOptimal(3.0 * initial_ub)));
    }

    // Breaking the bounds should make the SolveWithoutUpdate() fail but not the
    // Update() itself.
    model.set_lower_bound(x, new_lb);
    model.set_upper_bound(x, new_ub);
    ASSERT_OK(solver->Update());
    EXPECT_THAT(solver->SolveWithoutUpdate(solve_args),
                StatusIsInvertedBounds({.variables = {x.id()}}));
  }

  // Finally test with an update adding a variable with inverted bounds.
  //
  // See above for why we use various bounds.
  for (const auto [lb, ub] : new_variables_inverted_bounds) {
    SCOPED_TRACE(absl::StrCat("lb: ", lb, " ub: ", ub));
    Model model;

    // Here we add some variables that we immediately remove so that the id of
    // `x` below won't be 0. This will help making sure bugs in conversion from
    // column number to MathOpt ids are caught by this test.
    constexpr int64_t kXId = 13;
    for (int64_t i = 0; i < kXId; ++i) {
      model.DeleteVariable(model.AddVariable());
    }

    const Variable x =
        model.AddVariable(/*lower_bound=*/3.0, /*upper_bound=*/4.0,
                          GetParam().integer_variables, "x");
    ASSERT_EQ(x.id(), kXId);

    model.Maximize(3.0 * x);

    ASSERT_OK_AND_ASSIGN(const std::unique_ptr<IncrementalSolver> solver,
                         NewIncrementalSolver(&model, GetParam().solver_type));

    EXPECT_THAT(solver->SolveWithoutUpdate(solve_args),
                IsOkAndHolds(IsOptimal(3.0 * 4.0)));

    // Test the update using a new variable with inverted bounds (in case the
    // update code path is not identical to the NewIncrementalSolver() one).
    const Variable y = model.AddVariable(/*lower_bound=*/lb, /*upper_bound=*/ub,
                                         GetParam().integer_variables, "y");
    model.Maximize(3.0 * x + y);
    ASSERT_OK(solver->Update());
    EXPECT_THAT(solver->SolveWithoutUpdate(solve_args),
                StatusIsInvertedBounds({.variables = {y.id()}}));
  }
}

TEST_P(GenericTest, InvertedLinearConstraintBounds) {
  const SolveArguments solve_args = {.parameters = GetParam().solve_parameters};

  // First test with bounds inverted at the construction of the solver.
  {
    Model model;
    const Variable x =
        model.AddVariable(/*lower_bound=*/0.0, /*upper_bound=*/10.0,
                          GetParam().integer_variables, "x");

    // Here we add some constraints that we immediately remove so that the id of
    // `u` below won't be 0. This will help making sure bugs in conversion from
    // row number to MathOpt ids are caught by this test.
    constexpr int64_t kUId = 23;
    for (int64_t i = 0; i < kUId; ++i) {
      model.DeleteLinearConstraint(model.AddLinearConstraint());
    }

    const LinearConstraint u = model.AddLinearConstraint(3.0 <= x <= 1.0, "u");
    ASSERT_EQ(u.id(), kUId);

    model.Maximize(3.0 * x);

    // The instantiation should not fail, even if the bounds are reversed.
    ASSERT_OK_AND_ASSIGN(const std::unique_ptr<IncrementalSolver> solver,
                         NewIncrementalSolver(&model, GetParam().solver_type));

    // Solving should fail because of the inverted bounds.
    EXPECT_THAT(solver->Solve(solve_args),
                StatusIsInvertedBounds({.linear_constraints = {u.id()}}));
  }

  // Then test with bounds inverted during an update.
  {
    Model model;
    const Variable x =
        model.AddVariable(/*lower_bound=*/0.0, /*upper_bound=*/10.0,
                          GetParam().integer_variables, "x");

    // Here we add some constraints that we immediately remove so that the id of
    // `u` below won't be 0. This will help making sure bugs in conversion from
    // row number to MathOpt ids are caught by this test.
    constexpr int64_t kUId = 23;
    for (int64_t i = 0; i < kUId; ++i) {
      model.DeleteLinearConstraint(model.AddLinearConstraint());
    }

    const LinearConstraint u = model.AddLinearConstraint(3.0 <= x <= 4.0, "u");
    ASSERT_EQ(u.id(), kUId);

    model.Maximize(3.0 * x);

    ASSERT_OK_AND_ASSIGN(const std::unique_ptr<IncrementalSolver> solver,
                         NewIncrementalSolver(&model, GetParam().solver_type));

    EXPECT_THAT(solver->SolveWithoutUpdate(solve_args),
                IsOkAndHolds(IsOptimal(3.0 * 4.0)));

    model.set_lower_bound(u, 5.0);

    // Breaking the bounds should make the SolveWithoutUpdate() fail but not the
    // Update() itself.
    ASSERT_OK(solver->Update());
    EXPECT_THAT(solver->SolveWithoutUpdate(solve_args),
                StatusIsInvertedBounds({.linear_constraints = {u.id()}}));
  }

  // Finally test with an update adding a constraint with inverted bounds.
  {
    Model model;
    const Variable x =
        model.AddVariable(/*lower_bound=*/0.0, /*upper_bound=*/10.0,
                          GetParam().integer_variables, "x");

    // Here we add some constraints that we immediately remove so that the id of
    // `u` below won't be 0. This will help making sure bugs in conversion from
    // row number to MathOpt ids are caught by this test.
    constexpr int64_t kUId = 23;
    for (int64_t i = 0; i < kUId; ++i) {
      model.DeleteLinearConstraint(model.AddLinearConstraint());
    }

    const LinearConstraint u = model.AddLinearConstraint(3.0 <= x <= 4.0, "u");
    ASSERT_EQ(u.id(), kUId);

    model.Maximize(3.0 * x);

    ASSERT_OK_AND_ASSIGN(const std::unique_ptr<IncrementalSolver> solver,
                         NewIncrementalSolver(&model, GetParam().solver_type));

    EXPECT_THAT(solver->SolveWithoutUpdate(solve_args),
                IsOkAndHolds(IsOptimal(3.0 * 4.0)));

    // Test the update with a new constraint with inverted bounds (in case the
    // update code path is not identical to the NewIncrementalSolver() one).
    const LinearConstraint v = model.AddLinearConstraint(5.0 <= x <= 3.0, "v");

    ASSERT_OK(solver->Update());
    EXPECT_THAT(solver->SolveWithoutUpdate(solve_args),
                StatusIsInvertedBounds({.linear_constraints = {v.id()}}));
  }
}

TEST_P(TimeLimitTest, DenseIndependentSetNoTimeLimit) {
  const std::unique_ptr<const Model> model =
      DenseIndependentSet(GetParam().integer_variables);
  const double expected_objective =
      GetParam().integer_variables ? 7 : 10.0 * (5 + 4 + 3) / 2.0;
  EXPECT_THAT(Solve(*model, GetParam().solver_type),
              IsOkAndHolds(IsOptimal(expected_objective)));
}

TEST_P(TimeLimitTest, DenseIndependentSetTimeLimit) {
  ASSERT_TRUE(GetParam().event.has_value())
      << "The TimeLimit test requires a callback event is given.";
  SolveArguments solve_args = {.message_callback =
                                   InfoLoggerMessageCallback("[solver] ")};
  solve_args.parameters.time_limit = absl::Seconds(1);
  // We want to block all progress while sleeping in the callback, so we limit
  // the solver to one thread.
  solve_args.parameters.threads = 1;
  // Presolve can eliminate the whole problem for some solvers (CP-SAT).
  solve_args.parameters.presolve = Emphasis::kOff;
  solve_args.callback_registration.events.insert(*GetParam().event);

  const std::unique_ptr<const Model> model =
      DenseIndependentSet(GetParam().integer_variables);

  // Callback may be called from multiple threads, serialize access to has_run.
  absl::Mutex mutex;
  bool has_run = false;
  solve_args.callback = [&mutex, &has_run](const CallbackData& data) {
    const absl::MutexLock lock(mutex);
    if (!has_run) {
      LOG(INFO) << "Waiting two seconds in the callback...";
      absl::SleepFor(absl::Seconds(2));
      LOG(INFO) << "Done waiting in callback.";
    }
    has_run = true;
    return CallbackResult();
  };
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(*model, GetParam().solver_type, solve_args));
  EXPECT_THAT(result, TerminatesWithLimit(Limit::kTime,
                                          /*allow_limit_undetermined=*/true));
  EXPECT_TRUE(has_run);
}

}  // namespace
}  // namespace operations_research::math_opt
