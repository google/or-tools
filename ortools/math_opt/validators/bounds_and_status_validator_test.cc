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

#include "ortools/math_opt/validators/bounds_and_status_validator.h"

#include <cmath>
#include <limits>
#include <tuple>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/validators/enum_sets.h"

namespace operations_research::math_opt {
namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();

using ::google::protobuf::contrib::parse_proto::ParseTextProto;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

////////////////////////////////////////////////////////////////////////////////
// Parametrized Test Fixtures
////////////////////////////////////////////////////////////////////////////////

class AllFeasibilityStatusParamTest
    : public testing::TestWithParam<FeasibilityStatusProto> {};
INSTANTIATE_TEST_SUITE_P(AllFeasibilityStatusParamTests,
                         AllFeasibilityStatusParamTest,
                         testing::ValuesIn(AllFeasibilityStatuses()),
                         testing::PrintToStringParamName());

class AllPairsOfFeasibilityStatusParamTest
    : public testing::TestWithParam<
          std::tuple<FeasibilityStatusProto, FeasibilityStatusProto>> {};
INSTANTIATE_TEST_SUITE_P(
    AllPairsOfFeasibilityStatusParamTests, AllPairsOfFeasibilityStatusParamTest,
    testing::Combine(testing::ValuesIn(AllFeasibilityStatuses()),
                     testing::ValuesIn(AllFeasibilityStatuses())),
    [](const testing::TestParamInfo<
        AllPairsOfFeasibilityStatusParamTest::ParamType>& info) {
      return absl::StrCat(FeasibilityStatusProto_Name(std::get<0>(info.param)),
                          "_",
                          FeasibilityStatusProto_Name(std::get<1>(info.param)));
    });

class ParametrizeOverMinMaxTest : public ::testing::TestWithParam<bool> {};
INSTANTIATE_TEST_SUITE_P(
    AllMinMaxTests, ParametrizeOverMinMaxTest, testing::Values(true, false),
    [](const testing::TestParamInfo<ParametrizeOverMinMaxTest::ParamType>&
           info) { return info.param ? "max" : "min"; });

class AllStatusesAndMinMaxParamTest
    : public testing::TestWithParam<
          std::tuple<bool, FeasibilityStatusProto, FeasibilityStatusProto>> {};
INSTANTIATE_TEST_SUITE_P(
    AllStatusesAndMinMaxParamTests, AllStatusesAndMinMaxParamTest,
    testing::Combine(testing::ValuesIn({true, false}),
                     testing::ValuesIn(AllFeasibilityStatuses()),
                     testing::ValuesIn(AllFeasibilityStatuses())),
    [](const testing::TestParamInfo<AllStatusesAndMinMaxParamTest::ParamType>&
           info) {
      return absl::StrCat((std::get<0>(info.param) ? "Max" : "Min"),
                          FeasibilityStatusProto_Name(std::get<1>(info.param)),
                          "_",
                          FeasibilityStatusProto_Name(std::get<2>(info.param)));
    });

////////////////////////////////////////////////////////////////////////////////
// ValidateFeasibilityStatus
////////////////////////////////////////////////////////////////////////////////

TEST(ValidateFeasibilityStatusTest, FeasibilityStatusOk) {
  EXPECT_OK(ValidateFeasibilityStatus(FEASIBILITY_STATUS_FEASIBLE));
  EXPECT_THAT(
      ValidateFeasibilityStatus(FEASIBILITY_STATUS_UNSPECIFIED),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("invalid status FEASIBILITY_STATUS_UNSPECIFIED")));
  EXPECT_THAT(ValidateFeasibilityStatus(FeasibilityStatusProto{-1}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid status -1")));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateProblemStatus
////////////////////////////////////////////////////////////////////////////////

TEST(ValidateProblemStatusTest, ProblemStatusOkSimple) {
  ASSERT_OK_AND_ASSIGN(const auto problem_status,
                       ParseTextProto<ProblemStatusProto>(R"pb(
                         primal_status: FEASIBILITY_STATUS_FEASIBLE,
                         dual_status: FEASIBILITY_STATUS_FEASIBLE,
                         primal_or_dual_infeasible: 0
                       )pb"));
  EXPECT_OK(ValidateProblemStatus(problem_status));
}

TEST(ValidateProblemStatusTest, ProblemStatusOkPrimalDualInf) {
  ASSERT_OK_AND_ASSIGN(const auto problem_status,
                       ParseTextProto<ProblemStatusProto>(R"pb(
                         primal_status: FEASIBILITY_STATUS_UNDETERMINED,
                         dual_status: FEASIBILITY_STATUS_UNDETERMINED,
                         primal_or_dual_infeasible: 1
                       )pb"));
  EXPECT_OK(ValidateProblemStatus(problem_status));
}

TEST(ValidateProblemStatusTest, PrimalStatusIsChecked) {
  ASSERT_OK_AND_ASSIGN(const auto problem_status,
                       ParseTextProto<ProblemStatusProto>(R"pb(
                         primal_status: FEASIBILITY_STATUS_UNSPECIFIED,
                         dual_status: FEASIBILITY_STATUS_FEASIBLE,
                         primal_or_dual_infeasible: 0
                       )pb"));
  EXPECT_THAT(
      ValidateProblemStatus(problem_status),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("invalid status FEASIBILITY_STATUS_UNSPECIFIED"),
                     HasSubstr("primal_status"))));
}

TEST(ValidateProblemStatusTest, DualStatusIsChecked) {
  ASSERT_OK_AND_ASSIGN(const auto problem_status,
                       ParseTextProto<ProblemStatusProto>(R"pb(
                         primal_status: FEASIBILITY_STATUS_FEASIBLE,
                         dual_status: FEASIBILITY_STATUS_UNSPECIFIED,
                         primal_or_dual_infeasible: 0
                       )pb"));
  EXPECT_THAT(
      ValidateProblemStatus(problem_status),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("invalid status FEASIBILITY_STATUS_UNSPECIFIED"),
                     HasSubstr("dual_status"))));
}

TEST_P(AllFeasibilityStatusParamTest, IncompatiblePrimalStatusForInfOrUnb) {
  const FeasibilityStatusProto primal_status = GetParam();
  if (primal_status == FEASIBILITY_STATUS_UNDETERMINED) {
    GTEST_SKIP();
  }
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(primal_status);
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_primal_or_dual_infeasible(true);
  EXPECT_THAT(
      ValidateProblemStatus(problem_status),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("primal_or_dual_infeasible can be true"),
                HasSubstr(
                    absl::StrCat("primal status = ",
                                 FeasibilityStatusProto_Name(primal_status))),
                HasSubstr("dual status = FEASIBILITY_STATUS_UNDETERMINED"))));
}

TEST_P(AllFeasibilityStatusParamTest, IncompatibleDualStatusForInfOrUnb) {
  const FeasibilityStatusProto dual_status = GetParam();
  if (dual_status == FEASIBILITY_STATUS_UNDETERMINED) {
    GTEST_SKIP();
  }
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(dual_status);
  problem_status.set_primal_or_dual_infeasible(true);
  EXPECT_THAT(
      ValidateProblemStatus(problem_status),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("primal_or_dual_infeasible can be true"),
                HasSubstr("primal status = FEASIBILITY_STATUS_UNDETERMINED"),
                HasSubstr(
                    absl::StrCat("dual status = ",
                                 FeasibilityStatusProto_Name(dual_status))))));
}

