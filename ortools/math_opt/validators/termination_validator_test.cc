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

#include "ortools/math_opt/validators/termination_validator.h"

#include <cmath>
#include <limits>
#include <optional>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

////////////////////////////////////////////////////////////////////////////////
// Parametrized Test Fixtures
////////////////////////////////////////////////////////////////////////////////

class ParametrizeOverMinMaxTest : public ::testing::TestWithParam<bool> {};
INSTANTIATE_TEST_SUITE_P(
    MinMaxValidateTerminationTests, ParametrizeOverMinMaxTest,
    testing::Values(true, false),
    [](const testing::TestParamInfo<ParametrizeOverMinMaxTest::ParamType>&
           info) { return info.param ? "max" : "min"; });

////////////////////////////////////////////////////////////////////////////////
// ValidateTermination -- Simple tests
////////////////////////////////////////////////////////////////////////////////

// More ok tests in ValidateTermination -- Consistency tests.
TEST(ValidateTerminationTest, Ok) {
  TerminationProto termination;
  termination.set_reason(TERMINATION_REASON_INFEASIBLE);
  termination.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_INFEASIBLE);
  termination.mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_INFEASIBLE);
  termination.mutable_objective_bounds()->set_primal_bound(-kInf);
  termination.mutable_objective_bounds()->set_dual_bound(kInf);
  EXPECT_OK(ValidateTermination(termination,
                                /*is_maximize=*/true));
}

TEST_P(ParametrizeOverMinMaxTest, ReasonUnspecified) {
  const bool is_maximize = GetParam();
  TerminationProto termination = UnboundedTerminationProto(is_maximize);
  termination.clear_reason();
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("reason must be specified")));
}

TEST_P(ParametrizeOverMinMaxTest, LimitUnspecifiedForReasonNoSolutionFound) {
  const bool is_maximize = GetParam();
  TerminationProto termination =
      NoSolutionFoundTerminationProto(is_maximize, LIMIT_TIME);
  EXPECT_OK(ValidateTermination(termination, is_maximize));
  termination.clear_limit();
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("limit must be specified")));
}

TEST_P(ParametrizeOverMinMaxTest, LimitUnspecifiedForReasonFeasible) {
  const bool is_maximize = GetParam();
  TerminationProto termination =
      FeasibleTerminationProto(is_maximize, LIMIT_INTERRUPTED, 1.0);
  EXPECT_OK(ValidateTermination(termination, is_maximize));
  termination.clear_limit();
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("limit must be specified")));
}

TEST_P(ParametrizeOverMinMaxTest,
       LimitWithReasonNotLimitFeasibleOrNoSolutionFound) {
  const bool is_maximize = GetParam();
  TerminationProto termination = UnboundedTerminationProto(is_maximize);
  termination.set_limit(LIMIT_TIME);
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("limit should be unspecified")));
}

TEST_P(ParametrizeOverMinMaxTest, ValidCutoffOk) {
  const bool is_maximize = GetParam();
  TerminationProto termination = CutoffTerminationProto(is_maximize);
  EXPECT_OK(ValidateTermination(termination, is_maximize));
}

TEST_P(ParametrizeOverMinMaxTest, InvalidCuttoff) {
  const bool is_maximize = GetParam();
  TerminationProto termination =
      FeasibleTerminationProto(is_maximize, LIMIT_CUTOFF, 1.0);
  EXPECT_THAT(
      ValidateTermination(termination, is_maximize),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("no solutions")));
}

TEST_P(ParametrizeOverMinMaxTest, BoundsChecked) {
  const bool is_maximize = GetParam();
  TerminationProto termination =
      NoSolutionFoundTerminationProto(is_maximize, LIMIT_TIME);
  termination.mutable_objective_bounds()->set_primal_bound(std::nan(""));
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("in primal_bound")));
}

TEST_P(ParametrizeOverMinMaxTest, ProblemStatusChecked) {
  const bool is_maximize = GetParam();
  TerminationProto termination =
      NoSolutionFoundTerminationProto(is_maximize, LIMIT_TIME);
  termination.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_UNSPECIFIED);
  EXPECT_THAT(
      ValidateTermination(termination, is_maximize),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("invalid status FEASIBILITY_STATUS_UNSPECIFIED")));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateTermination -- Consistency
////////////////////////////////////////////////////////////////////////////////

