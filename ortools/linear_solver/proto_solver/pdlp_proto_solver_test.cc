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

#include "ortools/linear_solver/proto_solver/pdlp_proto_solver.h"

#include <atomic>
#include <string>

#include "absl/strings/substitute.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/pdlp/solve_log.pb.h"

namespace operations_research {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTextOrDie;
using pdlp::SolveLog;
using ::testing::ElementsAre;

// The param true means presolve is on, false means presolve is off.
class SolveMpModelProtoMaybePresolveTest : public testing::TestWithParam<bool> {
};

INSTANTIATE_TEST_SUITE_P(
    SolveMpModelProtoMaybePresolve, SolveMpModelProtoMaybePresolveTest,
    testing::Bool(),
    [](const testing::TestParamInfo<
        SolveMpModelProtoMaybePresolveTest::ParamType>& info) {
      return info.param ? "PresolveOn" : "PresolveOff";
    });

TEST(SolveProtoTest, SolvesEasyMinimizationLp) {
  // min -2y
  // s.t. x + y <= 1
  //      x, y >= 0
  const auto request = ParseTextOrDie<MPModelRequest>(R"pb(
    model {
      variable { lower_bound: 0 }
      variable { lower_bound: 0 objective_coefficient: -2.0 }
      constraint {
        var_index: 0
        var_index: 1
        coefficient: 1.0
        coefficient: 1.0
        lower_bound: -inf
        upper_bound: 1.0
      }
    }
  )pb");
  MPSolutionResponse response = PdlpSolveProto(request);
  SolveLog solve_log;
  ASSERT_TRUE(solve_log.ParseFromString(response.solver_specific_info()));
  EXPECT_EQ(response.status(), MPSOLVER_OPTIMAL);
  EXPECT_DOUBLE_EQ(response.objective_value(), -2.0);
  EXPECT_THAT(response.variable_value(), ElementsAre(0.0, 1.0));
  // As of 2022-02-22, the signs of these duals are consistent with Glop.
  EXPECT_THAT(response.dual_value(), ElementsAre(-2.0));
  EXPECT_THAT(response.reduced_cost(), ElementsAre(2.0, 0.0));

  EXPECT_TRUE(solve_log.has_solution_stats());
  EXPECT_GT(solve_log.iteration_count(), 0);
}

TEST(SolveProtoTest, RespectsInterruptAtStart) {
  // min -2y
  // s.t. x + y <= 1
  //      x, y >= 0
  const auto request = ParseTextOrDie<MPModelRequest>(R"pb(
    model {
      variable { lower_bound: 0 }
      variable { lower_bound: 0 objective_coefficient: -2.0 }
      constraint {
        var_index: 0
        var_index: 1
        coefficient: 1.0
        coefficient: 1.0
        lower_bound: -inf
        upper_bound: 1.0
      }
    }
  )pb");
  std::atomic<bool> interrupt_solve;
  interrupt_solve = true;
  MPSolutionResponse response = PdlpSolveProto(
      request, /*relax_integer_variables=*/false, &interrupt_solve);
  EXPECT_EQ(response.status(), MPSOLVER_NOT_SOLVED);
}

TEST_P(SolveMpModelProtoMaybePresolveTest, SolvesEasyMaximizationLp) {
  // max 2y
  // s.t. x + y <= 1
  //      x, y >= 0
  auto request = ParseTextOrDie<MPModelRequest>(R"pb(
    model {
      variable { lower_bound: 0 }
      variable { lower_bound: 0 objective_coefficient: 2.0 }
      constraint {
        var_index: 0
        var_index: 1
        coefficient: 1.0
        coefficient: 1.0
        lower_bound: -inf
        upper_bound: 1.0
      }
      maximize: true
    }
  )pb");
  const bool presolve_on = GetParam();
  request.set_solver_specific_parameters(
      absl::Substitute(R"pb(
                         presolve_options { use_glop: $0 }
                       )pb",
                       presolve_on));

  const MPSolutionResponse response = PdlpSolveProto(request);
  SolveLog solve_log;
  ASSERT_TRUE(solve_log.ParseFromString(response.solver_specific_info()));