////////////////////////////////////////////////////////////////////////////////
// CheckPrimalStatusIs
////////////////////////////////////////////////////////////////////////////////

TEST(CheckPrimalStatusIsTest, OkSimpleForm) {
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
  problem_status.set_dual_status(FEASIBILITY_STATUS_INFEASIBLE);

  EXPECT_OK(CheckPrimalStatusIs(problem_status, FEASIBILITY_STATUS_FEASIBLE));
}

TEST(CheckPrimalStatusIsTest, OkPrimalDualInfForm) {
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_primal_or_dual_infeasible(true);
  EXPECT_OK(
      CheckPrimalStatusIs(problem_status, FEASIBILITY_STATUS_UNDETERMINED));
}

TEST_P(AllPairsOfFeasibilityStatusParamTest,
       CheckPrimalStatusIsNotOkSimpleForm) {
  const auto [actual_feasibility_status, expected_feasibility_status] =
      GetParam();
  if (actual_feasibility_status == expected_feasibility_status) {
    GTEST_SKIP();
  }
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(actual_feasibility_status);
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);

  EXPECT_THAT(CheckPrimalStatusIs(problem_status, expected_feasibility_status),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected problem_status.primal_status")));
}

////////////////////////////////////////////////////////////////////////////////
// CheckPrimalStatusIsNot
////////////////////////////////////////////////////////////////////////////////