TEST_P(ParametrizeOverMinMaxTest, ValidateBoundStatusConsistencyCalled) {
  // Check CheckPrimalStatusIs(status, FEASIBILITY_STATUS_FEASIBLE) is called.
  const bool is_maximize = GetParam();
  TerminationProto termination = OptimalTerminationProto(10.0, 10.0);
  termination.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_THAT(
      ValidateTermination(termination, is_maximize),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("found primal status = FEASIBILITY_STATUS_UNDETERMINED")));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateTermination -- Consistency -- Optimal
////////////////////////////////////////////////////////////////////////////////

TEST_P(ParametrizeOverMinMaxTest, TerminationConsistencyOkForOptimal) {
  const bool is_maximize = GetParam();
  TerminationProto termination = OptimalTerminationProto(10.0, 10.0);
  EXPECT_OK(ValidateTermination(termination, is_maximize));
  // Trivial dual bound is also ok.
  // TODO(b/290359402): make this invalid if possible.
  termination.mutable_objective_bounds()->set_dual_bound(is_maximize ? kInf
                                                                     : -kInf);
  EXPECT_OK(ValidateTermination(termination, is_maximize));
}

TEST_P(ParametrizeOverMinMaxTest, OptimalButNotPrimalFeasibleStatus) {
  // Check CheckPrimalStatusIs(status, FEASIBILITY_STATUS_FEASIBLE) is called.
  const bool is_maximize = GetParam();
  TerminationProto termination =
      TerminateForReason(is_maximize, TERMINATION_REASON_OPTIMAL);
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.primal_status "
                                       "= FEASIBILITY_STATUS_FEASIBLE"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_OPTIMAL"))));
}

TEST_P(ParametrizeOverMinMaxTest, OptimalButNotDualFeasibleStatus) {
  // Check CheckDualStatusIs(status, FEASIBILITY_STATUS_FEASIBLE) is called.
  const bool is_maximize = GetParam();
  TerminationProto termination =
      TerminateForReason(is_maximize, TERMINATION_REASON_OPTIMAL);
  termination.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.dual_status "
                                       "= FEASIBILITY_STATUS_FEASIBLE"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_OPTIMAL"))));
}

TEST_P(ParametrizeOverMinMaxTest, OptimalInfinitePrimalBound) {
  // Check CheckFinitePrimalBound() is called.
  const bool is_maximize = GetParam();
  TerminationProto termination =
      OptimalTerminationProto(is_maximize ? -kInf : +kInf, 10.0);
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected finite primal bound"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_OPTIMAL"))));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateTermination -- Consistency -- Infeasible
////////////////////////////////////////////////////////////////////////////////

TEST_P(ParametrizeOverMinMaxTest, TerminationConsistencyOkForInfeasible) {
  const bool is_maximize = GetParam();
  const TerminationProto termination_dual_undetermined =
      InfeasibleTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_OK(ValidateTermination(termination_dual_undetermined, is_maximize));
  const TerminationProto termination_dual_feasible = InfeasibleTerminationProto(
      is_maximize,
      /*dual_feasibility_status=*/FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_OK(ValidateTermination(termination_dual_feasible, is_maximize));
  const TerminationProto termination_dual_infeasible =
      InfeasibleTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_OK(ValidateTermination(termination_dual_infeasible, is_maximize));
}

TEST_P(ParametrizeOverMinMaxTest, InfeasibleButNotPrimalInfeasible) {
  // Check CheckPrimalStatusIs(status, FEASIBILITY_STATUS_INFEASIBLE) is called.
  const bool is_maximize = GetParam();
  TerminationProto termination =
      TerminateForReason(is_maximize, TERMINATION_REASON_INFEASIBLE);
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.primal_status "
                                       "= FEASIBILITY_STATUS_INFEASIBLE"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_INFEASIBLE"))));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateTermination -- Consistency -- Unbounded
////////////////////////////////////////////////////////////////////////////////

TEST_P(ParametrizeOverMinMaxTest, TerminationConsistencyOkForUnbounded) {
  const bool is_maximize = GetParam();
  TerminationProto termination = UnboundedTerminationProto(is_maximize);
  EXPECT_OK(ValidateTermination(termination, is_maximize));
}

TEST_P(ParametrizeOverMinMaxTest, UnboundedButNotPrimalFeasible) {
  // Check CheckPrimalStatusIs(status, FEASIBILITY_STATUS_FEASIBLE) is called.
  const bool is_maximize = GetParam();
  TerminationProto termination =
      TerminateForReason(is_maximize, TERMINATION_REASON_UNBOUNDED);
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.primal_status "
                                       "= FEASIBILITY_STATUS_FEASIBLE"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_UNBOUNDED"))));
}

TEST_P(ParametrizeOverMinMaxTest, UnboundedButNotDualInfeasible) {
  // Check CheckDualStatusIs(status, FEASIBILITY_STATUS_INFEASIBLE) is called.
  const bool is_maximize = GetParam();
  TerminationProto termination =
      TerminateForReason(is_maximize, TERMINATION_REASON_UNBOUNDED);
  termination.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.dual_status "
                                       "= FEASIBILITY_STATUS_INFEASIBLE"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_UNBOUNDED"))));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateTermination -- Consistency -- InfeasibleOrUnbounded
////////////////////////////////////////////////////////////////////////////////

TEST_P(ParametrizeOverMinMaxTest,
       TerminationConsistencyOkForInfeasibleOrUnbounded) {
  const bool is_maximize = GetParam();
  TerminationProto termination_dual_infeasible =
      InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_OK(ValidateTermination(termination_dual_infeasible, is_maximize));

  TerminationProto termination_dual_undetermined =
      InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED);
  EXPECT_OK(ValidateTermination(termination_dual_undetermined, is_maximize));
}

TEST_P(ParametrizeOverMinMaxTest, InfOrUnbButNotPrimalUndetermined) {
  // Check CheckPrimalStatusIs(status, FEASIBILITY_STATUS_UNDETERMINED) is
  // called.
  const bool is_maximize = GetParam();
  TerminationProto termination = InfeasibleOrUnboundedTerminationProto(
      is_maximize,
      /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE);
  termination.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_THAT(
      ValidateTermination(termination, is_maximize),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("expected problem_status.primal_status "
                               "= FEASIBILITY_STATUS_UNDETERMINED"),
                     HasSubstr("for termination reason "
                               "TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED"))));
}

