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

// A minimal set of tests to cover the lines of mp_model_builder.h/cc. A more
// comprehensive set of tests that check the entire behavior of using gSCIP to
// solve from proto are given in
// ortools/linear_solver/scip_proto_solver_test.cc.
//
// Note that we do not test any codepaths on:
//   (a) Invalid proto input, these are covered by scip_proto_solver_test.
//   (b) Status errors. These errors are being propagated from gscip and the
//       the responsibility of the gscip unit tests.

#include "ortools/math_opt/solvers/gscip/gscip_from_mp_model_proto.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
#include "ortools/math_opt/solvers/gscip/gscip.pb.h"
#include "ortools/math_opt/solvers/gscip/gscip_testing.h"

namespace operations_research {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(GScipFromMPModelProtoTest, EmptyModel) {
  const MPModelProto model;
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  EXPECT_THAT(gscip_and_vars.variables, IsEmpty());
  EXPECT_THAT(gscip_and_vars.gscip->variables(), IsEmpty());
  EXPECT_THAT(gscip_and_vars.gscip->constraints(), IsEmpty());
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  AssertOptimalWithBestSolution(result, 0.0, {});
}

TEST(GScipFromMPModelProtoTest, OffsetOnly) {
  const MPModelProto model = ParseTestProto(R"pb(
    objective_offset: 3.5
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  AssertOptimalWithBestSolution(result, 3.5, {});
}

// min 2 * x + 5 * y
// x >= 1
// y >= 4
//
// (x*, y*) = (1, 4), obj* = 22.
TEST(GScipFromMPModelProtoTest, TwoVars) {
  const MPModelProto model = ParseTestProto(R"pb(
    variable { lower_bound: 1 objective_coefficient: 2 }
    variable { lower_bound: 4 objective_coefficient: 5 }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_EQ(gscip_and_vars.variables.size(), 2);
  EXPECT_THAT(gscip_and_vars.gscip->variables(),
              UnorderedElementsAre(gscip_and_vars.variables[0],
                                   gscip_and_vars.variables[1]));
  EXPECT_THAT(gscip_and_vars.gscip->constraints(), IsEmpty());
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  AssertOptimalWithBestSolution(
      result, 22.0,
      {{gscip_and_vars.variables[0], 1.0}, {gscip_and_vars.variables[1], 4.0}});
}

// max 2 * x
// x <= 2
//
// x* = 2, obj* = 4.
TEST(GScipFromMPModelProtoTest, Maximize) {
  const MPModelProto model = ParseTestProto(R"pb(
    maximize: true
    variable { upper_bound: 2 objective_coefficient: 2 }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  AssertOptimalWithBestSolution(result, 4.0,
                                {{gscip_and_vars.variables[0], 2.0}});
}

// min 2 * x + 5 * y
// x + y >= 1
// x, y >= 0
//
// (x*, y*) = (1, 0), obj* = 2.
TEST(GScipFromMPModelProtoTest, LinearConstraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    variable { lower_bound: 0 objective_coefficient: 2 }
    variable { lower_bound: 0 objective_coefficient: 5 }
    constraint {
      lower_bound: 1
      var_index: [ 0, 1 ]
      coefficient: [ 1.0, 1.0 ]
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  AssertOptimalWithBestSolution(
      result, 2.0,
      {{gscip_and_vars.variables[0], 1.0}, {gscip_and_vars.variables[1], 0.0}});
}

// max 0.1 * y
// y = abs(x)
// -4 <= x <= 1
//
// (x*, y*) = (-4, 4), obj* = 0.4.
TEST(GScipFromMPModelProtoTest, AbsConstraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    maximize: true
    variable { lower_bound: -4 upper_bound: 1 name: "x" }
    variable { objective_coefficient: 0.1 name: "y" }
    general_constraint {
      abs_constraint { var_index: 0 resultant_var_index: 1 }
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  // Note that the absolute value may create auxiliary variables.
  AssertOptimalWithPartialBestSolution(result, 0.4,
                                       {{gscip_and_vars.variables[0], -4.0},
                                        {gscip_and_vars.variables[1], 4.0}});
}

// min 2 * y
// y = max(x, 0.4)
// -2 <= x <= 1
//
// -2 <= x* <= 0.4, y* = 0.4, obj* = 0.8.
TEST(GScipFromMPModelProtoTest, MaxConstraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    variable { lower_bound: -2 upper_bound: 1 name: "x" }
    variable { objective_coefficient: 2.0 name: "y" }
    general_constraint {
      max_constraint { var_index: 0 constant: 0.4 resultant_var_index: 1 }
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  // Note that:
  //  - the max constraint may create auxiliary variables
  //  - x make take any value <= 0.4
  AssertOptimalWithPartialBestSolution(result, 0.8,
                                       {{gscip_and_vars.variables[1], 0.4}});
}

// Like the above test, but with a duplicate term in the max.
//
// min 2 * y
// y = max(x, x, 0.4)
// -2 <= x <= 1
//
// -2 <= x* <= 0.4, y* = 0.4, obj* = 0.8.
TEST(GScipFromMPModelProtoTest, MaxConstraintWithDuplicate) {
  const MPModelProto model = ParseTestProto(R"pb(
    variable { lower_bound: -2 upper_bound: 1 name: "x" }
    variable { objective_coefficient: 2.0 name: "y" }
    general_constraint {
      max_constraint {
        var_index: [ 0, 0 ]
        constant: 0.4
        resultant_var_index: 1
      }
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  // Note that:
  //  - the max constraint may create auxiliary variables
  //  - x make take any value <= 0.4
  AssertOptimalWithPartialBestSolution(result, 0.8,
                                       {{gscip_and_vars.variables[1], 0.4}});
}

// max 2 * z
// z = min(x, y)
// 2 <= x <= 4
// 3 <= y <= 5
//
// x* = 4, 4 <= y* = 5, z* = 4, obj* = 8.
TEST(GScipFromMPModelProtoTest, MinConstraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    maximize: true
    variable { lower_bound: 2 upper_bound: 4 name: "x" }
    variable { lower_bound: 3 upper_bound: 5 name: "y" }
    variable { objective_coefficient: 2.0 name: "z" }
    general_constraint {
      min_constraint {
        var_index: [ 0, 1 ]
        resultant_var_index: 2
      }
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  // Note that:
  //  - the max constraint may create auxiliary variables
  //  - y make take any value >= 4
  AssertOptimalWithPartialBestSolution(
      result, 8.0,
      {{gscip_and_vars.variables[0], 4.0}, {gscip_and_vars.variables[2], 4.0}});
}

// min x - 5z
// if z then x >= y
// x >= 0
// y >= 2
// z in {0, 1}
//
// (x*, y*, z*) = (2, 2, 1), objective value -3.0.
TEST(GScipFromMPModelProtoTest, IndicatorConstraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    variable { lower_bound: 0 name: "x" objective_coefficient: 1.0 }
    variable { lower_bound: 2 name: "y" }
    variable {
      lower_bound: 0
      upper_bound: 1
      objective_coefficient: -5.0
      is_integer: true
      name: "z"
    }
    general_constraint {
      indicator_constraint {
        var_index: 2
        var_value: 1
        constraint {
          lower_bound: 0.0
          var_index: [ 0, 1 ]
          coefficient: [ 1.0, -1.0 ]
        }
      }
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  // Note that the indicator constraint may create auxiliary variables.
  AssertOptimalWithPartialBestSolution(result, -3.0,
                                       {{gscip_and_vars.variables[0], 2.0},
                                        {gscip_and_vars.variables[1], 2.0},
                                        {gscip_and_vars.variables[2], 1.0}});
}

// min y
// y >= x^2
// x >= 3
//
// (x*, y*) = (3, 9), objective value 9.0.
TEST(GScipFromMPModelProtoTest, QuadraticConstraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    variable { lower_bound: 3 name: "x" }
    variable { name: "y" objective_coefficient: 1.0 }
    general_constraint {
      quadratic_constraint {
        lower_bound: 0.0,
        var_index: 1
        coefficient: 1.0,
        qvar1_index: 0
        qvar2_index: 0
        qcoefficient: -1.0
      }
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  AssertOptimalWithBestSolution(
      result, 9.0,
      {{gscip_and_vars.variables[0], 3.0}, {gscip_and_vars.variables[1], 9.0}});
}

// max 3.0*z - x
// z = AND(x, y)
// x, y, z in {0, 1}
//
// (x*, y*, z*) = (1, 1, 1), objective value 2.0.
TEST(GScipFromMPModelProtoTest, AndConstraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    maximize: true
    variable {
      lower_bound: 0
      upper_bound: 1
      is_integer: true
      name: "x"
      objective_coefficient: -1.0
    }
    variable { lower_bound: 0 upper_bound: 1 is_integer: true name: "y" }
    variable {
      lower_bound: 0
      upper_bound: 1
      is_integer: true
      name: "z"
      objective_coefficient: 3.0
    }
    general_constraint {
      and_constraint {
        var_index: [ 0, 1 ]
        resultant_var_index: 2
      }
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  AssertOptimalWithBestSolution(result, 2.0,
                                {{gscip_and_vars.variables[0], 1.0},
                                 {gscip_and_vars.variables[1], 1.0},
                                 {gscip_and_vars.variables[2], 1.0}});
}

// min -3.0*z + x
// z = OR(x, y)
// x, y, z in {0, 1}
//
// (x*, y*, z*) = (0, 1, 1), objective value -2.0.
TEST(GScipFromMPModelProtoTest, OrConstraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    variable {
      lower_bound: 0
      upper_bound: 1
      is_integer: true
      name: "x"
      objective_coefficient: 1.0
    }
    variable { lower_bound: 0 upper_bound: 1 is_integer: true name: "y" }
    variable {
      lower_bound: 0
      upper_bound: 1
      is_integer: true
      name: "z"
      objective_coefficient: -3.0
    }
    general_constraint {
      or_constraint {
        var_index: [ 0, 1 ]
        resultant_var_index: 2
      }
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  AssertOptimalWithBestSolution(result, -3.0,
                                {{gscip_and_vars.variables[0], 0.0},
                                 {gscip_and_vars.variables[1], 1.0},
                                 {gscip_and_vars.variables[2], 1.0}});
}

// max x + y
// SOS1(x, y)
// x <= 2
// y <= 3
//
// (x*, y*) = (0, 3), objective value 3.0.
TEST(GScipFromMPModelProtoTest, Sos1Constraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    maximize: true
    variable { upper_bound: 2 name: "x" objective_coefficient: 1.0 }
    variable { upper_bound: 3 name: "y" objective_coefficient: 1.0 }
    general_constraint { sos_constraint { var_index: [ 0, 1 ] } }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  AssertOptimalWithBestSolution(
      result, 3.0,
      {{gscip_and_vars.variables[0], 0.0}, {gscip_and_vars.variables[1], 3.0}});
}

// max x
// SOS1(x)
// x <= 2
//
// (x*) = (2), objective value 2.0.
TEST(GScipFromMPModelProtoTest, UselessSos1Constraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    maximize: true
    variable { upper_bound: 2 name: "x" objective_coefficient: 1.0 }
    general_constraint { sos_constraint { var_index: [ 0 ] } }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  AssertOptimalWithBestSolution(result, 2.0,
                                {{gscip_and_vars.variables[0], 2.0}});
}

// max w + x + y + z
// SOS2(w, x, y, z)
// w <= 20
// x <= 3
// y <= 5
// z <= 30
//
// (w*, x*, y*, z*) = (0, 0, 5, 30), objective value 35.0.
TEST(GScipFromMPModelProtoTest, Sos2Constraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    maximize: true
    variable { upper_bound: 20 name: "w" objective_coefficient: 1.0 }
    variable { upper_bound: 3 name: "x" objective_coefficient: 1.0 }
    variable { upper_bound: 5 name: "y" objective_coefficient: 1.0 }
    variable { upper_bound: 30 name: "z" objective_coefficient: 1.0 }
    general_constraint {
      sos_constraint {
        type: SOS2,
        var_index: [ 0, 1, 2, 3 ]
      }
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  AssertOptimalWithBestSolution(result, 35.0,
                                {{gscip_and_vars.variables[0], 0.0},
                                 {gscip_and_vars.variables[1], 0.0},
                                 {gscip_and_vars.variables[2], 5.0},
                                 {gscip_and_vars.variables[3], 30.0}});
}

// max x + y
// SOS2(x, y)
// x <= 3
// y <= 5
//
// (x*, y*) = (3, 5), objective value 8.0.
TEST(GScipFromMPModelProtoTest, UselessSos2Constraint) {
  const MPModelProto model = ParseTestProto(R"pb(
    maximize: true
    variable { upper_bound: 3 name: "x" objective_coefficient: 1.0 }
    variable { upper_bound: 5 name: "y" objective_coefficient: 1.0 }
    general_constraint {
      sos_constraint {
        type: SOS2,
        var_index: [ 0, 1 ]
      }
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  AssertOptimalWithBestSolution(
      result, 8.0,
      {{gscip_and_vars.variables[0], 3.0}, {gscip_and_vars.variables[1], 5.0}});
}

// min x^2
// x >= 3
//
// x* = 3, objective value 9.0.
TEST(GScipFromMPModelProtoTest, QuadraticObjectiveMin) {
  const MPModelProto model = ParseTestProto(R"pb(
    variable { lower_bound: 3 name: "x" }
    quadratic_objective { qvar1_index: 0 qvar2_index: 0 coefficient: 1.0 }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  // Note that the quadratic objective may create auxiliary variables.
  AssertOptimalWithPartialBestSolution(result, 9.0,
                                       {{gscip_and_vars.variables[0], 3.0}});
}

// max x^2
// -3<= x <=  1
//
// x* = -3, objective value 9.0.
TEST(GScipFromMPModelProtoTest, QuadraticObjectiveMax) {
  const MPModelProto model = ParseTestProto(R"pb(
    maximize: true
    variable { lower_bound: -3 upper_bound: 1 name: "x" }
    quadratic_objective { qvar1_index: 0 qvar2_index: 0 coefficient: 1.0 }
  )pb");
  ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                       GScipAndVariables::FromMPModelProto(model));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_and_vars.gscip->Solve());
  // Note that the quadratic objective may create auxiliary variables.
  AssertOptimalWithPartialBestSolution(result, 9.0,
                                       {{gscip_and_vars.variables[0], -3.0}});
}

// min |x - 0.5|
// s.t. x in {0, 1}
//
// MIP encoding
//
// min z
// s.t. z >= x - 0.5
//      z >= 0.5 - x
// x in {0, 1}
//
// This problem has an LP relaxation of 0, but an optimal solution of either
// x = 0 or x = 1 and objective value 0.5.
//
// This is a hint test. We disable everything in the MIP solver so it just runs
// branch and bound on the raw input. We show that with a node limit of 1,
// we find no feasible solution, but with a hint an addition, we have a feasible
// solution.
TEST(GScipFromMPModelProtoTest, AddHint) {
  const MPModelProto model = ParseTestProto(R"pb(
    variable { lower_bound: 0 upper_bound: 1 is_integer: true name: "x" }
    variable { name: "z" objective_coefficient: 1.0 }
    constraint {
      lower_bound: -0.5
      var_index: [ 0, 1 ]
      coefficient: [ -1.0, 1.0 ]
    }
    constraint {
      lower_bound: 0.5
      var_index: [ 0, 1 ]
      coefficient: [ 1.0, 1.0 ]
    }
  )pb");
  GScipParameters parameters;
  parameters.set_heuristics(GScipParameters::OFF);
  parameters.set_presolve(GScipParameters::OFF);
  parameters.set_separating(GScipParameters::OFF);
  (*parameters.mutable_long_params())["limits/totalnodes"] = 1;

  // The root LP relaxation will be zero and not integral.
  {
    ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                         GScipAndVariables::FromMPModelProto(model));
    ASSERT_OK_AND_ASSIGN(const GScipResult result,
                         gscip_and_vars.gscip->Solve(parameters));
    ASSERT_EQ(result.gscip_output.status(), GScipOutput::TOTAL_NODE_LIMIT);
    EXPECT_EQ(result.gscip_output.stats().best_bound(), 0.0);
    EXPECT_EQ(result.gscip_output.stats().node_count(), 1);
    EXPECT_THAT(result.solutions, IsEmpty());
  }
  // Now add a hint, and we will find a feasible solution
  {
    const PartialVariableAssignment hint =
        ParseTestProto(R"pb(var_index: [ 0, 1 ]
                            var_value: [ 1.0, 0.5 ])pb");
    ASSERT_OK_AND_ASSIGN(auto gscip_and_vars,
                         GScipAndVariables::FromMPModelProto(model));
    ASSERT_OK(gscip_and_vars.AddHint(hint));
    ASSERT_OK_AND_ASSIGN(const GScipResult result,
                         gscip_and_vars.gscip->Solve(parameters));
    ASSERT_EQ(result.gscip_output.status(), GScipOutput::TOTAL_NODE_LIMIT);
    EXPECT_NEAR(result.gscip_output.stats().best_bound(), 0.0, 1e-5);
    EXPECT_NEAR(result.gscip_output.stats().best_objective(), 0.5, 1e-5);
    EXPECT_EQ(result.gscip_output.stats().node_count(), 1);
    ASSERT_EQ(result.solutions.size(), 1);
    EXPECT_THAT(result.solutions[0],
                GScipSolutionAlmostEquals({{gscip_and_vars.variables[0], 1.0},
                                           {gscip_and_vars.variables[1], 0.5}},
                                          1e-5));
  }
}

}  // namespace
}  // namespace operations_research