TEST(CheckPrimalStatusIsNotTest, OkSimpleForm) {
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
  problem_status.set_dual_status(FEASIBILITY_STATUS_INFEASIBLE);

  EXPECT_OK(
      CheckPrimalStatusIsNot(problem_status, FEASIBILITY_STATUS_INFEASIBLE));
}

TEST(CheckPrimalStatusIsNotTest, OkPrimalDualInfForm) {
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_primal_or_dual_infeasible(true);
  EXPECT_OK(
      CheckPrimalStatusIsNot(problem_status, FEASIBILITY_STATUS_FEASIBLE));
}

TEST_P(AllFeasibilityStatusParamTest, CheckPrimalStatusIsNotNotOkSimpleForm) {
  const FeasibilityStatusProto feasibility_status = GetParam();
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(feasibility_status);
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);

  EXPECT_THAT(CheckPrimalStatusIsNot(problem_status, feasibility_status),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected problem_status.primal_status !=")));
}

///////////////////////////////////////////////////////////////////////////////
// CheckDualStatusIs
///////////////////////////////////////////////////////////////////////////////

TEST(CheckDualStatusIsTest, OkSimpleForm) {
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
  problem_status.set_dual_status(FEASIBILITY_STATUS_INFEASIBLE);

  EXPECT_OK(CheckDualStatusIs(problem_status, FEASIBILITY_STATUS_INFEASIBLE,
                              /*primal_or_dual_infeasible_also_ok=*/false));
}

TEST(CheckDualStatusIsTest, OkPrimalDualInfForm) {
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_primal_or_dual_infeasible(true);
  EXPECT_OK(CheckDualStatusIs(problem_status, FEASIBILITY_STATUS_INFEASIBLE,
                              /*primal_or_dual_infeasible_also_ok=*/true));
}

TEST_P(AllPairsOfFeasibilityStatusParamTest, CheckDualStatusIsNotOkSimpleForm) {
  const auto [actual_feasibility_status, expected_feasibility_status] =
      GetParam();
  if (actual_feasibility_status == expected_feasibility_status) {
    GTEST_SKIP();
  }
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(actual_feasibility_status);

  EXPECT_THAT(CheckDualStatusIs(problem_status, expected_feasibility_status,
                                /*primal_or_dual_infeasible_also_ok=*/false),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected problem_status.dual_status")));
}

TEST_P(AllPairsOfFeasibilityStatusParamTest,
       CheckDualStatusIsNotOkPrimalDualInfForm) {
  const auto [actual_feasibility_status, expected_feasibility_status] =
      GetParam();
  if (actual_feasibility_status == expected_feasibility_status) {
    GTEST_SKIP();
  }

  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(actual_feasibility_status);

  EXPECT_THAT(
      CheckDualStatusIs(problem_status, expected_feasibility_status,
                        /*primal_or_dual_infeasible_also_ok=*/true),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("expected either problem_status.dual_status")));
}

////////////////////////////////////////////////////////////////////////////////
// CheckDualStatusIsNot
////////////////////////////////////////////////////////////////////////////////

