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

#include "ortools/math_opt/core/c_api/solver.h"

#include <stddef.h>

#include <limits>
#include <string>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/status_builder.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::AllOf;
using ::testing::EqualsProto;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

static constexpr int kGlop = static_cast<int>(SolverType::kGlop);

TEST(MathOptInterrupterTest, BasicUse) {
  MathOptInterrupter* const interrupter = MathOptNewInterrupter();
  const absl::Cleanup delete_interrupter = [interrupter]() {
    MathOptFreeInterrupter(interrupter);
  };
  ASSERT_TRUE(interrupter != nullptr);
  EXPECT_FALSE(MathOptIsInterrupted(interrupter));
  MathOptInterrupt(interrupter);
  EXPECT_TRUE(MathOptIsInterrupted(interrupter));
}

TEST(MathOptInterrupterTest, FreeNullOk) { MathOptFreeInterrupter(nullptr); }

// A helper method to parse and validate the output of MathOptSolve(). The
// function verifies the following, any of which given an Aborted error.
//  * if code == 0 (success):
//    - status_msg_str is nullptr
//    - if solve_result_size == 0, then solve_result_bytes == nullptr
//    - solve_result_size <= INT_MAX
//    - solve_result_bytes parses to a SolveResultProto
//  * if code != 0 (error):
//    - status_msg_str is not nullptr
//    - solve_result_size == 0
//    - solve_result_bytes == nullptr
// When these invariants are met, returns:
//  * a SolveResultProto if `code` is zero.
//  * a Status error with StatusCode `code` and message `status_msg_str` if
//    `code` is nonzero.
// In the case where we abort, we include the error code if nonzero and the
// error message if not null.
//
// Note that we first test this method, and then use it in most of the actual
// unit tests for MathOptSolve().
absl::StatusOr<SolveResultProto> ParseMathOptSolveOutput(
    const int code, const void* const solve_result_bytes,
    const size_t solve_result_size, const char* const status_msg_str) {
  // If the code indicates success, ensure that the status message is empty and
  // that a SolveResultProto can be parsed, then return them.
  if (code == 0) {
    if (status_msg_str != nullptr) {
      return util::AbortedErrorBuilder()
             << "expected status_msg_str to be null on OK solve, but was: "
             << status_msg_str;
    }
    if (solve_result_size > 0 && solve_result_bytes == nullptr) {
      return util::AbortedErrorBuilder()
             << "expected solve_result_bytes to be not null on OK solve with "
                "positive solve_result_size: "
             << solve_result_size;
    }
    if (solve_result_size > std::numeric_limits<int>::max()) {
      return util::AbortedErrorBuilder()
             << "solve_result_size should be at most INT_MAX but found: "
             << solve_result_size;
    }
    SolveResultProto solve_result;
    if (!solve_result.ParseFromArray(solve_result_bytes,
                                     static_cast<int>(solve_result_size))) {
      return absl::AbortedError("failed to parse SolveResultProto");
    }
    return solve_result;
  }
  // Otherwise (the status code indicates an error), check that the status
  // message is present and that there is no SolveResultProto information, then
  // return a Status error.
  if (status_msg_str == nullptr) {
    return util::AbortedErrorBuilder() << "on solver failure with nonzero code "
                                       << static_cast<absl::StatusCode>(code)
                                       << " error message should not be null";
  }
  absl::Status underlying_failure(static_cast<absl::StatusCode>(code),
                                  status_msg_str);
  if (solve_result_size > 0) {
    return util::AbortedErrorBuilder()
           << "solve_result_size should be 0 on failure but was: "
           << solve_result_size
           << "; underlying failure was: " << underlying_failure;
  }
  if (solve_result_bytes != nullptr) {
    return util::AbortedErrorBuilder()
           << "solve_result_bytes should be nullptr on failure but was not"
           << "; underlying failure was: " << underlying_failure;
  }
  return underlying_failure;
}

TEST(ParseMathOptSolveOutputTest, CodeOkHasResult) {
  SolveResultProto result;
  result.mutable_termination()->set_reason(TERMINATION_REASON_OPTIMAL);
  const std::string result_str = result.SerializeAsString();
  EXPECT_THAT(
      ParseMathOptSolveOutput(0, result_str.data(), result_str.size(), nullptr),
      IsOkAndHolds(EqualsProto(result)));
}

TEST(ParseMathOptSolveOutputTest, CodeOkButStatusMessageNotNullAborts) {
  const SolveResultProto result;
  const std::string result_str = result.SerializeAsString();
  EXPECT_THAT(
      ParseMathOptSolveOutput(0, result_str.data(), result_str.size(), "dog"),
      StatusIs(absl::StatusCode::kAborted, HasSubstr("dog")));
}

TEST(ParseMathOptSolveOutputTest,
     CodeOkButSolveResultNullWithPositiveSizeAborts) {
  EXPECT_THAT(ParseMathOptSolveOutput(0, nullptr, 1, nullptr),
              StatusIs(absl::StatusCode::kAborted,
                       HasSubstr("solve_result_bytes to be not null")));
}