TEST_P(ParametrizeOverMinMaxTest, InfOrUnbButNotDualInfeasibleOrInfOrUnb) {
  // Check CheckDualStatusIs(status, FEASIBILITY_STATUS_INFEASIBLE,
  //                          /*primal_or_dual_infeasible_also_ok=*/true)) is
  // called.
  const bool is_maximize = GetParam();
  TerminationProto termination = InfeasibleOrUnboundedTerminationProto(
      is_maximize,
      /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED);
  termination.mutable_problem_status()->set_primal_or_dual_infeasible(false);
  EXPECT_THAT(
      ValidateTermination(termination, is_maximize),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("expected either problem_status.dual_status = "
                               "FEASIBILITY_STATUS_INFEASIBLE"),
                     HasSubstr("or problem_status.primal_or_dual_infeasible "
                               "= true"),
                     HasSubstr("for termination reason "
                               "TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED"))));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateTermination -- Consistency -- Imprecise
////////////////////////////////////////////////////////////////////////////////

TEST_P(ParametrizeOverMinMaxTest, TerminationConsistencyOkForImprecise) {
  const bool is_maximize = GetParam();
  TerminationProto termination =
      TerminateForReason(is_maximize, TERMINATION_REASON_IMPRECISE);
  EXPECT_OK(ValidateTermination(termination, is_maximize));
}

TEST_P(ParametrizeOverMinMaxTest, ImpreciserButNotPrimalUndetermined) {
  // Check CheckPrimalStatusIs(status, FEASIBILITY_STATUS_UNDETERMINED) is
  // called.
  const bool is_maximize = GetParam();
  TerminationProto termination =
      TerminateForReason(is_maximize, TERMINATION_REASON_IMPRECISE);
  termination.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_FEASIBLE);
  termination.mutable_objective_bounds()->set_primal_bound(10.0);
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.primal_status "
                                       "= FEASIBILITY_STATUS_UNDETERMINED"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_IMPRECISE"))));
}

TEST_P(ParametrizeOverMinMaxTest, ImpreciseButNotDualUndetermined) {
  // Check CheckDualStatusIs(status, FEASIBILITY_STATUS_UNDETERMINED) is
  // called.
  const bool is_maximize = GetParam();
  TerminationProto termination =
      TerminateForReason(is_maximize, TERMINATION_REASON_IMPRECISE);
  termination.mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.dual_status "
                                       "= FEASIBILITY_STATUS_UNDETERMINED"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_IMPRECISE"))));
}