TEST(CheckDualStatusIsNotTest, OkSimpleForm) {
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
  problem_status.set_dual_status(FEASIBILITY_STATUS_INFEASIBLE);

  EXPECT_OK(CheckDualStatusIsNot(problem_status, FEASIBILITY_STATUS_FEASIBLE));
}

TEST(CheckDualStatusIsNotTest, OkPrimalDualInfForm) {
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_primal_or_dual_infeasible(true);
  EXPECT_OK(CheckDualStatusIsNot(problem_status, FEASIBILITY_STATUS_FEASIBLE));
}

TEST_P(AllFeasibilityStatusParamTest, CheckDualStatusIsNotNotOkSimpleForm) {
  const FeasibilityStatusProto feasibility_status = GetParam();
  ProblemStatusProto problem_status;
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(feasibility_status);

  EXPECT_THAT(CheckDualStatusIsNot(problem_status, feasibility_status),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected problem_status.dual_status !=")));
}

////////////////////////////////////////////////////////////////////////////////
// BoundStatusConsistency
////////////////////////////////////////////////////////////////////////////////

ProblemStatusProto MakeStatus(const FeasibilityStatusProto primal_status,
                              const FeasibilityStatusProto dual_status) {
  ProblemStatusProto status;
  status.set_primal_status(primal_status);
  status.set_dual_status(dual_status);
  return status;
}

ObjectiveBoundsProto MakeBound(const double primal_bound,
                               const double dual_bound) {
  ObjectiveBoundsProto bounds;
  bounds.set_primal_bound(primal_bound);
  bounds.set_dual_bound(dual_bound);
  return bounds;
}

////////////////////////////////////////////////////////////////////////////////
// ValidateBoundStatusConsistency
////////////////////////////////////////////////////////////////////////////////

TEST_P(AllStatusesAndMinMaxParamTest,
       ValidateBoundStatusConsistencyFinitePrimalAndTrivialDual) {
  const auto [is_maximize, primal_status, dual_status] = GetParam();
  const ObjectiveBoundsProto bounds =
      MakeBound(/*primal_bound=*/10.0,
                /*dual_bound=*/is_maximize ? +kInf : -kInf);

  if (primal_status != FEASIBILITY_STATUS_FEASIBLE) {
    EXPECT_THAT(
        ValidateBoundStatusConsistency(
            bounds,
            MakeStatus(primal_status,
                       /*dual_status=*/FEASIBILITY_STATUS_UNDETERMINED),
            is_maximize),
        StatusIs(
            absl::StatusCode::kInvalidArgument,
            AllOf(HasSubstr(
                      "expected primal status = FEASIBILITY_STATUS_FEASIBLE"),
                  HasSubstr(absl::StrCat("finite primal bound = ",
                                         bounds.primal_bound())),
                  HasSubstr(absl::StrCat(
                      "primal status = ",
                      FeasibilityStatusProto_Name(primal_status))))));
  } else {
    if (dual_status == FEASIBILITY_STATUS_INFEASIBLE) {
      EXPECT_THAT(
          ValidateBoundStatusConsistency(
              bounds, MakeStatus(primal_status, dual_status), is_maximize),
          StatusIs(
              absl::StatusCode::kInvalidArgument,
              AllOf(HasSubstr("unexpected (primal status, dual status) = "
                              "(FEASIBILITY_STATUS_FEASIBLE, "
                              "FEASIBILITY_STATUS_INFEASIBLE)"),
                    HasSubstr(absl::StrCat("for not-unbounded primal bound = ",
                                           bounds.primal_bound())),
                    HasSubstr(absl::StrCat(
                        "is_maximize = ", (is_maximize ? "true" : "false"))))));
    } else {
      EXPECT_OK(ValidateBoundStatusConsistency(
          bounds, MakeStatus(primal_status, dual_status), is_maximize));
    }
  }
}

TEST_P(AllStatusesAndMinMaxParamTest,
       ValidateBoundStatusConsistencyFiniteDualAndTrivialPrimal) {
  const auto [is_maximize, primal_status, dual_status] = GetParam();
  const ObjectiveBoundsProto bounds =
      MakeBound(/*primal_bound=*/is_maximize ? -kInf : +kInf,
                /*dual_bound=*/10.0);

  if (dual_status != FEASIBILITY_STATUS_FEASIBLE) {
    EXPECT_THAT(
        ValidateBoundStatusConsistency(
            bounds,
            MakeStatus(/*primal_status=*/FEASIBILITY_STATUS_UNDETERMINED,
                       dual_status),
            is_maximize),
        StatusIs(
            absl::StatusCode::kInvalidArgument,
            AllOf(
                HasSubstr("expected dual status = FEASIBILITY_STATUS_FEASIBLE"),
                HasSubstr(
                    absl::StrCat("finite dual bound = ", bounds.dual_bound())),
                HasSubstr(
                    absl::StrCat("dual status = ",
                                 FeasibilityStatusProto_Name(dual_status))))));
  } else {
    if (primal_status == FEASIBILITY_STATUS_INFEASIBLE) {
      EXPECT_THAT(
          ValidateBoundStatusConsistency(
              bounds, MakeStatus(primal_status, dual_status), is_maximize),
          StatusIs(
              absl::StatusCode::kInvalidArgument,
              AllOf(HasSubstr("unexpected (dual status, primal status) = "
                              "(FEASIBILITY_STATUS_FEASIBLE, "
                              "FEASIBILITY_STATUS_INFEASIBLE)"),
                    HasSubstr(absl::StrCat("for not-unbounded dual bound = ",
                                           bounds.dual_bound())),
                    HasSubstr(absl::StrCat(
                        "is_maximize = ", (is_maximize ? "true" : "false"))))));
    } else {
      EXPECT_OK(ValidateBoundStatusConsistency(
          bounds, MakeStatus(primal_status, dual_status), is_maximize));
    }
  }
}

TEST_P(AllStatusesAndMinMaxParamTest,
       ValidateBoundStatusConsistencyTrivialPrimalAndDual) {
  const auto [is_maximize, primal_status, dual_status] = GetParam();
  const ObjectiveBoundsProto bounds =
      MakeBound(/*primal_bound=*/is_maximize ? -kInf : +kInf,
                /*dual_bound=*/is_maximize ? +kInf : -kInf);
  if ((primal_status == FEASIBILITY_STATUS_FEASIBLE &&
       dual_status == FEASIBILITY_STATUS_INFEASIBLE) ||
      (dual_status == FEASIBILITY_STATUS_FEASIBLE &&
       primal_status == FEASIBILITY_STATUS_INFEASIBLE)) {
    return;
  }
  EXPECT_OK(ValidateBoundStatusConsistency(
      bounds, MakeStatus(primal_status, dual_status), is_maximize));
}

TEST_P(ParametrizeOverMinMaxTest,
       ValidateBoundStatusConsistencyFinitePrimalAndDual) {
  const bool is_maximize = GetParam();
  EXPECT_OK(ValidateBoundStatusConsistency(
      MakeBound(/*primal_bound=*/10.0,
                /*dual_bound=*/10.0),
      MakeStatus(FEASIBILITY_STATUS_FEASIBLE, FEASIBILITY_STATUS_FEASIBLE),
      is_maximize));
}

TEST_P(AllStatusesAndMinMaxParamTest,
       ValidateBoundStatusConsistencyPrimalUnboundedStatusCheck) {
  const auto [is_maximize, primal_status, dual_status] = GetParam();
  const ObjectiveBoundsProto bounds =
      MakeBound(/*primal_bound=*/is_maximize ? +kInf : -kInf,
                /*dual_bound=*/is_maximize ? +kInf : -kInf);
  if (primal_status != FEASIBILITY_STATUS_FEASIBLE) {
    EXPECT_THAT(
        ValidateBoundStatusConsistency(
            bounds, MakeStatus(primal_status, dual_status), is_maximize),
        StatusIs(
            absl::StatusCode::kInvalidArgument,
            AllOf(HasSubstr(
                      "expected primal status = FEASIBILITY_STATUS_FEASIBLE"),
                  HasSubstr(absl::StrCat("for unbounded primal bound = ",
                                         bounds.primal_bound())),
                  HasSubstr(
                      absl::StrCat("found primal status = ",
                                   FeasibilityStatusProto_Name(primal_status))),
                  HasSubstr(absl::StrCat("is_maximize = ",
                                         (is_maximize ? "true" : "false"))))));
  } else if (dual_status != FEASIBILITY_STATUS_INFEASIBLE) {
    EXPECT_THAT(
        ValidateBoundStatusConsistency(
            bounds, MakeStatus(primal_status, dual_status), is_maximize),
        StatusIs(
            absl::StatusCode::kInvalidArgument,
            AllOf(HasSubstr(
                      "expected dual status = FEASIBILITY_STATUS_INFEASIBLE"),
                  HasSubstr(absl::StrCat("for unbounded primal bound = ",
                                         bounds.primal_bound())),
                  HasSubstr(
                      absl::StrCat("found dual status = ",
                                   FeasibilityStatusProto_Name(dual_status))),
                  HasSubstr(absl::StrCat("is_maximize = ",
                                         (is_maximize ? "true" : "false"))))));
  } else {
    EXPECT_OK(ValidateBoundStatusConsistency(
        bounds, MakeStatus(primal_status, dual_status), is_maximize));
  }
}

TEST_P(ParametrizeOverMinMaxTest,
       ValidateBoundStatusConsistencyPrimalUnboundedBoundCheck) {
  const bool is_maximize = GetParam();
  const double primal_bound = is_maximize ? +kInf : -kInf;
  const double dual_bound = 10.0;
  EXPECT_THAT(
      ValidateBoundStatusConsistency(MakeBound(primal_bound, dual_bound),
                                     MakeStatus(FEASIBILITY_STATUS_FEASIBLE,
                                                FEASIBILITY_STATUS_INFEASIBLE),
                                     is_maximize),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("expected dual bound = primal bound"),
                HasSubstr(absl::StrCat("for unbounded primal bound = ",
                                       primal_bound)),
                HasSubstr(absl::StrCat("found dual bound = ", dual_bound)),
                HasSubstr(absl::StrCat("is_maximize = ",
                                       (is_maximize ? "true" : "false"))))));
}

