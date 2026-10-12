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

#include "ortools/linear_solver/proto_solver/glop_proto_solver.h"

#include <atomic>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTextOrDie;
using ::testing::ElementsAre;

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
  MPSolutionResponse response = GlopSolveProto(request);
  EXPECT_EQ(response.status(), MPSOLVER_OPTIMAL);
  EXPECT_DOUBLE_EQ(response.objective_value(), -2.0);
  EXPECT_THAT(response.variable_value(), ElementsAre(0.0, 1.0));
  // As of 2022-02-22, the signs of these duals are consistent with Glop.
  EXPECT_THAT(response.dual_value(), ElementsAre(-2.0));
  EXPECT_THAT(response.reduced_cost(), ElementsAre(2.0, 0.0));
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
  MPSolutionResponse response = GlopSolveProto(request, &interrupt_solve);
  EXPECT_EQ(response.status(), MPSOLVER_CANCELLED_BY_USER);
}

TEST(SolveProtoTest, SolvesEasyMaximizationLp) {
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
  MPSolutionResponse response = GlopSolveProto(request);
  EXPECT_EQ(response.status(), MPSOLVER_OPTIMAL);
  EXPECT_DOUBLE_EQ(response.objective_value(), 2.0);
  EXPECT_THAT(response.variable_value(), ElementsAre(0.0, 1.0));
  // As of 2022-02-22, the signs of these duals are consistent with Glop.
  EXPECT_THAT(response.dual_value(), ElementsAre(2.0));
  EXPECT_THAT(response.reduced_cost(), ElementsAre(-2.0, 0.0));
}

TEST(SolveProtoTest, RejectIndicatorConstraint) {
  // max 2y
  // s.t. x + y <= 1
  //      x, y >= 0
  auto request = ParseTextOrDie<MPModelRequest>(R"pb(
    model {
      variable { lower_bound: 0 }
      variable { lower_bound: 0 objective_coefficient: 2.0 }
      variable { lower_bound: 0 upper_bound: 1 is_integer: true }
      general_constraint {
        indicator_constraint {
          var_index: 2
          var_value: 0
          constraint {
            var_index: 0
            var_index: 1
            coefficient: 1.0
            coefficient: 1.0
            lower_bound: -inf
            upper_bound: 1.0
          }
        }
      }
      maximize: true
    }
  )pb");
  MPSolutionResponse response = GlopSolveProto(request);
  EXPECT_EQ(response.status(), MPSOLVER_MODEL_INVALID);
}

}  // namespace
}  // namespace operations_research