TEST_P(ParametrizeOverMinMaxTest, ImpreciseButPrimalDualInf) {
  // Check CheckNotPrimalDualInfeasible() is called.
  const bool is_maximize = GetParam();
  TerminationProto termination =
      TerminateForReason(is_maximize, TERMINATION_REASON_IMPRECISE);
  termination.mutable_problem_status()->set_primal_or_dual_infeasible(true);
  EXPECT_THAT(
      ValidateTermination(termination, is_maximize),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("expected primal_or_dual_infeasible = false"),
                     HasSubstr("for termination reason "
                               "TERMINATION_REASON_IMPRECISE"))));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateTermination -- Consistency -- Feasible
////////////////////////////////////////////////////////////////////////////////

TEST_P(ParametrizeOverMinMaxTest, TerminationConsistencyOkForFeasible) {
  const bool is_maximize = GetParam();
  TerminationProto termination_no_dual_bound = FeasibleTerminationProto(
      is_maximize, LIMIT_OTHER, /*finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/std::nullopt);
  EXPECT_OK(ValidateTermination(termination_no_dual_bound, is_maximize));

  TerminationProto termination_finite_dual_bound = FeasibleTerminationProto(
      is_maximize, LIMIT_OTHER, /*finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/10.0);
  EXPECT_OK(ValidateTermination(termination_finite_dual_bound, is_maximize));

  TerminationProto termination_infinite_dual_bound = FeasibleTerminationProto(
      is_maximize, LIMIT_OTHER, /*finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/is_maximize ? kInf : -kInf);
  EXPECT_OK(ValidateTermination(termination_infinite_dual_bound, is_maximize));
}

TEST_P(ParametrizeOverMinMaxTest, FeasibleButNotPrimalFeasibleStatus) {
  // Check CheckPrimalStatusIs(status, FEASIBILITY_STATUS_FEASIBLE) is
  // called.
  const bool is_maximize = GetParam();
  TerminationProto termination = FeasibleTerminationProto(
      is_maximize, LIMIT_OTHER, /*finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/std::nullopt);
  termination.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_UNDETERMINED);
  termination.mutable_objective_bounds()->set_primal_bound(is_maximize ? -kInf
                                                                       : kInf);
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.primal_status "
                                       "= FEASIBILITY_STATUS_FEASIBLE"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_FEASIBLE"))));
}

TEST_P(ParametrizeOverMinMaxTest, FeasibleButDualInfeasible) {
  // Check CheckDualStatusIsNot(status, FEASIBILITY_STATUS_INFEASIBLE) is
  // called.
  const bool is_maximize = GetParam();
  TerminationProto termination = FeasibleTerminationProto(
      is_maximize, LIMIT_OTHER, /*finite_primal_objective=*/10.0,
      /*optional_dual_objective=*/std::nullopt);
  termination.mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_INFEASIBLE);
  termination.mutable_objective_bounds()->set_primal_bound(is_maximize ? kInf
                                                                       : -kInf);
  termination.mutable_objective_bounds()->set_dual_bound(is_maximize ? kInf
                                                                     : -kInf);
  EXPECT_THAT(ValidateTermination(termination, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.dual_status != "
                                       "FEASIBILITY_STATUS_INFEASIBLE"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_FEASIBLE"))));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateTermination -- Consistency -- NoSolutionFound
////////////////////////////////////////////////////////////////////////////////

TEST_P(ParametrizeOverMinMaxTest, TerminationConsistencyOkForNoSolutionFound) {
  const bool is_maximize = GetParam();
  TerminationProto termination_no_dual_bound = NoSolutionFoundTerminationProto(
      is_maximize, LIMIT_OTHER, /*optional_dual_objective=*/std::nullopt);
  EXPECT_OK(ValidateTermination(termination_no_dual_bound, is_maximize));

  TerminationProto termination_finite_dual_bound =
      NoSolutionFoundTerminationProto(is_maximize, LIMIT_OTHER,
                                      /*optional_dual_objective=*/10.0);
  EXPECT_OK(ValidateTermination(termination_finite_dual_bound, is_maximize));

  TerminationProto termination_infinite_dual_bound =
      NoSolutionFoundTerminationProto(
          is_maximize, LIMIT_OTHER,
          /*optional_dual_objective=*/is_maximize ? kInf : -kInf);
  EXPECT_OK(ValidateTermination(termination_infinite_dual_bound, is_maximize));

  // Primal feasible also ok.
  TerminationProto termination_primal_feasible =
      NoSolutionFoundTerminationProto(is_maximize, LIMIT_OTHER,
                                      /*optional_dual_objective=*/std::nullopt);
  termination_primal_feasible.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_OK(ValidateTermination(termination_primal_feasible, is_maximize));
}

TEST_P(ParametrizeOverMinMaxTest, NoSolutionFoundButPrimalInfeasible) {
  // Check CheckPrimalStatusIsNot(status, FEASIBILITY_STATUS_INFEASIBLE) is
  // called.
  const bool is_maximize = GetParam();
  TerminationProto termination = NoSolutionFoundTerminationProto(
      is_maximize, LIMIT_OTHER, /*optional_dual_objective=*/std::nullopt);
  termination.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_INFEASIBLE);
  EXPECT_THAT(
      ValidateTermination(termination, is_maximize),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("expected problem_status.primal_status "
                               "!= FEASIBILITY_STATUS_INFEASIBLE"),
                     HasSubstr("for termination reason "
                               "TERMINATION_REASON_NO_SOLUTION_FOUND"))));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateTermination -- Consistency -- Numerical and other errors