TEST_P(AllStatusesAndMinMaxParamTest,
       ValidateBoundStatusConsistencyDualUnboundedStatusCheck) {
  const auto [is_maximize, primal_status, dual_status] = GetParam();
  const ObjectiveBoundsProto bounds =
      MakeBound(/*primal_bound=*/is_maximize ? -kInf : +kInf,
                /*dual_bound=*/is_maximize ? -kInf : +kInf);
  if (primal_status == FEASIBILITY_STATUS_FEASIBLE &&
      dual_status == FEASIBILITY_STATUS_INFEASIBLE) {
    return;
  }

  if (dual_status != FEASIBILITY_STATUS_FEASIBLE) {
    EXPECT_THAT(
        ValidateBoundStatusConsistency(
            bounds, MakeStatus(primal_status, dual_status), is_maximize),
        StatusIs(
            absl::StatusCode::kInvalidArgument,
            AllOf(
                HasSubstr("expected dual status = FEASIBILITY_STATUS_FEASIBLE"),
                HasSubstr(absl::StrCat("for unbounded dual bound = ",
                                       bounds.dual_bound())),
                HasSubstr(
                    absl::StrCat("found dual status = ",
                                 FeasibilityStatusProto_Name(dual_status))),
                HasSubstr(absl::StrCat("is_maximize = ",
                                       (is_maximize ? "true" : "false"))))));
  } else if (primal_status != FEASIBILITY_STATUS_INFEASIBLE) {
    EXPECT_THAT(
        ValidateBoundStatusConsistency(
            bounds, MakeStatus(primal_status, dual_status), is_maximize),
        StatusIs(
            absl::StatusCode::kInvalidArgument,
            AllOf(HasSubstr(
                      "expected primal status = FEASIBILITY_STATUS_INFEASIBLE"),
                  HasSubstr(absl::StrCat("for unbounded dual bound = ",
                                         bounds.dual_bound())),
                  HasSubstr(
                      absl::StrCat("found primal status = ",
                                   FeasibilityStatusProto_Name(primal_status))),
                  HasSubstr(absl::StrCat("is_maximize = ",
                                         (is_maximize ? "true" : "false"))))));
  } else {
    EXPECT_OK(ValidateBoundStatusConsistency(
        bounds, MakeStatus(primal_status, dual_status), is_maximize));
  }
}

