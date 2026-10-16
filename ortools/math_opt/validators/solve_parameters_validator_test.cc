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

#include "ortools/math_opt/validators/solve_parameters_validator.h"

#include <limits>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/parameters.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::status::StatusIs;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(ValidateSolverParametersTest, EmptyOk) {
  EXPECT_OK(ValidateSolveParameters(SolveParametersProto{}));
}

TEST(ValidateSolverParametersTest, ValidTimeLimit) {
  SolveParametersProto parameters;
  // The nanos filed must contain a value that is less than a second.
  parameters.mutable_time_limit()->set_seconds(10);
  EXPECT_OK(ValidateSolveParameters(parameters));
}

TEST(ValidateSolverParametersTest, InvalidTimeLimit) {
  SolveParametersProto parameters;
  // The nanos filed must contain a value that is less than a second.
  parameters.mutable_time_limit()->set_nanos(1000000000);
  EXPECT_THAT(
      ValidateSolveParameters(parameters),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("time_limit")));
}

TEST(ValidateSolverParametersTest, NegativeTimeLimit) {
  SolveParametersProto parameters;
  parameters.mutable_time_limit()->set_seconds(-1.0);
  EXPECT_THAT(
      ValidateSolveParameters(parameters),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("time_limit")));
}

TEST(ValidateSolverParametersTest, ValidNodeLimit) {
  SolveParametersProto parameters;
  parameters.set_node_limit(2);
  EXPECT_OK(ValidateSolveParameters(parameters));
  parameters.set_node_limit(0);
  EXPECT_OK(ValidateSolveParameters(parameters));
}

TEST(ValidateSolverParametersTest, InvalidNodeLimit) {
  SolveParametersProto parameters;
  parameters.set_node_limit(-2);
  EXPECT_THAT(
      ValidateSolveParameters(parameters),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("node_limit")));
}

TEST(ValidateSolverParametersTest, ValidThreads) {
  SolveParametersProto parameters;
  parameters.set_threads(1);
  EXPECT_OK(ValidateSolveParameters(parameters));
}

TEST(ValidateSolverParametersTest, InvalidThreads) {
  SolveParametersProto parameters;
  parameters.set_threads(0);
  EXPECT_THAT(
      ValidateSolveParameters(parameters),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("threads")));

  parameters.set_threads(-1);
  EXPECT_THAT(
      ValidateSolveParameters(parameters),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("threads")));
}

TEST(ValidateSolverParametersTest, ValidGaps) {
  SolveParametersProto parameters;
  parameters.set_relative_gap_tolerance(1.0);
  parameters.set_absolute_gap_tolerance(1.0);
  EXPECT_OK(ValidateSolveParameters(parameters));
}

TEST(ValidateSolverParametersTest, InvalidRelativeGap) {
  SolveParametersProto parameters;
  parameters.set_relative_gap_tolerance(-1.0);
  EXPECT_THAT(ValidateSolveParameters(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("relative_gap_tolerance")));
}

TEST(ValidateSolverParametersTest, InvalidAbsoluteGap) {
  SolveParametersProto parameters;
  parameters.set_absolute_gap_tolerance(-1.0);
  EXPECT_THAT(ValidateSolveParameters(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("absolute_gap_tolerance")));
}

TEST(ValidateSolverParametersTest, ValidSolutionLimit) {
  SolveParametersProto parameters;
  parameters.set_solution_limit(3);
  EXPECT_OK(ValidateSolveParameters(parameters));
}

TEST(ValidateSolverParametersTest, InvalidSolutionLimit) {
  SolveParametersProto parameters;
  parameters.set_solution_limit(0);
  EXPECT_THAT(ValidateSolveParameters(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("solution_limit")));
}

TEST(ValidateSolverParametersTest, ValidCutoffLimit) {
  SolveParametersProto parameters;
  parameters.set_cutoff_limit(4.0);
  EXPECT_OK(ValidateSolveParameters(parameters));
}

TEST(ValidateSolverParametersTest, InvalidCutoffLimitNaN) {
  for (const double bad_cutoff : {kNaN, kInf, -kInf}) {
    SolveParametersProto parameters;
    parameters.set_cutoff_limit(bad_cutoff);
    EXPECT_THAT(
        ValidateSolveParameters(parameters),
        StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("cutoff_limit")))
        << "bad cutoff: " << bad_cutoff;
  }
}

TEST(ValidateSolverParametersTest, ValidObjectiveLimit) {
  SolveParametersProto parameters;
  parameters.set_objective_limit(4.0);
  EXPECT_OK(ValidateSolveParameters(parameters));
}

TEST(ValidateSolverParametersTest, InvalidObjectiveLimit) {
  SolveParametersProto parameters;
  parameters.set_objective_limit(kNaN);
  EXPECT_THAT(ValidateSolveParameters(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("objective_limit")));
}