////////////////////////////////////////////////////////////////////////////////

TEST_P(ParametrizeOverMinMaxTest, TerminationConsistencyOkForErrors) {
  const bool is_maximize = GetParam();
  TerminationProto termination_numerical =
      TerminateForReason(is_maximize, TERMINATION_REASON_NUMERICAL_ERROR);
  EXPECT_OK(ValidateTermination(termination_numerical, is_maximize));

  TerminationProto termination_other =
      TerminateForReason(is_maximize, TERMINATION_REASON_OTHER_ERROR);
  EXPECT_OK(ValidateTermination(termination_other, is_maximize));
}

TEST_P(ParametrizeOverMinMaxTest, ErrorButNotPrimalUndetermined) {
  // Check CheckPrimalStatusIs(status, FEASIBILITY_STATUS_UNDETERMINED) is
  // called.
  const bool is_maximize = GetParam();
  TerminationProto termination_numerical =
      TerminateForReason(is_maximize, TERMINATION_REASON_NUMERICAL_ERROR);
  termination_numerical.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_THAT(ValidateTermination(termination_numerical, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.primal_status "
                                       "= FEASIBILITY_STATUS_UNDETERMINED"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_NUMERICAL_ERROR"))));

  TerminationProto termination_other =
      TerminateForReason(is_maximize, TERMINATION_REASON_OTHER_ERROR);
  termination_other.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_THAT(ValidateTermination(termination_other, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.primal_status "
                                       "= FEASIBILITY_STATUS_UNDETERMINED"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_OTHER_ERROR"))));
}

TEST_P(ParametrizeOverMinMaxTest, ErrorButNotDualUndetermined) {
  // Check CheckDualStatusIs(status, FEASIBILITY_STATUS_UNDETERMINED) is
  // called.
  const bool is_maximize = GetParam();
  TerminationProto termination_numerical =
      TerminateForReason(is_maximize, TERMINATION_REASON_NUMERICAL_ERROR);
  termination_numerical.mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_THAT(ValidateTermination(termination_numerical, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.dual_status "
                                       "= FEASIBILITY_STATUS_UNDETERMINED"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_NUMERICAL_ERROR"))));

  TerminationProto termination_other =
      TerminateForReason(is_maximize, TERMINATION_REASON_OTHER_ERROR);
  termination_other.mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_FEASIBLE);
  EXPECT_THAT(ValidateTermination(termination_other, is_maximize),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("expected problem_status.dual_status "
                                       "= FEASIBILITY_STATUS_UNDETERMINED"),
                             HasSubstr("for termination reason "
                                       "TERMINATION_REASON_OTHER_ERROR"))));
}

TEST_P(ParametrizeOverMinMaxTest, ErrorButPrimalDualInf) {
  // Check CheckNotPrimalDualInfeasible() is called.
  const bool is_maximize = GetParam();
  TerminationProto termination_numerical =
      TerminateForReason(is_maximize, TERMINATION_REASON_NUMERICAL_ERROR);
  termination_numerical.mutable_problem_status()->set_primal_or_dual_infeasible(
      true);
  EXPECT_THAT(
      ValidateTermination(termination_numerical, is_maximize),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("expected primal_or_dual_infeasible = false"),
                     HasSubstr("for termination reason "
                               "TERMINATION_REASON_NUMERICAL_ERROR"))));

  TerminationProto termination_other =
      TerminateForReason(is_maximize, TERMINATION_REASON_OTHER_ERROR);
  termination_other.mutable_problem_status()->set_primal_or_dual_infeasible(
      true);
  EXPECT_THAT(
      ValidateTermination(termination_other, is_maximize),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("expected primal_or_dual_infeasible = false"),
                     HasSubstr("for termination reason "
                               "TERMINATION_REASON_OTHER_ERROR"))));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
