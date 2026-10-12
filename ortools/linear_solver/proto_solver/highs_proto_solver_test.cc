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

#include "ortools/linear_solver/proto_solver/highs_proto_solver.h"

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/base/path.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/mps_reader.h"

namespace operations_research {
namespace {

constexpr double kHighsTolerance = 1e-5;

using ::google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using ::testing::AllOf;
using ::testing::EqualsProto;
using ::testing::HasSubstr;
using ::testing::proto::Approximately;
using ::testing::proto::Partially;

// A wrapper around the proto solve to clear solve info that contains
// non-deterministic information like the duration of the solve.
MPSolutionResponse HighsSolveProtoAndClearSolveInfo(
    const MPModelRequest& request) {
  MPSolutionResponse response = HighsSolveProto(request);
  response.clear_solve_info();
  return response;
}

TEST(HighsProtoSolverTest, NoModelYieldsOptimal) {
  const auto response =
      HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(""));
  EXPECT_THAT(response, Partially(EqualsProto("status: MPSOLVER_OPTIMAL")));
  EXPECT_THAT(response.status_str(), HasSubstr("without model"));
}

TEST(HighsProtoSolverTest, EmptyModelsAreOptimal) {
  EXPECT_THAT(HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie("model {}")),
              Partially(EqualsProto(R"pb(
                status: MPSOLVER_OPTIMAL
                objective_value: 0
                best_objective_bound: 0
              )pb")));
}

TEST(HighsProtoSolverTest, CatchesInvalidModels) {
  const auto response =
      HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(R"pb(
        model {
          variable { lower_bound: 0 }
          constraint { var_index: 1 coefficient: 1 upper_bound: 0 }
        }
        enable_internal_solver_output: true
      )pb"));
  EXPECT_THAT(response,
              Partially(EqualsProto("status: MPSOLVER_MODEL_INVALID")));
  EXPECT_THAT(response.status_str(), HasSubstr("var_index"));
}

TEST(HighsProtoSolverTest, Unbounded) {
  EXPECT_THAT(HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(R"pb(
                model { variable { objective_coefficient: 1 } }
              )pb")),
              Partially(EqualsProto(R"pb(
                status: MPSOLVER_UNBOUNDED
              )pb")));
}

TEST(HighsProtoSolverTest, UnboundedMaximize) {
  EXPECT_THAT(HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(R"pb(
                model {
                  maximize: true
                  variable { objective_coefficient: 1 }
                }
              )pb")),
              Partially(EqualsProto(R"pb(
                status: MPSOLVER_UNBOUNDED
              )pb")));
}

TEST(HighsProtoSolverTest, UnboundedMip) {
  EXPECT_THAT(HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(R"pb(
                model { variable { objective_coefficient: 1 is_integer: true } }
              )pb")),
              Partially(EqualsProto(R"pb(
                status: MPSOLVER_UNBOUNDED
              )pb")));
}

TEST(HighsProtoSolverTest, UnboundedMaximizeMip) {
  EXPECT_THAT(HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(R"pb(
                model {
                  maximize: true
                  variable { objective_coefficient: 1 is_integer: true }
                }
              )pb")),
              Partially(EqualsProto(R"pb(
                status: MPSOLVER_UNBOUNDED
              )pb")));
}

TEST(HighsProtoSolverTest, CatchesTriviallyInfeasibleModels) {
  const auto response =
      HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(R"pb(
        model { variable { lower_bound: 0 upper_bound: -1 } }
      )pb"));
  EXPECT_THAT(response, Partially(EqualsProto("status: MPSOLVER_INFEASIBLE")));
  EXPECT_THAT(response.status_str(),
              AllOf(HasSubstr("Infeasible"), HasSubstr("variable #0")));
}

TEST(HighsProtoSolverTest, UsesObjectiveOffset) {
  EXPECT_THAT(HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(R"pb(
                model {
                  variable { lower_bound: 1 objective_coefficient: 2 }
                  objective_offset: 40
                }
              )pb")),
              EqualsProto(R"pb(
                status: MPSOLVER_OPTIMAL
                objective_value: 42
                best_objective_bound: 42
                variable_value: 1
              )pb"));
}

TEST(HighsProtoSolverTest, UsesObjectiveOffsetOnEmptyModel) {
  EXPECT_THAT(HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(R"pb(
                model { objective_offset: 317 }
              )pb")),
              Partially(EqualsProto(R"pb(
                status: MPSOLVER_OPTIMAL
                objective_value: 317
                best_objective_bound: 317
              )pb")));
}

// This specific model caused test failures on client code because HIGHS doesn't
// automatically round integer variables' optimal values.
TEST(HighsProtoSolverTest, ReturnsIntegerValues) {
  EXPECT_THAT(HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(R"pb(
                model {
                  maximize: false
                  variable {
                    lower_bound: 0.0
                    objective_coefficient: 67.0
                    is_integer: true
                  }
                  variable {
                    lower_bound: 0.0
                    objective_coefficient: 50.0
                    is_integer: true
                  }
                  variable {
                    lower_bound: 0.0
                    objective_coefficient: 134.0
                    is_integer: true
                  }
                  variable {
                    lower_bound: 0.0
                    objective_coefficient: 100.0
                    is_integer: true
                  }
                  constraint {
                    lower_bound: 18.0
                    upper_bound: 18.0
                    var_index: 0
                    var_index: 2
                    coefficient: 1.0
                    coefficient: 1.0
                  }
                  constraint {
                    lower_bound: 30.0
                    upper_bound: 30.0
                    var_index: 1
                    var_index: 3
                    coefficient: 1.0
                    coefficient: 1.0
                  }
                  constraint {
                    upper_bound: 1420.0
                    var_index: 0
                    var_index: 1
                    coefficient: 67.0
                    coefficient: 50.0
                  }
                  constraint {
                    upper_bound: 1420.0
                    var_index: 2
                    var_index: 3
                    coefficient: 67.0
                    coefficient: 50.0
                  }
                }
                solver_type: HIGHS_MIXED_INTEGER_PROGRAMMING
              )pb")),
              EqualsProto(R"pb(
                status: MPSOLVER_OPTIMAL
                objective_value: 3992.0
                best_objective_bound: 3992.0
                variable_value: 10
                variable_value: 15
                variable_value: 8
                variable_value: 15
              )pb"));
}

// This is a regression test for b/156228836. Before cr/328940768, reported
// optimal objective was 1e11 instead of 400 000.
//
// Model:
//   15 boolean variables: b_0, ..., b_14
//   15 non-negative real variables: x_0, ..., x_14
//   15 real variables in [0, 1]: y_0, ..., y_14
//  Constraints:
//   -1 <= x_i - b_i for all i
//   1 <= y_i + b_i for i = 0 .. 4
//   0 <= y_5 + b_5
//   b_0 + b_2 + ... + b_14 <= 1
//  Objective:
//   minimize   3e-5 (b_0 + ... + b_3) + 2e-5 (b_4 + ... + b_13) + 1e-5 b_14
//            + 10 000 (x_0 + ... + x_4) + 5 000 (x_5 + ... + x_14)
//            + 100 000 (y_0 + ... + y_14)
// Solution:
//   b_4 = 1; y_0, ..., y_3 = 1; all other = 0
//   -> objective 400 000 + 2e-5.
TEST(HighsProtoSolverTest, SolveModelWithExtremeCoefficients) {
  const int nBinary = 15;
  const int nXs = 15;
  const int nYs = 15;
  MPModelRequest request;
  auto* model = request.mutable_model();
  model->set_maximize(false);
  for (int i = 0; i < nBinary; ++i) {
    auto* var = model->add_variable();
    var->set_name(absl::StrFormat("b_%d", i));
    var->set_is_integer(true);
    var->set_lower_bound(0);
    var->set_upper_bound(1);
    if (i == 14) {
      var->set_objective_coefficient(1e-5);
    } else if (3 < i) {
      var->set_objective_coefficient(2e-5);
    } else {
      var->set_objective_coefficient(3e-5);
    }
  }
  for (int i = 0; i < nXs; ++i) {
    auto* var = model->add_variable();
    var->set_name(absl::StrFormat("x_%d", i));
    var->set_lower_bound(0);
    if (i > 4) {
      var->set_objective_coefficient(5000);
    } else {
      var->set_objective_coefficient(10000);
    }
  }
  for (int i = 0; i < nYs; ++i) {
    auto* var = model->add_variable();
    var->set_name(absl::StrFormat("y_%d", i));
    var->set_lower_bound(0);
    var->set_upper_bound(1);
    var->set_objective_coefficient(100000);
  }
  for (int i = 0; i < nBinary; ++i) {
    auto* constraint = model->add_constraint();
    constraint->set_name(absl::StrFormat("c_bx_%d", i));
    constraint->set_lower_bound(-1);
    constraint->add_coefficient(1);
    constraint->add_var_index(nBinary + i);
    constraint->add_coefficient(-1);
    constraint->add_var_index(i);
  }
  for (int i = 0; i < 6; ++i) {
    auto* constraint = model->add_constraint();
    constraint->set_name(absl::StrFormat("c_by_%d", i));
    constraint->set_lower_bound((i == 5) ? 0 : 1);
    constraint->add_coefficient(1);
    constraint->add_var_index(nBinary + nXs + i);
    constraint->add_coefficient(1);
    constraint->add_var_index(i);
  }
  {
    auto* constraint = model->add_constraint();
    constraint->set_name("c_binaries");
    constraint->set_upper_bound(1);
    for (int i = 0; i < nBinary; ++i) {
      constraint->add_coefficient(1);
      constraint->add_var_index(i);
    }
  }
  request.set_solver_time_limit_seconds(0.1);
  request.set_solver_specific_parameters("mip_feasibility_tolerance=1.0E-7");
  const auto response = HighsSolveProtoAndClearSolveInfo(request);
  EXPECT_EQ(response.status(), MPSOLVER_OPTIMAL);
  EXPECT_NEAR(response.objective_value(), 400000, 1e-1);
}

TEST(HighsProtoSolverTest, FeasibilityToleranceTest) {
  EXPECT_THAT(
      HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(R"pb(
        model {
          variable {
            lower_bound: -1.7
            upper_bound: 1.7
            objective_coefficient: 31.7
            is_integer: true
          }
          constraint {
            lower_bound: -0.9995
            var_index: [ 0 ]
            coefficient: [ 1 ]
          }
        }
        solver_type: HIGHS_MIXED_INTEGER_PROGRAMMING
        solver_specific_parameters: "mip_feasibility_tolerance = 0.001"
      )pb")),
      // Without the feasibility tolerance parameter, the optimal value is zero.
      Approximately(EqualsProto(MPSolutionResponse(ParseTextProtoOrDie(R"pb(
                      status: MPSOLVER_OPTIMAL
                      variable_value: [ -1 ]
                      objective_value: -31.7
                      best_objective_bound: -31.7
                    )pb"))),
                    kHighsTolerance));
}

TEST(HighsProtoSolverTest, HighsSolveProtoWithErrorInModelForHighs) {
  EXPECT_THAT(
      HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(
          "model { variable { objective_coefficient: -1e+50 } }")),
      Partially(EqualsProto("status: MPSOLVER_NOT_SOLVED status_str: \"Error "
                            "running HiGHS run()\"")));
}

TEST(HighsProtoSolverTest, DoesNotCrashForUnboundedFeasibility) {
  // Verify that passing an unbounded feasibility problem does not cause us to
  // make HIGHS crash.
  HighsSolveProtoAndClearSolveInfo(ParseTextProtoOrDie(R"pb(
    model {
      variable {}
      variable {}
      constraint { lower_bound: 2 upper_bound: 2 var_index: 0 coefficient: 1 }
      constraint { lower_bound: 0 upper_bound: inf var_index: 1 coefficient: 2 }
      constraint { lower_bound: 0 upper_bound: inf var_index: 1 coefficient: 1 }
    }
    solver_type: HIGHS_MIXED_INTEGER_PROGRAMMING
  )pb"));
}

}  // namespace
}  // namespace operations_research