TEST_P(ParametrizeOverMinMaxTest,
       ValidateBoundStatusConsistencyDualUnboundedBoundCheck) {
  const bool is_maximize = GetParam();

  const double primal_bound = 10.0;
  const double dual_bound = is_maximize ? -kInf : +kInf;
  EXPECT_THAT(
      ValidateBoundStatusConsistency(MakeBound(primal_bound, dual_bound),
                                     MakeStatus(FEASIBILITY_STATUS_INFEASIBLE,
                                                FEASIBILITY_STATUS_FEASIBLE),
                                     is_maximize),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("expected primal bound = dual bound"),
                HasSubstr(
                    absl::StrCat("for unbounded dual bound = ", dual_bound)),
                HasSubstr(absl::StrCat("found primal bound = ", primal_bound)),
                HasSubstr(absl::StrCat("is_maximize = ",
                                       (is_maximize ? "true" : "false"))))));
}

////////////////////////////////////////////////////////////////////////////////
// ValidateObjectiveBounds
////////////////////////////////////////////////////////////////////////////////

TEST(ValidateObjectiveBounds, Ok) {
  ObjectiveBoundsProto bounds_finite;
  bounds_finite.set_primal_bound(10.0);
  bounds_finite.set_dual_bound(20.0);
  EXPECT_OK(ValidateObjectiveBounds(bounds_finite));

  ObjectiveBoundsProto bounds_infinite;
  bounds_infinite.set_primal_bound(kInf);
  bounds_infinite.set_dual_bound(-kInf);
  EXPECT_OK(ValidateObjectiveBounds(bounds_infinite));
}