TEST(ValidateSolverParametersTest, ValidBestBoundLimit) {
  SolveParametersProto parameters;
  parameters.set_best_bound_limit(4.0);
  EXPECT_OK(ValidateSolveParameters(parameters));
}

TEST(ValidateSolverParametersTest, InvalidBestBoundLimit) {
  SolveParametersProto parameters;
  parameters.set_best_bound_limit(kNaN);
  EXPECT_THAT(ValidateSolveParameters(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("best_bound_limit")));
}

TEST(ValidateSolverParametersTest, ValidLpAlgorithm) {
  SolveParametersProto parameters;
  for (int lp_algorithm = LPAlgorithmProto_MIN;
       lp_algorithm < LPAlgorithmProto_MAX; ++lp_algorithm) {
    if (!LPAlgorithmProto_IsValid(lp_algorithm)) {
      continue;
    }
    parameters.set_lp_algorithm(static_cast<LPAlgorithmProto>(lp_algorithm));
    EXPECT_OK(ValidateSolveParameters(parameters))
        << "lp_algorithm: " << LPAlgorithmProto_Name(lp_algorithm);
  }
}

TEST(ValidateSolverParametersTest, InvalidLpAlgorithm) {
  SolveParametersProto parameters;
  parameters.set_lp_algorithm(LPAlgorithmProto(-1));
  EXPECT_THAT(ValidateSolveParameters(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("lp_algorithm"), HasSubstr("-1"))));
}

std::vector<EmphasisProto> ValidEmphasisValues() {
  std::vector<EmphasisProto> ret;
  for (int value = EmphasisProto_MIN; value < EmphasisProto_MAX; ++value) {
    if (EmphasisProto_IsValid(value)) {
      ret.push_back(static_cast<EmphasisProto>(value));
    }
  }
  return ret;
}

TEST(ValidateSolverParametersTest, ValidPresolve) {
  SolveParametersProto parameters;
  for (const EmphasisProto presolve : ValidEmphasisValues()) {
    parameters.set_presolve(presolve);
    EXPECT_OK(ValidateSolveParameters(parameters))
        << "presolve: " << EmphasisProto_Name(presolve);
  }
}

TEST(ValidateSolverParametersTest, InvalidPresolve) {
  SolveParametersProto parameters;
  parameters.set_presolve(EmphasisProto(-1));
  EXPECT_THAT(ValidateSolveParameters(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("presolve"), HasSubstr("-1"))));
}

TEST(ValidateSolverParametersTest, ValidCuts) {
  SolveParametersProto parameters;
  for (const EmphasisProto cuts : ValidEmphasisValues()) {
    parameters.set_cuts(cuts);
    EXPECT_OK(ValidateSolveParameters(parameters))
        << "cuts: " << EmphasisProto_Name(cuts);
  }
}

TEST(ValidateSolverParametersTest, InvalidCuts) {
  SolveParametersProto parameters;
  parameters.set_cuts(EmphasisProto(-1));
  EXPECT_THAT(ValidateSolveParameters(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cuts"), HasSubstr("-1"))));
}

TEST(ValidateSolverParametersTest, ValidHeuristics) {
  SolveParametersProto parameters;
  for (const EmphasisProto heuristics : ValidEmphasisValues()) {
    parameters.set_heuristics(heuristics);
    EXPECT_OK(ValidateSolveParameters(parameters))
        << "heuristics: " << EmphasisProto_Name(heuristics);
  }
}

TEST(ValidateSolverParametersTest, InvalidHeuristics) {
  SolveParametersProto parameters;
  parameters.set_heuristics(EmphasisProto(-1));
  EXPECT_THAT(ValidateSolveParameters(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("heuristics"), HasSubstr("-1"))));
}

TEST(ValidateSolverParametersTest, ValidScaling) {
  SolveParametersProto parameters;
  for (const EmphasisProto scaling : ValidEmphasisValues()) {
    parameters.set_scaling(scaling);
    EXPECT_OK(ValidateSolveParameters(parameters))
        << "scaling: " << EmphasisProto_Name(scaling);
  }
}

TEST(ValidateSolverParametersTest, InvalidScaling) {
  SolveParametersProto parameters;
  parameters.set_scaling(EmphasisProto(-1));
  EXPECT_THAT(ValidateSolveParameters(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("scaling"), HasSubstr("-1"))));
}

TEST(ValidateSolverParametersTest, ValidSolutionPoolSize) {
  SolveParametersProto parameters;
  parameters.set_solution_pool_size(3);
  EXPECT_OK(ValidateSolveParameters(parameters));
}

TEST(ValidateSolverParametersTest, InvalidSolutionPoolSize) {
  SolveParametersProto parameters;
  parameters.set_solution_pool_size(-3);
  EXPECT_THAT(ValidateSolveParameters(parameters),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("solution_pool_size")));
}

}  // namespace
}  // namespace operations_research::math_opt
