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

#include "ortools/util/mp_model_solution_checker.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/util/logging.h"

namespace operations_research {

namespace {

using ::google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

TEST(IsSolutionFeasibleAndGetTightObjectiveBoundsTest, EmptyModel) {
  const MPModelProto model;
  std::vector<double> solution;
  EXPECT_THAT(SolutionIsFeasible(model, solution, {}), IsOkAndHolds(true));
  EXPECT_THAT(GetTightObjectiveBounds(model, solution),
              IsOkAndHolds(Pair(0.0, 0.0)));
}

MPModelProto TestModel() {
  // Minimize 2x + 3y + b + 7
  // s.t.
  //  x in [0, 1]
  //  y in [0, 2]
  //  b in {0, 1}
  // 0.5 <= x + 1.5y <= 1.5
  // b = 1 => x <= 0.5
  return ParseTextProtoOrDie(R"pb(
    variable {
      name: "x"
      lower_bound: 0.0
      upper_bound: 1.0
      objective_coefficient: 2.0
    }
    variable {
      name: "y"
      lower_bound: 0.0
      upper_bound: 2.0
      objective_coefficient: 3.0
    }
    variable {
      name: "b"
      lower_bound: 0.0
      upper_bound: 1.0
      is_integer: true
      objective_coefficient: 1.0
    }
    constraint {
      name: "c1"
      var_index: [ 0, 1 ]
      coefficient: [ 1.0, 1.5 ]
      lower_bound: 0.5
      upper_bound: 1.5
    }
    general_constraint {
      indicator_constraint {
        var_index: 2
        var_value: 1
        constraint { name: "c2" var_index: 0 coefficient: 1.0 upper_bound: 0.5 }
      }
    }
    objective_offset: 7.0
  )pb");
}

struct IsSolutionFeasibleTestCase {
  std::string name;
  std::vector<double> sol;
  bool feas;
  std::pair<double, double> obj_bnds;
};

class IsSolutionFeasibleTest
    : public ::testing::TestWithParam<IsSolutionFeasibleTestCase> {};

TEST_P(IsSolutionFeasibleTest, FeasibleSolutionWithIndicatorConstraint) {
  const auto& test_case = GetParam();
  SolverLogger logger;
  logger.EnableLogging(true);
  int num_logs = 0;
  logger.AddInfoLoggingCallback([&](const std::string&) { ++num_logs; });
  EXPECT_THAT(
      SolutionIsFeasible(TestModel(), test_case.sol, {.logger = &logger}),
      IsOkAndHolds(test_case.feas));
  if (test_case.feas) {
    EXPECT_EQ(num_logs, 0);
  } else {
    // Exactly 1 log and 1 summary line.
    EXPECT_EQ(num_logs, 2);
  }
  EXPECT_THAT(
      GetTightObjectiveBounds(TestModel(), test_case.sol),
      IsOkAndHolds(Pair(test_case.obj_bnds.first, test_case.obj_bnds.second)));
}

// var: x in [0, 1], y in [0, 2], b in {0, 1}
// obj: 2x + 3y + b + 7
// c1: 0.5 <= x + 1.5y <= 1.5
// c2: b = 1 => x <= 0.5
INSTANTIATE_TEST_SUITE_P(
    IsSolutionFeasibleTests, IsSolutionFeasibleTest,
    testing::ValuesIn<IsSolutionFeasibleTestCase>(
        {{"feasible_solution", {0.5, 0.0, 1.0}, true, {9, 9}},
         {"x_violated_down", {-0.25, 1.0, 0.0}, false, {9.5, 9.5}},
         {"x_violated_up", {1.25, 0.0, 0.0}, false, {9.5, 9.5}},
         {"b_non_integer", {0.5, 0.0, 0.5}, false, {8.5, 8.5}},
         {"c1_violated_up", {0.0, 2.0, 0.0}, false, {13.0, 13.0}},
         {"c1_violated_down", {0.0, 0.0, 0.0}, false, {7.0, 7.0}},
         {"c2_violated", {1.0, 0.0, 1.0}, false, {10.0, 10.0}},
         {"c2_deactivated", {1.0, 0.0, 0.0}, true, {9.0, 9.0}},
         {"inexact_objective",
          {std::nextafter(0.5, 1.0), 0.0, 0.0},
          true,
          {8, std::nextafter(8.0, 9.0)}}}),
    [](const ::testing::TestParamInfo<IsSolutionFeasibleTest::ParamType>&
           info) { return info.param.name; });

TEST(IsSolutionFeasibleTest, ConstraintTolerance) {
  const MPModelProto model = ParseTextProtoOrDie(R"pb(
    variable { name: "x" }
    variable { name: "y" }
    variable { name: "z" }
    constraint {
      name: "c1"
      var_index: [ 0, 1, 2 ]
      coefficient: [ 1, 0.5, 1 ]
      upper_bound: 0.0
    }
  )pb");
  // Usual sum would make the solution feasible.
  const std::vector<double> solution = {1e100, std::nextafter(0.0, 1.0),
                                        -1e100};
  EXPECT_THAT(
      SolutionIsFeasible(model, solution, {.constraint_tolerance = 0.0}),
      IsOkAndHolds(false));
  EXPECT_THAT(
      SolutionIsFeasible(model, solution,
                         {.constraint_tolerance = std::nextafter(0.0, 1.0)}),
      IsOkAndHolds(true));
}

TEST(IsSolutionFeasibleTest, NegativeConstraintTolerance) {
  const MPModelProto model;
  EXPECT_THAT(
      SolutionIsFeasible(model, {}, {.constraint_tolerance = -0.1}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("negative")));
}

TEST(IsSolutionFeasibleTest, NegativeVariableTolerance) {
  const MPModelProto model;
  EXPECT_THAT(
      SolutionIsFeasible(model, {}, {.variable_tolerance = -0.1}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("negative")));
}

TEST(IsSolutionFeasibleTest, NegativeIntegerTolerance) {
  const MPModelProto model;
  EXPECT_THAT(
      SolutionIsFeasible(model, {}, {.integer_tolerance = -0.1}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("negative")));
}

TEST(IsSolutionFeasibleTest, WrongSolutionSize) {
  EXPECT_THAT(SolutionIsFeasible(TestModel(), {1.0, 2.0}, {}),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("size")));
  EXPECT_THAT(GetTightObjectiveBounds(TestModel(), {1.0, 2.0}),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("size")));
}

TEST(IsSolutionFeasibleTest, WrongVarIndex) {
  const MPModelProto model = ParseTextProtoOrDie(R"pb(
    constraint { var_index: 123 coefficient: 1.0 lower_bound: 0.0 }
  )pb");
  EXPECT_THAT(
      SolutionIsFeasible(model, {}, {}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("index 123")));
}

TEST(IsSolutionFeasibleTest, NonIndicatorGeneralConstraint) {
  const MPModelProto model = ParseTextProtoOrDie(R"pb(
    general_constraint { abs_constraint {} }
  )pb");
  EXPECT_THAT(SolutionIsFeasible(model, {}, {}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("not an indicator constraint")));
}

}  // namespace

}  // namespace operations_research