TEST(ValidateObjectiveBounds, NotOk) {
  ObjectiveBoundsProto bounds_primal;
  bounds_primal.set_primal_bound(std::nan(""));
  bounds_primal.set_dual_bound(20.0);
  EXPECT_THAT(ValidateObjectiveBounds(bounds_primal),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid NaN value")));

  ObjectiveBoundsProto bounds_dual;
  bounds_dual.set_primal_bound(10.0);
  bounds_dual.set_dual_bound(std::nan(""));
  EXPECT_THAT(ValidateObjectiveBounds(bounds_dual),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid NaN value")));
}

////////////////////////////////////////////////////////////////////////////////
// CheckFinitePrimalBound
////////////////////////////////////////////////////////////////////////////////

TEST(CheckFinitePrimalBound, Ok) {
  ObjectiveBoundsProto bounds;
  bounds.set_primal_bound(10.0);
  EXPECT_OK(CheckFinitePrimalBound(bounds));
}

TEST(CheckFinitePrimalBound, PlusInfinity) {
  ObjectiveBoundsProto bounds;
  bounds.set_primal_bound(kInf);
  EXPECT_THAT(CheckFinitePrimalBound(bounds),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected finite primal bound")));
}

TEST(CheckFinitePrimalBound, MinusInfinity) {
  ObjectiveBoundsProto bounds;
  bounds.set_primal_bound(-kInf);
  EXPECT_THAT(CheckFinitePrimalBound(bounds),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected finite primal bound")));
}

}  // namespace
}  // namespace operations_research::math_opt