TEST(ParseMathOptSolveOutputTest, CodeOkButResultMessageSizeTooLargeAborts) {
  const std::string fake_solve_result = "fakey fakey fakey";
  EXPECT_THAT(
      ParseMathOptSolveOutput(
          0, fake_solve_result.data(),
          static_cast<size_t>(std::numeric_limits<int>::max()) + 1, nullptr),
      StatusIs(absl::StatusCode::kAborted,
               HasSubstr("solve_result_size should be at most INT_MAX")));
}

TEST(ParseMathOptSolveOutputTest, CodeOkButSolveResultFailsToParseAborts) {
  const std::string fake_solve_result = "fakey fakey fakey";
  EXPECT_THAT(ParseMathOptSolveOutput(0, fake_solve_result.data(),
                                      fake_solve_result.size(), nullptr),
              StatusIs(absl::StatusCode::kAborted,
                       HasSubstr("parse SolveResultProto")));
}

TEST(ParseMathOptSolveOutputTest, CodeErrorNullResultAndMessageOk) {
  EXPECT_THAT(
      ParseMathOptSolveOutput(
          static_cast<int>(absl::StatusCode::kAlreadyExists), nullptr, 0,
          "my message"),
      StatusIs(absl::StatusCode::kAlreadyExists, testing::StrEq("my message")));
}

TEST(ParseMathOptSolveOutputTest, CodeErrorNullResultAndEmptyMessageOk) {
  EXPECT_THAT(
      ParseMathOptSolveOutput(
          static_cast<int>(absl::StatusCode::kAlreadyExists), nullptr, 0, ""),
      StatusIs(absl::StatusCode::kAlreadyExists, testing::StrEq("")));
}

TEST(ParseMathOptSolveOutputTest, CodeErrorAndMessageNullAborts) {
  EXPECT_THAT(ParseMathOptSolveOutput(
                  static_cast<int>(absl::StatusCode::kFailedPrecondition),
                  nullptr, 0, nullptr),
              StatusIs(absl::StatusCode::kAborted,
                       AllOf(HasSubstr("error message should not be null"),
                             HasSubstr("FAILED_PRECONDITION"))));
}

TEST(ParseMathOptSolveOutputTest, CodeErrorAndSolveResultAborts) {
  const SolveResultProto result;
  const std::string result_str = result.SerializeAsString();
  EXPECT_THAT(ParseMathOptSolveOutput(
                  static_cast<int>(absl::StatusCode::kFailedPrecondition),
                  result_str.data(), 0, "my message"),
              StatusIs(absl::StatusCode::kAborted,
                       AllOf(HasSubstr("solve_result_bytes should be nullptr"),
                             HasSubstr("my message"),
                             HasSubstr("FAILED_PRECONDITION"))));
}

TEST(ParseMathOptSolveOutputTest, CodeErrorAndSolveResultSizeAborts) {
  EXPECT_THAT(ParseMathOptSolveOutput(
                  static_cast<int>(absl::StatusCode::kFailedPrecondition),
                  nullptr, 5, "my message"),
              StatusIs(absl::StatusCode::kAborted,
                       AllOf(HasSubstr("solve_result_size should be 0"),
                             HasSubstr("my message"),
                             HasSubstr("FAILED_PRECONDITION"))));
}

// A light wrapper on MathOptSolve that validates that all output arguments are
// handled correctly, and converts them to a absl::StatusOr<SolveResultProto>,
// and then frees any allocated memory.
absl::StatusOr<SolveResultProto> MathOptSolveCpp(
    const void* model, const size_t model_size,
    MathOptInterrupter* interrupter = nullptr) {
  // Note, we intentionally put bad values in here, as we want to be sure that
  // this are overwritten by the value we should be filling them with, or
  // nullptr/0 if they end up not being set. In user code, we would expect users
  // initialize these values with 0 or nullptr instead.
  std::string bad_string = "bad initial pointer";
  char* const bad_memory_pointer = bad_string.data();
  void* solve_result_bytes = bad_memory_pointer;
  size_t solve_result_size = std::numeric_limits<size_t>::max();
  char* status_msg_str = bad_memory_pointer;
  const int code =
      MathOptSolve(model, model_size, kGlop, interrupter, &solve_result_bytes,
                   &solve_result_size, &status_msg_str);
  const absl::Cleanup cleanup = [solve_result_bytes, status_msg_str,
                                 bad_memory_pointer]() {
    // Avoid double free. An error will have been detected by
    // ParseMathOptSolveOutput() already, but we don't want memory corruption.
    if (solve_result_bytes != bad_memory_pointer) {
      MathOptFree(solve_result_bytes);
    }
    if (status_msg_str != bad_memory_pointer) {
      MathOptFree(status_msg_str);
    }
  };
  return ParseMathOptSolveOutput(code, solve_result_bytes, solve_result_size,
                                 status_msg_str);
}

