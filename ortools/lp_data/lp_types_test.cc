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

#include "ortools/lp_data/lp_types.h"

#include <atomic>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/glop/status.h"
#include "ortools/lp_data/lp_types_testing.h"
#include "ortools/util/time_limit.h"

namespace operations_research::glop {
namespace {

using ::testing::_;
using ::testing::Optional;

template <typename T>
std::string ToString(const T& value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

TEST(AbnormalityStatusTest, Ok) {
  const AbnormalityStatus status;
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(status.cause, std::nullopt);
  EXPECT_THAT(status, AbnormalityStatusIsOK());
  EXPECT_THAT(status, Not(AbnormalityStatusIs(
                          AbnormalityCause::kLuFactorizationPivotTooSmall)));
  EXPECT_EQ(ToString(status), "OK");
}

TEST(AbnormalityStatusTest, Error) {
  const AbnormalityStatus status =
      AbnormalityStatus(AbnormalityCause::kLuFactorizationPivotTooSmall);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.cause,
              Optional(AbnormalityCause::kLuFactorizationPivotTooSmall));
  EXPECT_THAT(status, Not(AbnormalityStatusIsOK()));
  EXPECT_THAT(status, AbnormalityStatusIs(
                          AbnormalityCause::kLuFactorizationPivotTooSmall));
  EXPECT_THAT(
      status,
      Not(AbnormalityStatusIs(AbnormalityCause::kRevisedSimplexPivotTooSmall)));
  EXPECT_EQ(ToString(status), "ABNORMALITY(LuFactorizationPivotTooSmall)");
}

TEST(TimeLimitStateToCauseTest, Running) {
  TimeLimit time_limit;
  EXPECT_EQ(TimeLimitStateToCause(time_limit), std::nullopt);
}

TEST(TimeLimitStateToCauseTest, External) {
  TimeLimit time_limit;
  std::atomic<bool> external = true;
  time_limit.RegisterExternalBooleanAsLimit(&external);
  EXPECT_THAT(TimeLimitStateToCause(time_limit),
              Optional(InterruptionCause::kExternal));
}

TEST(TimeLimitStateToCauseTest, Time) {
  TimeLimit time_limit(/*limit_in_seconds=*/0.0);
  EXPECT_THAT(TimeLimitStateToCause(time_limit),
              Optional(InterruptionCause::kTimeLimit));
}

// Returns a SolveStatus. The template parameter is a SolveStatus::Xxx
// alternative.
//
// The macro below will add template specialization for each alternative in
// SolveStatus. We use macros here to make sure we don't do a typo we could have
// done in the code as well. Without this guarantee this code is not very
// interesting.
template <typename Alternative>
SolveStatus CallSolveStatusFactory();

#define GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY(Alternative)           \
  template <>                                                      \
  SolveStatus CallSolveStatusFactory<SolveStatus::Alternative>() { \
    return Alternative##SolveStatus();                             \
  }
// Same as GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY() for alternatives with causes.
//
// Use kTimeLimit cause.
#define GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY_WITH_CAUSE(Alternative, cause) \
  template <>                                                              \
  SolveStatus CallSolveStatusFactory<SolveStatus::Alternative>() {         \
    return Alternative##SolveStatus(cause);                                \
  }

GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY(Optimal);
GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY(PrimalInfeasible);
GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY(DualInfeasible);
GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY(InfeasibleOrUnbounded);
GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY(PrimalUnbounded);
GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY(DualUnbounded);
GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY_WITH_CAUSE(Init,
                                               InterruptionCause::kTimeLimit);
GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY_WITH_CAUSE(PrimalFeasible,
                                               InterruptionCause::kTimeLimit);
GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY_WITH_CAUSE(DualFeasible,
                                               InterruptionCause::kTimeLimit);
GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY_WITH_CAUSE(
    Abnormal, AbnormalityCause::kLuFactorizationPivotTooSmall);
GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY(InvalidProblem);
GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY(Imprecise);

#undef GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY
#undef GLOP_IMPL_SOLVE_STATUS_CALL_FACTORY_WITH_CAUSE

// Returns the name of the Alternative in SNAKE_CASE.
//
// For same rationale as CallSolveStatusFactory(), we use macros to define
// template specializations.
template <typename Alternative>
std::string AlternativeNameSnakeCase();

// Returns the SNAKE_CASE of the input alternative_name (which is CamelCase).
//
// E.g., AlternativeNameSnakeCaseImpl("PrimalFeasible") = "PRIMAL_FEASIBLE".
std::string AlternativeNameSnakeCaseImpl(
    const absl::string_view alternative_name) {
  // Detects pieces by starting upper-case.
  struct UpperCaseDelimiter {
    absl::string_view Find(absl::string_view text, size_t pos) {
      for (size_t i = pos + 1; i < text.size(); ++i) {
        if (absl::ascii_isupper(text[i])) {
          // We use 0 for the length here as we want to keep the character! See
          // absl::ByLength().
          return text.substr(i, 0);
        }
      }
      return text.substr(text.size());
    }
  };
  std::vector<std::string> pieces =
      absl::StrSplit(alternative_name, UpperCaseDelimiter{});
  for (std::string& piece : pieces) {
    absl::AsciiStrToUpper(&piece);
  }
  return absl::StrJoin(pieces, "_");
}

#define GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(Alternative)           \
  template <>                                                        \
  std::string AlternativeNameSnakeCase<SolveStatus::Alternative>() { \
    return AlternativeNameSnakeCaseImpl(#Alternative);               \
  }

GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(Optimal);
GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(PrimalInfeasible);
GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(DualInfeasible);
GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(InfeasibleOrUnbounded);
GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(PrimalUnbounded);
GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(DualUnbounded);
GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(Init);
GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(PrimalFeasible);
GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(DualFeasible);
GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(Abnormal);
GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(InvalidProblem);
GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE(Imprecise);

#undef GLOP_IMPL_ALTERNATIVE_NAME_SNAKE_CASE

// Test fixture for testing SolveStatus behavior for each SolveStatus:Xxx
// alternative (listed in SolveStatusAlernativeTestTypes).
template <typename Alternative>
class SolveStatusAlernativeTest : public ::testing::Test {
 protected:
  static constexpr bool kHasInterruptionCause =
      std::is_same_v<Alternative, SolveStatus::PrimalFeasible> ||
      std::is_same_v<Alternative, SolveStatus::DualFeasible> ||
      std::is_same_v<Alternative, SolveStatus::Init>;
};

using SolveStatusAlernativeTestTypes = ::testing::Types<
    SolveStatus::Optimal, SolveStatus::PrimalInfeasible,
    SolveStatus::DualInfeasible, SolveStatus::InfeasibleOrUnbounded,
    SolveStatus::PrimalUnbounded, SolveStatus::DualUnbounded, SolveStatus::Init,
    SolveStatus::PrimalFeasible, SolveStatus::DualFeasible,
    SolveStatus::Abnormal, SolveStatus::InvalidProblem, SolveStatus::Imprecise>;

TYPED_TEST_SUITE(SolveStatusAlernativeTest, SolveStatusAlernativeTestTypes);

TYPED_TEST(SolveStatusAlernativeTest, Is) {
  const SolveStatus solve_status = CallSolveStatusFactory<TypeParam>();
  EXPECT_TRUE(solve_status.Is<TypeParam>());
}

TYPED_TEST(SolveStatusAlernativeTest, InterruptionCause) {
  const SolveStatus solve_status = CallSolveStatusFactory<TypeParam>();
  if constexpr (TestFixture::kHasInterruptionCause) {
    EXPECT_THAT(solve_status.interruption_cause(),
                Optional(InterruptionCause::kTimeLimit));
  } else {
    EXPECT_EQ(solve_status.interruption_cause(), std::nullopt);
  }
}

TYPED_TEST(SolveStatusAlernativeTest, LegacyStatus) {
  const SolveStatus solve_status = CallSolveStatusFactory<TypeParam>();
  if constexpr (std::is_same_v<TypeParam, SolveStatus::Abnormal>) {
    const Status status = solve_status.LegacyStatus();
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), Status::ERROR_LU);
    EXPECT_EQ(status.error_message(), "LuFactorizationPivotTooSmall");
  } else {
    const Status status = solve_status.LegacyStatus();
    EXPECT_TRUE(status.ok()) << GetErrorCodeString(status.error_code()) << ": "
                             << status.error_message();
  }
}

TYPED_TEST(SolveStatusAlernativeTest, ProblemStatus) {
  const SolveStatus solve_status = CallSolveStatusFactory<TypeParam>();
  EXPECT_EQ(GetProblemStatusString(solve_status.problem_status()),
            AlternativeNameSnakeCase<TypeParam>());
}

TYPED_TEST(SolveStatusAlernativeTest, GetSolveStatusString) {
  const SolveStatus solve_status = CallSolveStatusFactory<TypeParam>();
  const std::string prefix = AlternativeNameSnakeCase<TypeParam>();
  const std::string expected =
      std::is_same_v<TypeParam, SolveStatus::Abnormal>
          ? absl::StrCat(prefix, "(cause: LuFactorizationPivotTooSmall)")
          : (TestFixture::kHasInterruptionCause
                 ? absl::StrCat(prefix, "(cause: TIME_LIMIT)")
                 : prefix);
  // Test the SolveStatus itself.
  EXPECT_EQ(GetSolveStatusString(solve_status), expected);
  // operator<< on the SolveStatus should be the same.
  EXPECT_EQ(ToString(solve_status), expected);

  // operator<< on the alternative should be the same.
  EXPECT_EQ(ToString(std::get<TypeParam>(solve_status.value)), expected);
}

TEST(SolveStatusTest, EqOperator) {
  // Since we use c++20 default operator, we simply use basic test to validate
  // it builds.
  EXPECT_EQ(OptimalSolveStatus(), OptimalSolveStatus());
  EXPECT_EQ(PrimalFeasibleSolveStatus(InterruptionCause::kTimeLimit),
            PrimalFeasibleSolveStatus(InterruptionCause::kTimeLimit));
  EXPECT_EQ(
      AbnormalSolveStatus(AbnormalityCause::kLuFactorizationPivotTooSmall),
      AbnormalSolveStatus(AbnormalityCause::kLuFactorizationPivotTooSmall));
}

TEST(SolveStatusTest, NotEqOperator) {
  // See comment in SolveStatusTest.EqOperator.
  EXPECT_NE(OptimalSolveStatus(), PrimalUnboundedSolveStatus());
  EXPECT_NE(PrimalFeasibleSolveStatus(InterruptionCause::kTimeLimit),
            PrimalFeasibleSolveStatus(InterruptionCause::kExternal));
  EXPECT_NE(
      AbnormalSolveStatus(AbnormalityCause::kLuFactorizationPivotTooSmall),
      AbnormalSolveStatus(AbnormalityCause::kLpSolverInconsistentSolution));
}

TEST(FeasibleSolveStatusTest, Primal) {
  const SolveStatus solve_status = FeasibleSolveStatus(
      FeasibilityStatus::kPrimal, InterruptionCause::kExternal);
  EXPECT_TRUE(solve_status.Is<SolveStatus::PrimalFeasible>());
  EXPECT_THAT(solve_status.interruption_cause(),
              Optional(InterruptionCause::kExternal));
}

TEST(FeasibleSolveStatusTest, Dual) {
  const SolveStatus solve_status = FeasibleSolveStatus(
      FeasibilityStatus::kDual, InterruptionCause::kExternal);
  EXPECT_TRUE(solve_status.Is<SolveStatus::DualFeasible>());
  EXPECT_THAT(solve_status.interruption_cause(),
              Optional(InterruptionCause::kExternal));
}

// Result of TestGlopReturnIfAbnormal().
template <typename FuncRetType>
struct GlopReturnIfAbnormalTestResult {
  FuncRetType ret_value ABSL_REQUIRE_EXPLICIT_INIT;
  int numCallsBeforeMacro ABSL_REQUIRE_EXPLICIT_INIT;
  int numCallsExpr ABSL_REQUIRE_EXPLICIT_INIT;
  int numCallsAfterMacro ABSL_REQUIRE_EXPLICIT_INIT;
};

// Evaluates:
//
//   FuncRetType func() {
//     ++numCallsBeforeMacro;
//     GLOP_RETURN_IF_ABNORMAL(expr());
//     ++numCallsAfterMacro;
//     return ok_value;
//   }
//
// where expr():
// * returns expr_result
// * and increment numCallsExpr (to validate the macro only evaluates it once).
//
// The FuncRetType is either AbnormalityStatus or SolveStatus.
template <typename FuncRetType>
GlopReturnIfAbnormalTestResult<FuncRetType> TestGlopReturnIfAbnormal(
    const std::optional<AbnormalityCause> expr_result,
    const FuncRetType& ok_value) {
  int numCallsExpr = 0;
  const auto expr = [&]() -> AbnormalityStatus {
    ++numCallsExpr;
    if (expr_result.has_value()) {
      return AbnormalityStatus(*expr_result);
    }
    return AbnormalityStatus{};
  };

  int numCallsBeforeMacro = 0;
  int numCallsAfterMacro = 0;
  auto ret_value = [&]() -> FuncRetType {
    ++numCallsBeforeMacro;
    GLOP_RETURN_IF_ABNORMAL(expr());
    ++numCallsAfterMacro;
    return ok_value;
  }();

  return {
      .ret_value = ret_value,
      .numCallsBeforeMacro = numCallsBeforeMacro,
      .numCallsExpr = numCallsExpr,
      .numCallsAfterMacro = numCallsAfterMacro,
  };
}

TEST(GlopReturnIfAbnormalTest, OkReturnAbnormalityStatus) {
  const auto result = TestGlopReturnIfAbnormal(
      /*expr_result=*/std::nullopt,
      /*ok_value=*/AbnormalityStatus{});
  EXPECT_THAT(result.ret_value, AbnormalityStatusIsOK());
  EXPECT_EQ(result.numCallsBeforeMacro, 1);
  EXPECT_EQ(result.numCallsExpr, 1);
  EXPECT_EQ(result.numCallsAfterMacro, 1);
}

TEST(GlopReturnIfAbnormalTest, ErrorReturnAbnormalityStatus) {
  const auto result = TestGlopReturnIfAbnormal(
      /*expr_result=*/AbnormalityCause::kLuFactorizationPivotTooSmall,
      /*ok_value=*/AbnormalityStatus{});
  EXPECT_THAT(
      result.ret_value,
      AbnormalityStatusIs(AbnormalityCause::kLuFactorizationPivotTooSmall));
  EXPECT_EQ(result.numCallsBeforeMacro, 1);
  EXPECT_EQ(result.numCallsExpr, 1);
  EXPECT_EQ(result.numCallsAfterMacro, 0);
}

TEST(GlopReturnIfAbnormalTest, OkReturnSolveStatus) {
  const auto result = TestGlopReturnIfAbnormal(
      /*expr_result=*/std::nullopt,
      /*ok_value=*/OptimalSolveStatus());
  EXPECT_THAT(result.ret_value, SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(result.numCallsBeforeMacro, 1);
  EXPECT_EQ(result.numCallsExpr, 1);
  EXPECT_EQ(result.numCallsAfterMacro, 1);
}

TEST(GlopReturnIfAbnormalTest, ErrorReturnSolveStatus) {
  const auto result = TestGlopReturnIfAbnormal(
      /*expr_result=*/AbnormalityCause::kLuFactorizationPivotTooSmall,
      /*ok_value=*/OptimalSolveStatus());
  EXPECT_THAT(result.ret_value,
              SolveStatusWithCause<SolveStatus::Abnormal>(
                  AbnormalityCause::kLuFactorizationPivotTooSmall));
  EXPECT_EQ(result.numCallsBeforeMacro, 1);
  EXPECT_EQ(result.numCallsExpr, 1);
  EXPECT_EQ(result.numCallsAfterMacro, 0);
}

}  // namespace
}  // namespace operations_research::glop
