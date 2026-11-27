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

#include "ortools/linear_solver/proto_solver/sat_solver_utils.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/mip_model.pb.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::EqualsProto;

TEST(ApplyMipPresolveStepsTest, SkippedWithNearIntegerContinuousVariable) {
  const sat::MipModelProto model = ParseTestProto(R"pb(
    variables { lower_bound: 1 upper_bound: 1 is_integer: true }
    variables {
      lower_bound: 0
      upper_bound: 10
      objective_coefficient: 1
      maximum_distance_to_integer: 0.25
    }
    constraints {
      vars: [ 0, 1 ]
      coeffs: [ 1, 1 ]
      lower_bound: 2
    }
  )pb");
  sat::MPModelProtoWrapper wrapper =
      sat::MPModelProtoWrapper::FromMipModelProto(model);
  const glop::GlopParameters glop_params;
  std::vector<std::unique_ptr<glop::Preprocessor>> for_postsolve;
  int num_presolved_constraints = -1;
  SolverLogger logger;
  EXPECT_EQ(ApplyMipPresolveSteps(glop_params, &wrapper, &for_postsolve,
                                  &num_presolved_constraints, &logger),
            glop::ProblemStatus::INIT);
  EXPECT_THAT(wrapper.proto(), EqualsProto(model));
  EXPECT_TRUE(for_postsolve.empty());
  EXPECT_EQ(num_presolved_constraints, 1);
}

TEST(ApplyMipPresolveStepsTest, SubstitutesChainedObjectiveDefinitions) {
  // Minimize obj, with obj = z1 + z2, z1 = 2 * s1 + 3 * s2 and
  // z2 = 4 * s1 + 5 * s2. Substituting obj makes its defining row free, but
  // that row still contains z1 and z2, which only become singleton columns once
  // it is removed. Each s_i appears in two other constraints, so that it is not
  // a singleton column itself.
  const sat::MipModelProto model = ParseTestProto(R"pb(
    variables { objective_coefficient: 1 }                        # obj
    variables { lower_bound: 0 }                                  # z1
    variables { lower_bound: 0 }                                  # z2
    variables { lower_bound: 0 }                                  # s1
    variables { lower_bound: 0 }                                  # s2
    variables { lower_bound: 0 upper_bound: 1 is_integer: true }  # x
    variables { lower_bound: 0 upper_bound: 1 is_integer: true }  # y
    constraints {
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, -1, -1 ]
      lower_bound: 0
      upper_bound: 0
    }
    constraints {
      vars: [ 1, 3, 4 ]
      coeffs: [ 1, -2, -3 ]
      lower_bound: 0
      upper_bound: 0
    }
    constraints {
      vars: [ 2, 3, 4 ]
      coeffs: [ 1, -4, -5 ]
      lower_bound: 0
      upper_bound: 0
    }
    constraints {
      vars: [ 3, 5 ]
      coeffs: [ 1, 1 ]
      lower_bound: 1
    }
    constraints {
      vars: [ 3, 6 ]
      coeffs: [ 1, 1 ]
      lower_bound: 1
    }
    constraints {
      vars: [ 4, 5 ]
      coeffs: [ 1, -1 ]
      lower_bound: 0
    }
    constraints {
      vars: [ 4, 6 ]
      coeffs: [ 1, -1 ]
      lower_bound: 0
    }
  )pb");
  sat::MPModelProtoWrapper wrapper =
      sat::MPModelProtoWrapper::FromMipModelProto(model);
  const glop::GlopParameters glop_params;
  std::vector<std::unique_ptr<glop::Preprocessor>> for_postsolve;
  int num_presolved_constraints = -1;
  SolverLogger logger;

  EXPECT_EQ(ApplyMipPresolveSteps(glop_params, &wrapper, &for_postsolve,
                                  &num_presolved_constraints, &logger),
            glop::ProblemStatus::INIT);

  // obj, z1 and z2 are substituted, so the objective is now on s1 and s2.
  ASSERT_EQ(wrapper.num_variables(), 4);
  EXPECT_EQ(wrapper.variables(0).objective_coefficient(), 6.0);
  EXPECT_EQ(wrapper.variables(1).objective_coefficient(), 8.0);
  EXPECT_EQ(wrapper.variables(2).objective_coefficient(), 0.0);
  EXPECT_EQ(wrapper.variables(3).objective_coefficient(), 0.0);
}

}  // namespace
}  // namespace operations_research