TEST(MathOptSolveTest, SuccessSimpleLp) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  model.Maximize(x);

  const ModelProto model_proto = model.ExportModel();
  const std::string model_str = model_proto.SerializeAsString();

  ASSERT_OK_AND_ASSIGN(const SolveResultProto solve_result,
                       MathOptSolveCpp(model_str.data(), model_str.size()));
  ASSERT_EQ(solve_result.termination().reason(), TERMINATION_REASON_OPTIMAL);
  EXPECT_NEAR(solve_result.termination().objective_bounds().primal_bound(), 1.0,
              1.0e-5);
}

TEST(MathOptSolveTest, EmptyModelOk) {
  ASSERT_OK_AND_ASSIGN(const SolveResultProto solve_result,
                       MathOptSolveCpp(nullptr, 0));
  ASSERT_EQ(solve_result.termination().reason(), TERMINATION_REASON_OPTIMAL);
  EXPECT_NEAR(solve_result.termination().objective_bounds().primal_bound(), 0.0,
              1.0e-5);
}

TEST(MathOptSolveTest, InvalidArgumentMipForGlop) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  model.Maximize(x);

  const ModelProto model_proto = model.ExportModel();
  const std::string model_str = model_proto.SerializeAsString();

  EXPECT_THAT(MathOptSolveCpp(model_str.data(), model_str.size()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("integer variables")));
}

TEST(MathOptSolveTest, NullModelWithNonzeroSizeError) {
  EXPECT_THAT(MathOptSolveCpp(nullptr, 1),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("model cannot be null")));
}

TEST(MathOptSolveTest, ModelProtoTooBigError) {
  std::string fake_model;
  EXPECT_THAT(
      MathOptSolveCpp(fake_model.data(),
                      static_cast<size_t>(std::numeric_limits<int>::max()) + 1),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("max int")));
}

TEST(MathOptSolveTest, ModelProtoDoesNotParseError) {
  std::string fake_model = "Will not parse as ModelProto in binary format";
  EXPECT_THAT(MathOptSolveCpp(fake_model.data(), fake_model.size()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("bad model proto")));
}

TEST(MathOptSolveTest, NoOutputSuccessOk) {
  // TODO(b/294571915): once solve parameters are supported, enable output and
  // then test that the logs are still printed.
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  model.Maximize(x);

  const ModelProto model_proto = model.ExportModel();
  const std::string model_str = model_proto.SerializeAsString();

  const int code = MathOptSolve(model_str.data(), model_str.size(), kGlop,
                                nullptr, nullptr, nullptr, nullptr);
  EXPECT_EQ(code, 0) << absl::StatusCode(code);
}

TEST(MathOptSolveTest, NoOutputFailureOk) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  model.Maximize(x);

  const ModelProto model_proto = model.ExportModel();
  const std::string model_str = model_proto.SerializeAsString();

  const int code = MathOptSolve(model_str.data(), model_str.size(), kGlop,
                                nullptr, nullptr, nullptr, nullptr);
  EXPECT_EQ(code, static_cast<int>(absl::StatusCode::kInvalidArgument))
      << absl::StatusCode(code);
}

TEST(MathOptSolveTest, InterruptLpBeforeSolve) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  model.Maximize(x);

  const ModelProto model_proto = model.ExportModel();
  const std::string model_str = model_proto.SerializeAsString();

  MathOptInterrupter* const interrupter = MathOptNewInterrupter();
  const absl::Cleanup cleanup = [interrupter]() {
    MathOptFreeInterrupter(interrupter);
  };
  MathOptInterrupt(interrupter);

  ASSERT_OK_AND_ASSIGN(
      const SolveResultProto solve_result,
      MathOptSolveCpp(model_str.data(), model_str.size(), interrupter));
  EXPECT_EQ(solve_result.termination().reason(),
            TERMINATION_REASON_NO_SOLUTION_FOUND);
  EXPECT_EQ(solve_result.termination().limit(), LIMIT_INTERRUPTED);
  EXPECT_TRUE(MathOptIsInterrupted(interrupter));
}

TEST(MathOptSolveTest, NoInterruptionOk) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  model.Maximize(x);

  const ModelProto model_proto = model.ExportModel();
  const std::string model_str = model_proto.SerializeAsString();

  MathOptInterrupter* const interrupter = MathOptNewInterrupter();
  const absl::Cleanup cleanup = [interrupter]() {
    MathOptFreeInterrupter(interrupter);
  };

  ASSERT_OK_AND_ASSIGN(
      const SolveResultProto solve_result,
      MathOptSolveCpp(model_str.data(), model_str.size(), interrupter));
  EXPECT_EQ(solve_result.termination().reason(), TERMINATION_REASON_OPTIMAL);
  EXPECT_FALSE(MathOptIsInterrupted(interrupter));
}

}  // namespace
}  // namespace operations_research::math_opt