  EXPECT_EQ(response.status(), MPSOLVER_OPTIMAL);
  EXPECT_DOUBLE_EQ(response.objective_value(), 2.0);
  EXPECT_THAT(response.variable_value(), ElementsAre(0.0, 1.0));
  // As of 2022-02-22, the signs of these duals are consistent with Glop.
  EXPECT_THAT(response.dual_value(), ElementsAre(2.0));
  EXPECT_THAT(response.reduced_cost(), ElementsAre(-2.0, 0.0));

  EXPECT_TRUE(solve_log.has_solution_stats());
  if (presolve_on) {
    EXPECT_EQ(solve_log.iteration_count(), 0);
  } else {
    EXPECT_GT(solve_log.iteration_count(), 0);
  }
}

TEST(SolveProtoTest, RelaxesIntegerVariables) {
  // max x
  // s.t. 0 <= x <= 1, x integer
  const auto request = ParseTextOrDie<MPModelRequest>(R"pb(
    model {
      variable {
        lower_bound: 0
        upper_bound: 1
        is_integer: true
        objective_coefficient: 1
      }
      maximize: true
    }
  )pb");
  const MPSolutionResponse response =
      PdlpSolveProto(request, /*relax_integer_variables=*/true);
  EXPECT_EQ(response.status(), MPSOLVER_OPTIMAL);
  EXPECT_DOUBLE_EQ(response.objective_value(), 1.0);
  EXPECT_THAT(response.variable_value(), ElementsAre(1.0));
  EXPECT_THAT(response.reduced_cost(), ElementsAre(1.0));
}

TEST(SolveProtoTest, ErrorsOnIntegerVariables) {
  // max x
  // s.t. 0 <= x <= 1, x integer
  const auto request = ParseTextOrDie<MPModelRequest>(R"pb(
    model {
      variable {
        lower_bound: 0
        upper_bound: 1
        is_integer: true
        objective_coefficient: 1
      }
      maximize: true
    }
  )pb");

  const MPSolutionResponse response =
      PdlpSolveProto(request, /*relax_integer_variables=*/false);
  EXPECT_EQ(response.status(), MPSOLVER_MODEL_INVALID);
}

// This test solves an LP that's designed to terminate with
// POINT_TYPE_ITERATE_DIFFERENCE. This yields a solution without any
// convergence information. SolveMpModelProto() should return a proto without
// objective_value set but otherwise function normally.
TEST(SolveProtoTest, SolvesWithDifferenceOfIterates) {
  // This test uses the LP min x + y s.t. 2*y >= 0, y >= 5. This is primal
  // unbounded and dual infeasible. The solver will start at x=0, y=5 and the
  // current iterate won't be a dual infeasibility certificate until x < -5,
  // which takes several iterations due to the 2*y >= 0 constraint limiting
  // the step size. Difference of iterates detects dual infeasibility
  // immediately.
  auto request = ParseTextOrDie<MPModelRequest>(R"pb(
    model {
      variable { objective_coefficient: 1 }
      variable { lower_bound: 5 objective_coefficient: 1 }
      constraint { var_index: 1 coefficient: 2.0 lower_bound: 0.0 }
    }
  )pb");
  request.set_solver_specific_parameters(R"pb(
    linesearch_rule: CONSTANT_STEP_SIZE_RULE
    major_iteration_frequency: 1
    l_inf_ruiz_iterations: 0
    l2_norm_rescaling: false
  )pb");
  const MPSolutionResponse response = PdlpSolveProto(request);
  SolveLog solve_log;
  ASSERT_TRUE(solve_log.ParseFromString(response.solver_specific_info()));

  EXPECT_EQ(response.status(), MPSOLVER_NOT_SOLVED);
  EXPECT_THAT(response.variable_value(), ElementsAre(-0.4, 0.0));
  EXPECT_THAT(response.dual_value(), ElementsAre(0.0));
  EXPECT_THAT(response.reduced_cost(), ElementsAre(1.0, 1.0));
  EXPECT_FALSE(response.has_objective_value());
  EXPECT_TRUE(solve_log.has_solution_stats());
  EXPECT_EQ(solve_log.iteration_count(), 1);
}

}  // namespace
}  // namespace operations_research
